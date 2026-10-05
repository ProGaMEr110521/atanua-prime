/*
Atanua what's-new helpers - pure changelog logic behind the one-time
post-update popup. No I/O here, so unit tests can drive section
extraction and show-once gating; file search and rendering live in
main.cpp.
*/
#ifndef APP_CHANGELOG_H
#define APP_CHANGELOG_H

#include <string.h>

namespace AppChangelog {

/* Keep [A-Za-z0-9._-] so stored versions are safe in XML attributes. */
inline void sanitizeVersion(const char *src, char *dst, int cap)
{
    int i = 0;
    if (!dst || cap <= 0)
        return;
    if (src)
    {
        while (*src && i < cap - 1)
        {
            char c = *src++;
            int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
            if (ok)
                dst[i++] = c;
        }
    }
    dst[i] = '\0';
}

/* "v1.3.141236" or "1.3.141236" into triple ints. Returns 1. */
inline int parseVer(const char *s, int out[3])
{
    char buf[64];
    const char *q;
    long parts[3];
    int i;
    if (!s || !out)
        return 0;
    sanitizeVersion(s, buf, (int)sizeof(buf));
    q = buf;
    if (*q == 'v' || *q == 'V')
        q++;
    for (i = 0; i < 3; i++)
    {
        long v;
        if (*q < '0' || *q > '9')
            return 0;
        v = 0;
        while (*q >= '0' && *q <= '9')
            v = v * 10 + (*q++ - '0');
        parts[i] = v;
        if (i < 2)
        {
            if (*q != '.')
                return 0;
            q++;
        }
    }
    if (*q != '\0')
        return 0;
    out[0] = (int)parts[0];
    out[1] = (int)parts[1];
    out[2] = (int)parts[2];
    return 1;
}

/* -1 when a < b, 0 when equal, 1 when a > b. */
inline int cmpVer(const int a[3], const int b[3])
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

/* 1 when line opens the section for version ("## [vX]" or "## [X]"). */
inline int isVersionHeader(const char *line, const char *version)
{
    size_t vlen;
    const char *p;
    if (!line || !version || !*version)
        return 0;
    if (strncmp(line, "## [", 4) != 0)
        return 0;
    p = line + 4;
    if (*p == 'v' || *p == 'V')
        p++;
    vlen = strlen(version);
    if (vlen > 0 && (version[0] == 'v' || version[0] == 'V'))
    {
        version++;
        vlen--;
    }
    if (vlen == 0 || strncmp(p, version, vlen) != 0)
        return 0;
    return p[vlen] == ']' ? 1 : 0;
}

inline int isSectionHeader(const char *line)
{
    return (line && strncmp(line, "## ", 3) == 0) ? 1 : 0;
}

/* Copy the section body for version out of a CHANGELOG.md blob.
 * Skips the header line itself, stops at the next "## " header,
 * trims leading/trailing blank lines. Returns 1 when the section was
 * found (even if its body is empty), 0 otherwise. Never overflows. */
inline int sectionFor(const char *md, const char *version, char *out, int cap)
{
    const char *p;
    const char *bodyStart = 0;
    const char *bodyEnd = 0;
    if (!md || !version || !out || cap <= 0)
        return 0;
    out[0] = '\0';
    p = md;
    while (*p)
    {
        const char *lineEnd = strchr(p, '\n');
        size_t len = lineEnd ? (size_t)(lineEnd - p) : strlen(p);
        char line[256];
        size_t copy = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
        memcpy(line, p, copy);
        line[copy] = '\0';
        while (copy > 0 && (line[copy - 1] == '\r' || line[copy - 1] == ' ' || line[copy - 1] == '\t'))
            line[--copy] = '\0';
        if (!bodyStart)
        {
            if (isVersionHeader(line, version))
                bodyStart = lineEnd ? lineEnd + 1 : p + len;
        }
        else if (isSectionHeader(line))
        {
            bodyEnd = p;
            break;
        }
        if (!lineEnd)
            break;
        p = lineEnd + 1;
    }
    if (!bodyStart)
        return 0;
    if (!bodyEnd)
        bodyEnd = md + strlen(md);
    while (bodyStart < bodyEnd && (*bodyStart == '\n' || *bodyStart == '\r'))
        bodyStart++;
    while (bodyEnd > bodyStart && (bodyEnd[-1] == '\n' || bodyEnd[-1] == '\r' ||
        bodyEnd[-1] == ' ' || bodyEnd[-1] == '\t'))
        bodyEnd--;
    {
        size_t n = (size_t)(bodyEnd - bodyStart);
        if (n > (size_t)(cap - 1))
            n = (size_t)(cap - 1);
        memcpy(out, bodyStart, n);
        out[n] = '\0';
    }
    return 1;
}

/* 1 when the popup should appear once: a remembered version exists and
 * the running build compares newer. Fresh installs (empty seen) stay
 * quiet and just record; unparseable pairs fall back to string compare
 * so dev builds still surface exactly once per change. */
inline int shouldShow(const char *seen, const char *current)
{
    int sVer[3];
    int cVer[3];
    if (!seen || !*seen || !current || !*current)
        return 0;
    if (strcmp(seen, current) == 0)
        return 0;
    if (parseVer(seen, sVer) && parseVer(current, cVer))
        return cmpVer(sVer, cVer) < 0 ? 1 : 0;
    return 1;
}

/* Append src to out without overflowing; always NUL-terminates. */
inline void appendTrunc(char *out, int cap, int *len, const char *src, int srcLen)
{
    int room;
    if (!out || cap <= 0 || !len || !src || srcLen <= 0)
        return;
    room = cap - 1 - *len;
    if (room <= 0)
        return;
    if (srcLen > room)
        srcLen = room;
    memcpy(out + *len, src, (size_t)srcLen);
    *len += srcLen;
    out[*len] = '\0';
}

/* Collect the bodies for every tagged version in (seen, current],
 * newest first, each prefixed with its "vX" line. Returns 1 when at
 * least one section matched. Powers the once-popup so multi-release
 * jumps show everything new; the Help reopen uses sectionFor for the
 * single running build instead. */
inline int rangeFor(const char *md, const char *seen, const char *current,
    char *out, int cap)
{
    int sVer[3];
    int cVer[3];
    int sOk;
    int matched = 0;
    int len = 0;
    const char *p;
    if (!md || !out || cap <= 0)
        return 0;
    out[0] = '\0';
    if (!parseVer(current, cVer))
        return 0;
    sOk = (seen && *seen) ? parseVer(seen, sVer) : 0;
    p = md;
    while (*p)
    {
        const char *lineEnd = strchr(p, '\n');
        size_t llen = lineEnd ? (size_t)(lineEnd - p) : strlen(p);
        char line[256];
        size_t copy = llen < sizeof(line) - 1 ? llen : sizeof(line) - 1;
        memcpy(line, p, copy);
        line[copy] = '\0';
        while (copy > 0 && (line[copy - 1] == '\r' || line[copy - 1] == ' ' || line[copy - 1] == '\t'))
            line[--copy] = '\0';
        if (isSectionHeader(line) && strncmp(line, "## [", 4) == 0)
        {
            const char *close = strchr(line + 4, ']');
            if (close && close > line + 4)
            {
                char tag[64];
                int vVer[3];
                size_t tlen = (size_t)(close - (line + 4));
                if (tlen > sizeof(tag) - 1)
                    tlen = sizeof(tag) - 1;
                memcpy(tag, line + 4, tlen);
                tag[tlen] = '\0';
                if (parseVer(tag, vVer)
                    && cmpVer(vVer, cVer) <= 0
                    && (!sOk || cmpVer(sVer, vVer) < 0))
                {
                    char body[8192];
                    if (sectionFor(md, tag, body, (int)sizeof(body)) && body[0])
                    {
                        char head[80];
                        int hlen = 0;
                        const char *tagNoV = (tag[0] == 'v' || tag[0] == 'V') ? tag + 1 : tag;
                        head[hlen++] = 'v';
                        while (*tagNoV && hlen < (int)sizeof(head) - 2)
                            head[hlen++] = *tagNoV++;
                        head[hlen++] = '\n';
                        if (matched)
                            appendTrunc(out, cap, &len, "\n", 1);
                        appendTrunc(out, cap, &len, head, hlen);
                        appendTrunc(out, cap, &len, body, (int)strlen(body));
                        appendTrunc(out, cap, &len, "\n", 1);
                        matched = 1;
                    }
                }
            }
        }
        if (!lineEnd)
            break;
        p = lineEnd + 1;
    }
    return matched;
}

} // namespace AppChangelog

#endif
