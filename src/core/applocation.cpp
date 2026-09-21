/*
Atanua Real-Time Logic Simulator
Copyright (c) 2008-2014 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/
#include "applocation.h"
#include "toolkit.h"

#include <stdio.h>
#include <string.h>

#ifdef LINUX_VERSION
#include <libgen.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef LINUX_VERSION
static int dirHasData(const char *dir)
{
    char probe[512];
    if (!dir || !*dir)
        return 0;
    snprintf(probe, sizeof(probe), "%s/data/icon.png", dir);
    return access(probe, R_OK) == 0;
}

static void copyDir(char *dst, int cap, const char *src)
{
    if (!dst || cap <= 0 || !src)
        return;
    strncpy(dst, src, (size_t)(cap - 1));
    dst[cap - 1] = '\0';
}

static int exeDirectory(char *out, int cap)
{
    char linkPath[PATH_MAX];
    ssize_t len;
    if (!out || cap <= 0)
        return 0;
    len = readlink("/proc/self/exe", linkPath, sizeof(linkPath) - 1);
    if (len <= 0)
        return 0;
    linkPath[len] = '\0';
    copyDir(out, cap, dirname(linkPath));
    return out[0] != '\0';
}
#endif

void AppLocation_Init(int argc, char **argv)
{
#ifdef LINUX_VERSION
    char exeDir[PATH_MAX];
    char prefixShare[PATH_MAX];
    char resolvedShare[PATH_MAX];
    const char *candidates[5];
    int count = 0;
    int i;

    if (!exeDirectory(exeDir, (int)sizeof(exeDir)) && argc > 0 && argv && argv[0])
    {
        char fallback[PATH_MAX];
        copyDir(fallback, (int)sizeof(fallback), argv[0]);
        char *slash = strrchr(fallback, '/');
        if (slash)
        {
            *slash = '\0';
            copyDir(exeDir, (int)sizeof(exeDir), fallback);
        }
    }

    if (exeDir[0])
        candidates[count++] = exeDir;
    if (exeDir[0])
    {
        snprintf(prefixShare, sizeof(prefixShare), "%s/../share/atanua", exeDir);
        if (realpath(prefixShare, resolvedShare))
            candidates[count++] = resolvedShare;
    }
    candidates[count++] = "/usr/share/atanua";
    candidates[count++] = "/usr/local/share/atanua";

    for (i = 0; i < count; i++)
    {
        if (dirHasData(candidates[i]))
        {
            chdir(candidates[i]);
            return;
        }
    }
    if (exeDir[0])
        chdir(exeDir);
#else
    (void)argc;
    (void)argv;
#endif
}

const char *AppLocation_ConfigFile(void)
{
#ifdef LINUX_VERSION
    static char path[512];
    static int ready = 0;
    if (!ready)
    {
        const char *xdg = getenv("XDG_CONFIG_HOME");
        if (xdg && xdg[0])
            snprintf(path, sizeof(path), "%s/atanua/atanua.xml", xdg);
        else
        {
            const char *home = getenv("HOME");
            if (!home || !home[0])
                home = ".";
            snprintf(path, sizeof(path), "%s/.config/atanua/atanua.xml", home);
        }
        ready = 1;
    }
    return path;
#else
    return "atanua.xml";
#endif
}

void AppLocation_EnsureConfigDir(void)
{
#ifdef LINUX_VERSION
    char dir[512];
    const char *file = AppLocation_ConfigFile();
    char *slash;
    if (!file)
        return;
    strncpy(dir, file, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    slash = strrchr(dir, '/');
    if (!slash)
        return;
    *slash = '\0';
    mkdir(dir, 0755);
#endif
}

int AppLocation_NextScreenshotPath(char *out, int cap)
{
    int i;
    FILE *probe;
    if (!out || cap <= 0)
        return 0;

#ifdef LINUX_VERSION
    {
        const char *xdgPictures = getenv("XDG_PICTURES_DIR");
        const char *home = getenv("HOME");
        const char *base = NULL;
        char pictures[512];
        if (xdgPictures && xdgPictures[0])
            base = xdgPictures;
        else if (home && home[0])
        {
            snprintf(pictures, sizeof(pictures), "%s/Pictures", home);
            base = pictures;
        }
        if (base)
        {
            mkdir(base, 0755);
            for (i = 1; i < 1000; i++)
            {
                snprintf(out, (size_t)cap, "%s/atanua%03d.png", base, i);
                probe = fopen(out, "rb");
                if (!probe)
                    return 1;
                fclose(probe);
            }
            return 0;
        }
    }
#endif

    for (i = 1; i < 1000; i++)
    {
        snprintf(out, (size_t)cap, "atanua%03d.png", i);
        probe = fopen(out, "rb");
        if (!probe)
            return 1;
        fclose(probe);
    }
    return 0;
}
