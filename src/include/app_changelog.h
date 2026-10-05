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
 * the running build parses newer. Fresh installs (empty seen) stay
 * quiet and just record; unparseable pairs fall back to string compare
 * so dev builds still surface exactly once per change. */
inline int shouldShow(const char *seen, const char *current)
{
    int sVer[3];
    int cVer[3];
    int sOk;
    int cOk;
    if (!seen || !*seen || !current || !*current)
        return 0;
    if (strcmp(seen, current) == 0)
        return 0;
    {
        const char *s = seen;
        const char *c = current;
        char sBuf[64];
        char cBuf[64];
        sanitizeVersion(s, sBuf, (int)sizeof(sBuf));
        sanitizeVersion(c, cBuf, (int)sizeof(cBuf));
        sOk = 0;
        cOk = 0;
        /* Reuse the updater's version grammar without including its
         * network header: v-prefixed or bare triple-dotted ints. */
        {
            const char *q;
            long parts[3];
            int i;
            q = sBuf;
            if (*q == 'v' || *q == 'V')
                q++;
            sOk = 1;
            for (i = 0; i < 3; i++)
            {
                char *end = 0;
                long v;
                if (*q < '0' || *q > '9')
                {
                    sOk = 0;
                    break;
                }
                v = 0;
                while (*q >= '0' && *q <= '9')
                    v = v * 10 + (*q++ - '0');
                parts[i] = v;
                if (i < 2)
                {
                    if (*q != '.')
                    {
                        sOk = 0;
                        break;
                    }
                    q++;
                }
            }
            if (sOk && *q != '\0')
                sOk = 0;
            if (sOk)
            {
                sVer[0] = (int)parts[0];
                sVer[1] = (int)parts[1];
                sVer[2] = (int)parts[2];
            }
            q = cBuf;
            if (*q == 'v' || *q == 'V')
                q++;
            cOk = 1;
            for (i = 0; i < 3; i++)
            {
                long v;
                if (*q < '0' || *q > '9')
                {
                    cOk = 0;
                    break;
                }
                v = 0;
                while (*q >= '0' && *q <= '9')
                    v = v * 10 + (*q++ - '0');
                parts[i] = v;
                if (i < 2)
                {
                    if (*q != '.')
                    {
                        cOk = 0;
                        break;
                    }
                    q++;
                }
            }
            if (cOk && *q != '\0')
                cOk = 0;
            if (cOk)
            {
                cVer[0] = (int)parts[0];
                cVer[1] = (int)parts[1];
                cVer[2] = (int)parts[2];
            }
        }
        if (sOk && cOk)
        {
            int i;
            for (i = 0; i < 3; i++)
            {
                if (cVer[i] > sVer[i])
                    return 1;
                if (cVer[i] < sVer[i])
                    return 0;
            }
            return 0;
        }
    }
    return 1;
}

} // namespace AppChangelog

#endif
