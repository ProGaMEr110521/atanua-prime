/*
.atnua file-association helpers for Atanua Prime (Windows, per-user).

Pure, window-system independent logic behind the one-click "associate
.atanua files with this app" Settings toggle. The registry writes live
behind AtanuaConfig::mFileAssoc + an explicit toggle in the settings
panel; unit tests drive these without touching the registry.

Layout (HKCU only, no admin, no surprise writes):
  Software\Classes\AtanuaPrime.Design\shell\open\command  = "<exe>" "%1"
  Software\Classes\AtanuaPrime.Design\DefaultIcon         = "<exe>",1
  Software\Classes\.atanua\(Default)                      = "AtanuaPrime.Design"
(Index 1 is the document icon; index 0 is the application icon, both
embedded from atanua-app.ico / atanua-doc.ico via atanua.rc.)

Rules:
- All registry access goes through the narrow read/write/remove helpers
  declared in nativefunctions (implemented with Win32 Reg* APIs), so this
  header stays portable and testable.
- Paths are never trusted blindly: the exe path must be non-empty and
  the command line is quoted before "%1" is appended.
- Association state is a single tri-state: 1 = fully ours (both progid
  verbs point at this exe), 0 = missing/foreign, -1 = registry error.
*/
#ifndef FILEASSOC_H
#define FILEASSOC_H

#include <stddef.h>
#include <string.h>

namespace FileAssoc {

// ProgID owned by this fork. Stable across installs of the same binary.
inline const char *progId()
{
    return "AtanuaPrime.Design";
}

// Extension key, with leading dot, as stored under HKCU\Software\Classes.
inline const char *extensionKey()
{
    return ".atanua";
}

// Open-verb subkey under the ProgID.
inline const char *openCommandSubkey()
{
    return "shell\\open\\command";
}

// DefaultIcon subkey under the ProgID.
inline const char *defaultIconSubkey()
{
    return "DefaultIcon";
}

// Build the open-verb command: "<exe>" "%1". Returns false when the exe
// path is empty or the buffer is too small; never writes a partial line.
inline bool formatOpenCommand(const char *exePath, char *out, int cap)
{
    if (!exePath || !exePath[0] || !out || cap <= 0)
        return false;
    // Exact bytes needed: '"' + exe + '"' + ' ' + '"%1"' + NUL.
    size_t need = strlen(exePath) + 7;
    if (need + 1 > (size_t)cap)
        return false;
    int i = 0;
    out[i++] = '"';
    for (const char *p = exePath; *p; p++)
        out[i++] = *p;
    out[i++] = '"';
    out[i++] = ' ';
    out[i++] = '"';
    out[i++] = '%';
    out[i++] = '1';
    out[i++] = '"';
    out[i] = 0;
    return true;
}

// Build the DefaultIcon value: "<exe>",1. Same guards as above.
inline bool formatDefaultIcon(const char *exePath, char *out, int cap)
{
    if (!exePath || !exePath[0] || !out || cap <= 0)
        return false;
    size_t need = strlen(exePath) + 5;
    if (need + 1 > (size_t)cap)
        return false;
    int i = 0;
    out[i++] = '"';
    for (const char *p = exePath; *p; p++)
        out[i++] = *p;
    out[i++] = '"';
    out[i++] = ',';
    out[i++] = '1';
    out[i] = 0;
    return true;
}

// Compare a stored open-command against this exe: true when the stored
// value launches exactly this binary (quoted or bare) with "%1".
// NULL-safe; never reads past either string.
inline bool openCommandMatchesExe(const char *stored, const char *exePath)
{
    if (!stored || !stored[0] || !exePath || !exePath[0])
        return false;
    // Skip a single leading quote on each side. Copy the exe into a
    // local buffer first so trailing-quote trim needs no mutation.
    if (stored[0] == '"')
        stored++;
    char exe[1024];
    size_t k = 0;
    for (const char *p = exePath; *p && k + 1 < sizeof(exe); p++)
        exe[k++] = *p;
    exe[k] = 0;
    size_t exeLen = k;
    if (exeLen > 0 && exe[0] == '"')
    {
        memmove(exe, exe + 1, exeLen);
        exeLen--;
    }
    if (exeLen > 0 && exe[exeLen - 1] == '"')
        exeLen--;
    exe[exeLen] = 0;
    exePath = exe;
    size_t i = 0;
    while (i < exeLen && stored[i] && stored[i] != '"')
    {
        char a = stored[i];
        char b = exePath[i];
        if (a >= 'A' && a <= 'Z')
            a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z')
            b = (char)(b - 'A' + 'a');
        if (a != b)
            return false;
        i++;
    }
    if (i != exeLen)
        return false;
    // After the exe must come the closing quote, whitespace, then "%1"
    // (whose own opening quote is optional-tolerant).
    const char *tail = stored + i;
    if (*tail == '"')
        tail++;
    while (*tail == ' ' || *tail == '\t')
        tail++;
    if (*tail == '"')
        tail++;
    return tail[0] == '%' && tail[1] == '1';
}

} // namespace FileAssoc

#endif
