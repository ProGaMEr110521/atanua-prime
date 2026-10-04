/*
Atanua quick-find palette - pure helpers behind the double-shift
component search. No I/O and no UI toolkit here, so unit tests can drive
the shipped matching and tap-timing logic; rendering lives in main.cpp.
*/
#ifndef QUICK_FIND_H
#define QUICK_FIND_H

namespace QuickFind {

/* Double-tap window for the shortcut key, in milliseconds. */
inline unsigned tapWindowMs()
{
    return 400;
}

inline int asciiTolower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

/* Case-insensitive substring: empty filter matches everything, null
 * names never match. Mirrors the sidebar filter exactly. */
inline int substringMatch(const char *name, const char *filter)
{
    if (!filter || !filter[0])
        return 1;
    if (!name)
        return 0;
    for (const char *p = name; *p; p++)
    {
        const char *a = p;
        const char *b = filter;
        while (*a && *b && asciiTolower(*a) == asciiTolower(*b))
        {
            a++;
            b++;
        }
        if (!*b)
            return 1;
    }
    return 0;
}

/* Double-press detector for the shortcut key. First press arms, a second
 * press inside the window fires and disarms (so a triple press reads as
 * open, arm, open rather than one stuck state). Unsigned math keeps
 * tick-counter wrap-around correct. Returns 1 on fire, 0 otherwise. */
inline int doubleTapTick(unsigned *lastTick, unsigned nowTick, unsigned windowMs)
{
    if (!lastTick)
        return 0;
    int fire = (*lastTick != 0 && nowTick - *lastTick <= windowMs) ? 1 : 0;
    *lastTick = fire ? 0 : nowTick;
    return fire;
}

} // namespace QuickFind

#endif
