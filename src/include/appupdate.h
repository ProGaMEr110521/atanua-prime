/*
Atanua update check - pure dependency-free logic.
Version parsing/comparison plus newest-release selection from a GitHub
releases API payload. No platform or UI dependencies so it stays unit
testable; fetching and prompting live elsewhere.
*/
#ifndef APPUPDATE_H
#define APPUPDATE_H

#include <string.h>
#include <stdlib.h>

#define APPUPDATE_OWNER "ProGaMEr110521"
#define APPUPDATE_REPO "atanua-prime"
#define APPUPDATE_API_URL "https://api.github.com/repos/ProGaMEr110521/atanua-prime/releases?per_page=10"
#define APPUPDATE_MAX_TAG 63
#define APPUPDATE_MAX_URL 255

typedef struct AppUpdateRelease
{
    char tag[64];
    int draft;
    int prerelease;
    char htmlUrl[256];
    char windowsUrl[256];
    char linuxUrl[256];
} AppUpdateRelease;

static void AppUpdate_CopyStr(char *dst, int cap, const char *src, int len)
{
    if (!dst || cap <= 0)
        return;
    if (!src || len < 0)
        len = 0;
    if (len > cap - 1)
        len = cap - 1;
    memcpy(dst, src, (size_t)len);
    dst[len] = '\0';
}

/* "v1.3.141223" or "1.3.141223" -> {1,3,141223}. Returns 1 on success. */
static int AppUpdate_ParseVersion(const char *s, int outVer[3])
{
    long parts[3];
    int i;
    if (!s || !outVer)
        return 0;
    if (*s == 'v' || *s == 'V')
        s++;
    for (i = 0; i < 3; i++)
    {
        char *end = NULL;
        if (*s < '0' || *s > '9')
            return 0;
        parts[i] = strtol(s, &end, 10);
        if (end == s || parts[i] < 0 || parts[i] > 1000000000L)
            return 0;
        s = end;
        if (i < 2)
        {
            if (*s != '.')
                return 0;
            s++;
        }
    }
    if (*s != '\0')
        return 0;
    outVer[0] = (int)parts[0];
    outVer[1] = (int)parts[1];
    outVer[2] = (int)parts[2];
    return 1;
}

/* -1 when a < b, 0 when equal, 1 when a > b. */
static int AppUpdate_CompareVersions(const int a[3], const int b[3])
{
    int i;
    for (i = 0; i < 3; i++)
    {
        if (a[i] < b[i])
            return -1;
        if (a[i] > b[i])
            return 1;
    }
    return 0;
}

/* Pointer to the closing quote of the string opening at p, or NULL. */
static const char *AppUpdate_QuotedEnd(const char *p, const char *end)
{
    const char *q;
    if (!p || p >= end || *p != '"')
        return NULL;
    q = p + 1;
    while (q < end)
    {
        if (*q == '\\' && q + 1 < end)
        {
            q += 2;
            continue;
        }
        if (*q == '"')
            return q;
        q++;
    }
    return NULL;
}

/* Finds "key" (without quotes) inside [start, end). */
static const char *AppUpdate_FindKey(const char *start, const char *end, const char *key)
{
    size_t klen;
    const char *p;
    if (!start || !end || !key || start >= end)
        return NULL;
    klen = strlen(key);
    p = start;
    while (p + 1 + klen < end)
    {
        if (*p == '"' && strncmp(p + 1, key, klen) == 0 && p[1 + klen] == '"')
            return p;
        p++;
    }
    return NULL;
}

static const char *AppUpdate_SkipSpaces(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        p++;
    return p;
}

/* Reads a JSON string value after a key position. Returns 1 on success. */
static int AppUpdate_ReadStringAfter(const char *keyPos, const char *end,
    char *out, int outCap)
{
    const char *p, *close;
    if (!keyPos)
        return 0;
    p = strchr(keyPos, ':');
    if (!p || p >= end)
        return 0;
    p = AppUpdate_SkipSpaces(p + 1, end);
    if (p >= end || *p != '"')
        return 0;
    close = AppUpdate_QuotedEnd(p, end);
    if (!close)
        return 0;
    AppUpdate_CopyStr(out, outCap, p + 1, (int)(close - (p + 1)));
    return 1;
}

/* Reads a JSON true/false value after a key position. Returns 1 on success. */
static int AppUpdate_ReadBoolAfter(const char *keyPos, const char *end, int *out)
{
    const char *p;
    if (!keyPos || !out)
        return 0;
    p = strchr(keyPos, ':');
    if (!p || p >= end)
        return 0;
    p = AppUpdate_SkipSpaces(p + 1, end);
    if (p + 4 <= end && strncmp(p, "true", 4) == 0)
    {
        *out = 1;
        return 1;
    }
    if (p + 5 <= end && strncmp(p, "false", 5) == 0)
    {
        *out = 0;
        return 1;
    }
    return 0;
}

static int AppUpdate_ContainsWord(const char *text, const char *word)
{
    size_t wlen, t;
    size_t n;
    if (!text || !word)
        return 0;
    wlen = strlen(word);
    if (wlen == 0)
        return 0;
    n = strlen(text);
    for (t = 0; t + wlen <= n; t++)
    {
        size_t k;
        int hit = 1;
        for (k = 0; k < wlen; k++)
        {
            char a = text[t + k];
            char b = word[k];
            if (a >= 'A' && a <= 'Z')
                a = (char)(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z')
                b = (char)(b + ('a' - 'A'));
            if (a != b)
            {
                hit = 0;
                break;
            }
        }
        if (hit)
            return 1;
    }
    return 0;
}

/* Parses one top-level release object span. Returns 1 when usable. */
static int AppUpdate_ParseOne(const char *start, const char *end, AppUpdateRelease *out)
{
    const char *key;
    const char *authorAt;
    const char *htmlScopeEnd;
    const char *assetsAt;
    const char *assetsEnd;
    const char *pos;
    memset(out, 0, sizeof(*out));
    key = AppUpdate_FindKey(start, end, "tag_name");
    if (!AppUpdate_ReadStringAfter(key, end, out->tag, (int)sizeof(out->tag)))
        return 0;
    if (out->tag[0] == '\0')
        return 0;
    key = AppUpdate_FindKey(start, end, "draft");
    AppUpdate_ReadBoolAfter(key, end, &out->draft);
    key = AppUpdate_FindKey(start, end, "prerelease");
    AppUpdate_ReadBoolAfter(key, end, &out->prerelease);
    /* The release page URL precedes the nested author object; prefer that
       span so an author's html_url can never be mistaken for it. */
    authorAt = AppUpdate_FindKey(start, end, "author");
    htmlScopeEnd = authorAt ? authorAt : end;
    {
        const char *h = NULL;
        const char *scan = start;
        while ((scan = AppUpdate_FindKey(scan, htmlScopeEnd, "html_url")) != NULL)
        {
            h = scan;
            scan++;
            if (scan >= htmlScopeEnd)
                break;
        }
        if (h)
            AppUpdate_ReadStringAfter(h, end, out->htmlUrl, (int)sizeof(out->htmlUrl));
    }
    assetsAt = AppUpdate_FindKey(start, end, "assets");
    if (assetsAt)
    {
        const char *arr = strchr(assetsAt, '[');
        if (arr && arr < end)
        {
            int depth = 0;
            const char *p = arr;
            assetsEnd = end;
            while (p < end)
            {
                if (*p == '[')
                    depth++;
                else if (*p == ']')
                {
                    depth--;
                    if (depth <= 0)
                    {
                        assetsEnd = p;
                        break;
                    }
                }
                p++;
            }
            pos = arr;
            while ((pos = AppUpdate_FindKey(pos, assetsEnd, "name")) != NULL)
            {
                char name[128];
                const char *nextName;
                const char *urlKey;
                const char *urlEnd;
                char url[256];
                if (!AppUpdate_ReadStringAfter(pos, assetsEnd, name, (int)sizeof(name)))
                    break;
                nextName = AppUpdate_FindKey(pos + 1, assetsEnd, "name");
                if (nextName == NULL)
                    nextName = assetsEnd;
                urlKey = AppUpdate_FindKey(pos + 1, nextName, "browser_download_url");
                urlEnd = nextName;
                if (urlKey && AppUpdate_ReadStringAfter(urlKey, urlEnd, url, (int)sizeof(url)))
                {
                    if (!out->windowsUrl[0] && AppUpdate_ContainsWord(name, "windows"))
                        AppUpdate_CopyStr(out->windowsUrl, (int)sizeof(out->windowsUrl), url, (int)strlen(url));
                    else if (!out->linuxUrl[0] && AppUpdate_ContainsWord(name, "linux"))
                        AppUpdate_CopyStr(out->linuxUrl, (int)sizeof(out->linuxUrl), url, (int)strlen(url));
                }
                pos = nextName;
                if (pos >= assetsEnd)
                    break;
            }
        }
    }
    return 1;
}

/* Scans a releases array payload into out[] (cap entries).
   Returns the stored release count, or -1 on malformed input. */
static int AppUpdate_ParseReleases(const char *json, AppUpdateRelease *out, int cap)
{
    const char *arr;
    const char *end;
    const char *p;
    const char *objStart;
    int count;
    int depth;
    int inStr;
    int esc;
    if (!json || !out || cap <= 0)
        return -1;
    arr = strchr(json, '[');
    if (!arr)
        return -1;
    end = json + strlen(json);
    count = 0;
    depth = 0;
    inStr = 0;
    esc = 0;
    objStart = NULL;
    for (p = arr + 1; p < end; p++)
    {
        char c = *p;
        if (inStr)
        {
            if (esc)
                esc = 0;
            else if (c == '\\')
                esc = 1;
            else if (c == '"')
                inStr = 0;
            continue;
        }
        if (c == '"')
        {
            inStr = 1;
            continue;
        }
        if (c == '{')
        {
            if (depth == 0)
                objStart = p;
            depth++;
            continue;
        }
        if (c == '}')
        {
            depth--;
            if (depth < 0)
                return -1;
            if (depth == 0 && objStart)
            {
                if (count < cap && (p + 1 - objStart) < 65536)
                {
                    if (AppUpdate_ParseOne(objStart, p + 1, &out[count]))
                        count++;
                }
                objStart = NULL;
            }
            continue;
        }
        if (c == ']' && depth == 0)
            break;
    }
    return count;
}

/* Picks the newest non-draft release strictly newer than current.
   Prereleases count: the newest published tag wins either way.
   Returns 1 and the index, or 0 when up to date. */
static int AppUpdate_SelectNewest(const AppUpdateRelease *rs, int n,
    const int current[3], int *picked)
{
    int i;
    int best;
    int bestVer[3];
    int have;
    if (!rs || n <= 0 || !current || !picked)
        return 0;
    best = -1;
    have = 0;
    for (i = 0; i < n; i++)
    {
        int ver[3];
        if (rs[i].draft)
            continue;
        if (!AppUpdate_ParseVersion(rs[i].tag, ver))
            continue;
        if (AppUpdate_CompareVersions(ver, current) <= 0)
            continue;
        if (!have || AppUpdate_CompareVersions(ver, bestVer) > 0)
        {
            best = i;
            bestVer[0] = ver[0];
            bestVer[1] = ver[1];
            bestVer[2] = ver[2];
            have = 1;
        }
    }
    if (!have)
        return 0;
    *picked = best;
    return 1;
}

#endif
