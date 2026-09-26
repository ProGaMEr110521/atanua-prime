/*
Drop/argv file-open helpers for Atanua Prime.

Pure, window-system independent logic behind opening .atanua files via
double-click (argv[1]), drag-and-drop (SDL_DROPFILE) and the file
association. Unit tests drive these without opening a window; main.cpp
wires them to SDL events and do_loaddialog().

Rules (mirror plan.md acceptance criteria):
- Only ".atanua" (case-insensitive) is accepted; anything else is ignored.
- NULL and empty paths are rejected, never dereferenced.
- The UI layer (not here) prompts on a dirty canvas before discarding work
  and frees SDL drop paths with SDL_free.
*/
#ifndef DROPFILE_H
#define DROPFILE_H

#include <stddef.h>
#include <string.h>

namespace DropFile {

// Canonical extension, lowercase, with leading dot.
inline const char *extension()
{
    return ".atanua";
}

// Case-insensitive ".atanua" suffix check. NULL-safe.
inline bool hasAtanuaExtension(const char *path)
{
    if (!path || !path[0])
        return false;
    size_t len = strlen(path);
    // Need at least one name char plus the 7-char extension.
    if (len < 8)
        return false;
    const char *ext = path + len - 7;
    if (ext[0] != '.')
        return false;
    static const char kWant[6] = { 'a', 't', 'a', 'n', 'u', 'a' };
    for (int i = 0; i < 6; i++)
    {
        char c = ext[1 + i];
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        if (c != kWant[i])
            return false;
    }
    return true;
}

// Whether a dropped/command-line path should be opened: non-empty and
// carrying the .atanua extension.
inline bool shouldAcceptDrop(const char *path)
{
    if (!path || !path[0])
        return false;
    return hasAtanuaExtension(path);
}

//Basename for UI prompts ("C:\dir\foo.atanua" -> "foo.atanua").
//Never returns NULL; returns the input tail or "" for NULL input.
inline const char *baseName(const char *path)
{
    if (!path || !path[0])
        return "";
    const char *tail = path;
    for (const char *p = path; *p; p++)
    {
        if (*p == '\\' || *p == '/')
            tail = p + 1;
    }
    return tail;
}

} // namespace DropFile

#endif
