/*
Atanua update check - background fetch plus one-shot prompt state.
Pure version/release logic lives in appupdate.h (unit tested); this file
fetches the releases payload over HTTPS on a worker thread (WinInet on
Windows, curl on Linux) and latches a newer-than-builtin result for the
UI thread to surface once. Failures and up-to-date stay silent.
*/
#include "atanua.h"
#include "atanua_internal.h"
#include "appupdate.h"

#include <SDL2/SDL.h>

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#endif

#include <stdio.h>
#include <string.h>

#define APPUPDATE_FETCH_CAP (64 * 1024)

static SDL_atomic_t s_started;
static SDL_atomic_t s_ready;
static int s_consumed = 0; /* main thread only */
static char s_version[64];
static char s_url[256];
static char s_fetchBuf[APPUPDATE_FETCH_CAP];

/* Download/install phases. Only forward motion; terminal states stick. */
enum
{
    AUP_IDLE = 0,
    AUP_BUSY_DL = 1,
    AUP_BUSY_EX = 2,
    AUP_READY = 3,
    AUP_FAILED = 4,
    AUP_DONE = 5
};
static SDL_atomic_t s_phase;
static SDL_atomic_t s_doneBytes;
static SDL_atomic_t s_totalBytes; /* -1 while unknown */
static SDL_atomic_t s_cancel;
static char s_resultmsg[256];

static int fetchReleases(char *out, int cap)
{
#ifdef _WIN32
    HINTERNET hInet = NULL;
    HINTERNET hConn = NULL;
    HINTERNET hReq = NULL;
    DWORD timeout;
    DWORD status = 0;
    DWORD statusLen = sizeof(status);
    DWORD got = 0;
    DWORD total = 0;
    int ok = 0;
    static const char *acceptTypes[] = { "*/*", NULL };
    if (!out || cap <= 0)
        return 0;
    hInet = InternetOpenA("AtanuaUpdateCheck/1.0", INTERNET_OPEN_TYPE_PRECONFIG,
        NULL, NULL, 0);
    if (!hInet)
        return 0;
    timeout = 8000;
    InternetSetOptionA(hInet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    timeout = 15000;
    InternetSetOptionA(hInet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    hConn = InternetConnectA(hInet, "api.github.com", INTERNET_DEFAULT_HTTPS_PORT,
        NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn)
    {
        InternetCloseHandle(hInet);
        return 0;
    }
    hReq = HttpOpenRequestA(hConn, "GET",
        "/repos/ProGaMEr110521/atanua-prime/releases?per_page=10",
        NULL, NULL, acceptTypes,
        INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hReq)
    {
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return 0;
    }
    HttpAddRequestHeadersA(hReq,
        "User-Agent: AtanuaUpdateCheck/1.0\r\nAccept: application/vnd.github+json\r\n",
        (DWORD)-1, HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);
    if (!HttpSendRequestA(hReq, NULL, 0, NULL, 0))
    {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return 0;
    }
    if (!HttpQueryInfoA(hReq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
        &status, &statusLen, NULL)
        || status != 200)
    {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return 0;
    }
    while (total < (DWORD)(cap - 1)
        && InternetReadFile(hReq, out + total, (DWORD)(cap - 1) - total, &got)
        && got > 0)
    {
        total += got;
    }
    out[total] = '\0';
    ok = total > 0 ? 1 : 0;
    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hInet);
    return ok;
#else
    FILE *p;
    size_t total = 0;
    size_t got;
    if (!out || cap <= 0)
        return 0;
    p = popen("curl -fsSL --connect-timeout 8 --max-time 15 "
        "-H 'User-Agent: AtanuaUpdateCheck/1.0' "
        "-H 'Accept: application/vnd.github+json' "
        "'https://api.github.com/repos/ProGaMEr110521/atanua-prime/releases?per_page=10' 2>/dev/null",
        "r");
    if (!p)
        return 0;
    while (total < (size_t)(cap - 1)
        && (got = fread(out + total, 1, (size_t)(cap - 1) - total, p)) > 0)
    {
        total += got;
    }
    out[total] = '\0';
    pclose(p);
    return total > 0 ? 1 : 0;
#endif
}

static int checkThread(void *unused)
{
    int current[3];
    AppUpdateRelease rs[16];
    int n;
    int picked;
    const char *url;
    (void)unused;
    if (!AppUpdate_ParseVersion(ATANUAVERSION, current))
        return 0;
    if (!fetchReleases(s_fetchBuf, (int)sizeof(s_fetchBuf)))
        return 0;
    n = AppUpdate_ParseReleases(s_fetchBuf, rs, 16);
    if (n <= 0)
        return 0;
    if (!AppUpdate_SelectNewest(rs, n, current, &picked))
        return 0;
#ifdef _WIN32
    url = rs[picked].windowsUrl[0] ? rs[picked].windowsUrl : rs[picked].htmlUrl;
#else
    url = rs[picked].linuxUrl[0] ? rs[picked].linuxUrl : rs[picked].htmlUrl;
#endif
    AppUpdate_CopyStr(s_version, (int)sizeof(s_version),
        rs[picked].tag, (int)strlen(rs[picked].tag));
    AppUpdate_CopyStr(s_url, (int)sizeof(s_url), url, (int)strlen(url));
    SDL_AtomicSet(&s_ready, 1);
    return 0;
}

void AppUpdate_StartCheck(void)
{
    if (SDL_AtomicCAS(&s_started, 0, 1))
    {
        SDL_Thread *t = SDL_CreateThread(checkThread, "atanua-update", NULL);
        if (t)
            SDL_DetachThread(t);
        else
            SDL_AtomicSet(&s_started, 0);
    }
}

int AppUpdate_Poll(char *versionOut, int versionCap, char *urlOut, int urlCap)
{
    if (s_consumed)
        return 0;
    if (SDL_AtomicGet(&s_ready) == 0)
        return 0;
    s_consumed = 1;
    AppUpdate_CopyStr(versionOut, versionCap, s_version, (int)strlen(s_version));
    AppUpdate_CopyStr(urlOut, urlCap, s_url, (int)strlen(s_url));
    return 1;
}

static void failWith(const char *msg)
{
    AppUpdate_CopyStr(s_resultmsg, (int)sizeof(s_resultmsg), msg, (int)strlen(msg));
    SDL_AtomicSet(&s_phase, AUP_FAILED);
}

static int cancelled(void)
{
    return SDL_AtomicGet(&s_cancel) != 0;
}

#ifdef _WIN32

/* Split https://host/path into parts. Returns 1 on success. */
static int splitHttpUrl(const char *url, char *hostOut, int hostCap,
    int *secureOut, char *objOut, int objCap)
{
    const char *p;
    const char *slash;
    if (!url || !hostOut || !secureOut || !objOut)
        return 0;
    if (strncmp(url, "https://", 8) == 0)
    {
        *secureOut = 1;
        p = url + 8;
    }
    else if (strncmp(url, "http://", 7) == 0)
    {
        *secureOut = 0;
        p = url + 7;
    }
    else
    {
        return 0;
    }
    slash = strchr(p, '/');
    if (!slash || slash == p)
        return 0;
    AppUpdate_CopyStr(hostOut, hostCap, p, (int)(slash - p));
    AppUpdate_CopyStr(objOut, objCap, slash, (int)strlen(slash));
    return hostOut[0] != '\0' && objOut[0] != '\0' ? 1 : 0;
}

static int queryFinalHost(HINTERNET hReq, char *hostOut, int hostCap)
{
    char url[1024];
    DWORD len = (DWORD)sizeof(url);
    if (!InternetQueryOptionA(hReq, INTERNET_OPTION_URL, url, &len))
        return 0;
    return AppUpdate_ExtractHost(url, hostOut, hostCap);
}

/* Returns 1 (ok), 0 (failed), -1 (cancelled). */
static int downloadFileWin(const char *url, const char *destPath)
{
    char host[256];
    char obj[512];
    char finalHost[256];
    int secure = 0;
    HINTERNET hInet = NULL;
    HINTERNET hConn = NULL;
    HINTERNET hReq = NULL;
    DWORD timeout;
    DWORD status = 0;
    DWORD statusLen = sizeof(status);
    DWORD total = 0;
    DWORD len = 0;
    DWORD got = 0;
    FILE *out = NULL;
    static char chunk[32768];
    int rc = 0;
    static const char *acceptTypes[] = { "*/*", NULL };
    if (!splitHttpUrl(url, host, (int)sizeof(host), &secure, obj, (int)sizeof(obj)))
        return 0;
    hInet = InternetOpenA("AtanuaUpdateCheck/1.0", INTERNET_OPEN_TYPE_PRECONFIG,
        NULL, NULL, 0);
    if (!hInet)
        return 0;
    timeout = 8000;
    InternetSetOptionA(hInet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    timeout = 15000;
    InternetSetOptionA(hInet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    hConn = InternetConnectA(hInet, host,
        secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT,
        NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn)
    {
        InternetCloseHandle(hInet);
        return 0;
    }
    hReq = HttpOpenRequestA(hConn, "GET", obj, NULL, NULL, acceptTypes,
        (secure ? INTERNET_FLAG_SECURE : 0) | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hReq)
    {
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return 0;
    }
    HttpAddRequestHeadersA(hReq, "User-Agent: AtanuaUpdateCheck/1.0\r\n",
        (DWORD)-1, HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);
    if (!HttpSendRequestA(hReq, NULL, 0, NULL, 0))
    {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return 0;
    }
    if (!HttpQueryInfoA(hReq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
        &status, &statusLen, NULL)
        || status != 200)
    {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return 0;
    }
    if (!queryFinalHost(hReq, finalHost, (int)sizeof(finalHost))
        || !AppUpdate_HostAllowed(finalHost))
    {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return 0;
    }
    len = sizeof(total);
    if (HttpQueryInfoA(hReq, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
        &total, &len, NULL)
        && total > 0)
    {
        SDL_AtomicSet(&s_totalBytes, (int)total);
    }
    out = fopen(destPath, "wb");
    if (!out)
    {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return 0;
    }
    rc = 1;
    while (InternetReadFile(hReq, chunk, (DWORD)sizeof(chunk), &got) && got > 0)
    {
        if (cancelled())
        {
            rc = -1;
            break;
        }
        if (fwrite(chunk, 1, got, out) != got)
        {
            rc = 0;
            break;
        }
        SDL_AtomicAdd(&s_doneBytes, (int)got);
    }
    fclose(out);
    if (rc != 1)
        DeleteFileA(destPath);
    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hInet);
    return rc;
}

static int shortPath(const char *path, char *out, int cap)
{
    DWORD n = GetShortPathNameA(path, out, (DWORD)cap);
    if (n == 0 || n >= (DWORD)cap)
        return 0;
    return 1;
}

static int extractZipWin(const char *zipPath, const char *destDir)
{
    char sysdir[260];
    char cmd[1024];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    if (!GetSystemDirectoryA(sysdir, (DWORD)sizeof(sysdir)))
        return 0;
    snprintf(cmd, sizeof(cmd), "\"%s\\tar.exe\" -xf \"%s\" -C \"%s\"",
        sysdir, zipPath, destDir);
    si.cb = sizeof(si);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW,
        NULL, NULL, &si, &pi))
    {
        return 0;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0 ? 1 : 0;
}

static int writeUpdaterBat(const char *batPath, const char *exeName,
    const char *stageShort, const char *appShort)
{
    FILE *f = fopen(batPath, "w");
    if (!f)
        return 0;
    fprintf(f,
        "@echo off\n"
        ":waitloop\n"
        "tasklist /FI \"IMAGENAME eq %s\" 2>NUL | find /I \"%s\" >NUL\n"
        "if not errorlevel 1 ( timeout /t 1 /nobreak >NUL & goto waitloop )\n"
        "timeout /t 1 /nobreak >NUL\n"
        "xcopy /E /Y /Q /R \"%s\\atanua-windows\\*\" \"%s\\\"\n"
        "start \"\" \"%s\\%s\"\n"
        "rmdir /S /Q \"%s\"\n",
        exeName, exeName, stageShort, appShort, appShort, exeName, stageShort);
    return fclose(f) == 0 ? 1 : 0;
}

static int launchUpdaterWin(const char *batPath)
{
    char sysdir[260];
    char cmd[1024];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    if (!GetSystemDirectoryA(sysdir, (DWORD)sizeof(sysdir)))
        return 0;
    snprintf(cmd, sizeof(cmd), "%s\\cmd.exe /S /C \"\"%s\"\"", sysdir, batPath);
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWMINIMIZED;
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE,
        NULL, NULL, &si, &pi))
    {
        return 0;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 1;
}

#else /* not Windows: best-effort curl flow, silent when unavailable */

#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

extern char **environ;

/* Staged Linux package paths for ApplyAndRelaunch on the main thread. */
static char s_stageDir[1024];
static char s_appExe[1024];
static int s_linuxStaged = 0;

static int shellSafe(const char *s)
{
    if (!s || !*s)
        return 0;
    while (*s)
    {
        if (*s == '\'')
            return 0;
        s++;
    }
    return 1;
}

static int runCmd(const char *cmd)
{
    FILE *p;
    int rc;
    if (!cmd)
        return -1;
    p = popen(cmd, "r");
    if (!p)
        return -1;
    rc = pclose(p);
    return rc;
}

static int haveCurl(void)
{
    FILE *p = popen("command -v curl 2>/dev/null", "r");
    char c = 0;
    size_t n = 0;
    if (!p)
        return 0;
    n = fread(&c, 1, 1, p);
    pclose(p);
    return n == 1 ? 1 : 0;
}

/* Returns 1 (ok), 0 (failed), -1 (cancelled). No live percent from curl. */
static int downloadFileNix(const char *url, const char *destPath)
{
    char cmd[1024];
    int rc;
    if (!shellSafe(url) || !shellSafe(destPath))
        return 0;
    {
        char host[256];
        if (!AppUpdate_ExtractHost(url, host, (int)sizeof(host))
            || !AppUpdate_HostAllowed(host))
        {
            return 0;
        }
    }
    snprintf(cmd, sizeof(cmd),
        "curl -fL --connect-timeout 8 --max-time 180 -o '%s' '%s'",
        destPath, url);
    rc = runCmd(cmd);
    if (cancelled())
    {
        unlink(destPath);
        return -1;
    }
    return rc == 0 ? 1 : 0;
}

/* Copy one file. Works while the destination path names a running binary
 * only if the caller writes to a temp name first; do not truncate in place. */
static int copyFileNix(const char *src, const char *dst)
{
    char buf[65536];
    FILE *in;
    FILE *out;
    size_t n;
    if (!src || !dst)
        return 0;
    in = fopen(src, "rb");
    if (!in)
        return 0;
    out = fopen(dst, "wb");
    if (!out)
    {
        fclose(in);
        return 0;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
    {
        if (fwrite(buf, 1, n, out) != n)
        {
            fclose(in);
            fclose(out);
            unlink(dst);
            return 0;
        }
    }
    if (ferror(in))
    {
        fclose(in);
        fclose(out);
        unlink(dst);
        return 0;
    }
    fclose(in);
    if (fclose(out) != 0)
    {
        unlink(dst);
        return 0;
    }
    return 1;
}

/* Replace the running executable via temp + rename (avoids ETXTBSY). */
static int installBinaryNix(const char *srcBin, const char *destExe)
{
    char tmpPath[1080];
    if (!srcBin || !destExe || !*destExe)
        return 0;
    snprintf(tmpPath, sizeof(tmpPath), "%s.new", destExe);
    unlink(tmpPath);
    if (!copyFileNix(srcBin, tmpPath))
        return 0;
    if (chmod(tmpPath, 0755) != 0)
    {
        unlink(tmpPath);
        return 0;
    }
    if (rename(tmpPath, destExe) != 0)
    {
        unlink(tmpPath);
        return 0;
    }
    return 1;
}

/* Prefix install: binary in .../bin, data in .../share/atanua.
 * Portable: data next to the binary. */
static int installDataNix(const char *pkgRoot, const char *exeDir)
{
    char shareProbe[PATH_MAX];
    char shareResolved[PATH_MAX];
    char cmd[2200];
    const char *dataDest;
    if (!pkgRoot || !exeDir || !shellSafe(pkgRoot) || !shellSafe(exeDir))
        return 0;
    snprintf(shareProbe, sizeof(shareProbe), "%s/../share/atanua", exeDir);
    if (realpath(shareProbe, shareResolved)
        && shellSafe(shareResolved)
        && access(shareResolved, W_OK) == 0)
    {
        dataDest = shareResolved;
    }
    else
    {
        dataDest = exeDir;
    }
    if (!shellSafe(dataDest))
        return 0;
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s/data' && cp -a '%s/atanua-linux/data/.' '%s/data/'",
        dataDest, pkgRoot, dataDest);
    return runCmd(cmd) == 0 ? 1 : 0;
}

/* Reject packages whose dynamic libs are missing here (e.g. Ubuntu
 * libtinyxml2.so.10 on Arch/Omarchy). Better to fail before replacing. */
static int binaryDepsOkNix(const char *path)
{
    char cmd[1200];
    FILE *p;
    char line[512];
    int sawLine = 0;
    int bad = 0;
    if (!path || !shellSafe(path) || access(path, X_OK) != 0)
        return 0;
    snprintf(cmd, sizeof(cmd), "ldd '%s' 2>/dev/null", path);
    p = popen(cmd, "r");
    if (!p)
        return 0;
    while (fgets(line, (int)sizeof(line), p))
    {
        sawLine = 1;
        if (strstr(line, "not found"))
            bad = 1;
    }
    pclose(p);
    return (sawLine && !bad) ? 1 : 0;
}

static void cleanupStageNix(void)
{
    char rm[1100];
    if (s_stageDir[0] && shellSafe(s_stageDir))
    {
        snprintf(rm, sizeof(rm), "rm -rf '%s'", s_stageDir);
        runCmd(rm);
    }
    s_stageDir[0] = '\0';
    s_linuxStaged = 0;
}

#endif

static int downloadThread(void *unused)
{
    char safeTag[64];
    char stageDir[1024];
    char assetPath[1024];
    char urlCopy[256];
    const char *base;
    const char *suffix;
    (void)unused;
    AppUpdate_SafeTag(s_version, safeTag, (int)sizeof(safeTag));
    if (safeTag[0] == '\0')
    {
        failWith("Update failed: bad version tag.");
        return 0;
    }
    AppUpdate_CopyStr(urlCopy, (int)sizeof(urlCopy), s_url, (int)strlen(s_url));
    base = strrchr(urlCopy, '/');
    base = base ? base + 1 : urlCopy;
    if (!*base)
    {
        failWith("Update failed: bad download address.");
        return 0;
    }
    suffix = strrchr(base, '.');
#ifdef _WIN32
    {
        char tmpPath[1024];
        char exePath[1024];
        char appDir[1024];
        char stageShort[1024];
        char appShort[1024];
        char batPath[1024];
        char verifyPath[1024];
        char *exeName;
        char *slash;
        DWORD tlen;
        int dl;
        if (GetTempPathA((DWORD)sizeof(tmpPath), tmpPath) == 0)
        {
            failWith("Update failed: no temp folder.");
            return 0;
        }
        snprintf(stageDir, sizeof(stageDir), "%satanua_update\\%s", tmpPath, safeTag);
        {
            char parentDir[1024];
            snprintf(parentDir, sizeof(parentDir), "%satanua_update", tmpPath);
            CreateDirectoryA(parentDir, NULL);
            if (!CreateDirectoryA(stageDir, NULL)
                && GetLastError() != ERROR_ALREADY_EXISTS)
            {
                failWith("Update failed: no temp folder.");
                return 0;
            }
        }
        snprintf(assetPath, sizeof(assetPath), "%s\\%s", stageDir, base);
        SDL_AtomicSet(&s_doneBytes, 0);
        SDL_AtomicSet(&s_totalBytes, -1);
        dl = downloadFileWin(s_url, assetPath);
        if (dl < 0 || cancelled())
            return 0;
        if (!dl)
        {
            failWith("Update failed: download did not complete.");
            return 0;
        }
        SDL_AtomicSet(&s_phase, AUP_BUSY_EX);
        if (!extractZipWin(assetPath, stageDir))
        {
            failWith("Update failed: could not unpack the archive.");
            return 0;
        }
        if (cancelled())
            return 0;
        snprintf(verifyPath, sizeof(verifyPath), "%s\\atanua-windows\\atanua.exe", stageDir);
        tlen = GetFileAttributesA(verifyPath);
        if (tlen == INVALID_FILE_ATTRIBUTES)
        {
            failWith("Update failed: package layout unexpected.");
            return 0;
        }
        if (GetModuleFileNameA(NULL, exePath, (DWORD)sizeof(exePath)) == 0)
        {
            failWith("Update failed: current location unknown.");
            return 0;
        }
        slash = strrchr(exePath, '\\');
        if (!slash || !slash[1])
        {
            failWith("Update failed: current location unknown.");
            return 0;
        }
        exeName = slash + 1;
        *slash = '\0';
        strncpy(appDir, exePath, sizeof(appDir) - 1);
        appDir[sizeof(appDir) - 1] = '\0';
        if (!shortPath(stageDir, stageShort, (int)sizeof(stageShort))
            || !shortPath(appDir, appShort, (int)sizeof(appShort)))
        {
            failWith("Update failed: path setup failed.");
            return 0;
        }
        snprintf(batPath, sizeof(batPath), "%s\\update.bat", stageShort);
        if (!writeUpdaterBat(batPath, exeName, stageShort, appShort)
            || !launchUpdaterWin(batPath))
        {
            failWith("Update failed: restarter setup failed.");
            return 0;
        }
        AppUpdate_CopyStr(s_resultmsg, (int)sizeof(s_resultmsg),
            "Restarting to finish the update.", 31);
        SDL_AtomicSet(&s_phase, AUP_READY);
        return 0;
    }
#else
    {
        /* Stage only. Install + execve of the absolute binary path happen on
         * the main thread in AppUpdate_ApplyAndRelaunch so a running binary
         * can be replaced (temp + rename) and the same path the user launched
         * comes back up. The old helper script waited for exit, cp'd the
         * portable layout into the bin dir, then relaunched with stderr
         * discarded — so a missing shared library killed the new process
         * silently and left the user with no window. */
        const char *tmpBase = getenv("TMPDIR");
        char appExe[1024];
        char pkgPath[1024];
        ssize_t linkLen;
        int dl;
        if (!tmpBase || !*tmpBase)
            tmpBase = "/tmp";
        snprintf(stageDir, sizeof(stageDir), "%s/atanua_update/%s", tmpBase, safeTag);
        {
            char mk[1024];
            snprintf(mk, sizeof(mk), "mkdir -p '%s'", stageDir);
            if (!shellSafe(stageDir) || runCmd(mk) != 0)
            {
                failWith("Update failed: no temp folder.");
                return 0;
            }
        }
        if (!haveCurl())
        {
            failWith("Update failed: curl not found; download it manually.");
            return 0;
        }
        snprintf(assetPath, sizeof(assetPath), "%s/%s", stageDir, base);
        if (!shellSafe(assetPath))
        {
            failWith("Update failed: bad file name.");
            return 0;
        }
        {
            const char *dot = strrchr(base, '.');
            if (!dot || (strcmp(dot, ".gz") != 0 && strcmp(dot, ".tgz") != 0))
            {
                failWith("Update failed: unexpected package type.");
                return 0;
            }
        }
        SDL_AtomicSet(&s_doneBytes, 0);
        SDL_AtomicSet(&s_totalBytes, -1);
        dl = downloadFileNix(s_url, assetPath);
        if (dl < 0 || cancelled())
            return 0;
        if (!dl)
        {
            failWith("Update failed: download did not complete.");
            return 0;
        }
        SDL_AtomicSet(&s_phase, AUP_BUSY_EX);
        {
            char cmd[2048];
            snprintf(cmd, sizeof(cmd), "tar -xzf '%s' -C '%s'", assetPath, stageDir);
            if (runCmd(cmd) != 0)
            {
                failWith("Update failed: could not unpack the archive.");
                return 0;
            }
        }
        if (cancelled())
            return 0;
        snprintf(pkgPath, sizeof(pkgPath), "%s/atanua-linux/atanua", stageDir);
        if (access(pkgPath, F_OK) != 0)
        {
            failWith("Update failed: package layout unexpected.");
            return 0;
        }
        if (!binaryDepsOkNix(pkgPath))
        {
            failWith("Update failed: this build needs libraries not "
                "available on this system.");
            return 0;
        }
        linkLen = readlink("/proc/self/exe", appExe, sizeof(appExe) - 1);
        if (linkLen <= 0)
        {
            failWith("Update failed: current location unknown.");
            return 0;
        }
        appExe[linkLen] = '\0';
        if (!strrchr(appExe, '/') || !shellSafe(appExe) || !shellSafe(stageDir))
        {
            failWith("Update failed: current location unknown.");
            return 0;
        }
        AppUpdate_CopyStr(s_appExe, (int)sizeof(s_appExe), appExe, (int)strlen(appExe));
        AppUpdate_CopyStr(s_stageDir, (int)sizeof(s_stageDir), stageDir, (int)strlen(stageDir));
        s_linuxStaged = 1;
        AppUpdate_CopyStr(s_resultmsg, (int)sizeof(s_resultmsg),
            "Restarting to finish the update.", 31);
        SDL_AtomicSet(&s_phase, AUP_READY);
        return 0;
    }
#endif
}

void AppUpdate_BeginDownload(void)
{
    if (SDL_AtomicCAS(&s_phase, AUP_IDLE, AUP_BUSY_DL))
    {
        SDL_AtomicSet(&s_doneBytes, 0);
        SDL_AtomicSet(&s_totalBytes, -1);
        SDL_AtomicSet(&s_cancel, 0);
        SDL_Thread *t = SDL_CreateThread(downloadThread, "atanua-dl", NULL);
        if (!t)
            failWith("Update failed: could not start download.");
        else
            SDL_DetachThread(t);
    }
}

int AppUpdate_DownloadActive(int *percentOut)
{
    int phase = SDL_AtomicGet(&s_phase);
    int total;
    int done;
    if (phase != AUP_BUSY_DL && phase != AUP_BUSY_EX)
        return 0;
    if (percentOut)
    {
        if (phase == AUP_BUSY_EX)
        {
            *percentOut = -2;
        }
        else
        {
            total = SDL_AtomicGet(&s_totalBytes);
            done = SDL_AtomicGet(&s_doneBytes);
            if (total > 0 && done >= 0)
            {
                long pct = (long)done * 100L / (long)total;
                *percentOut = pct > 100 ? 100 : (int)pct;
            }
            else
            {
                *percentOut = -1;
            }
        }
    }
    return 1;
}

void AppUpdate_CancelDownload(void)
{
    SDL_AtomicSet(&s_cancel, 1);
}

int AppUpdate_ConsumeReady(char *msgOut, int msgCap)
{
    int phase = SDL_AtomicGet(&s_phase);
    if (phase != AUP_READY && phase != AUP_FAILED)
        return 0;
    SDL_AtomicSet(&s_phase, AUP_DONE);
    if (msgOut && msgCap > 0)
        AppUpdate_CopyStr(msgOut, msgCap, s_resultmsg, (int)strlen(s_resultmsg));
    return phase == AUP_READY ? 1 : 2;
}

int AppUpdate_ApplyAndRelaunch(void)
{
#ifdef _WIN32
    /* Windows restarter bat was already launched from the download thread. */
    return 1;
#else
    char pkgBin[1100];
    char bakPath[1080];
    char exeDir[1024];
    char *slash;
    char *args[2];
    pid_t child;
    int status = 0;
    int i;

    if (!s_linuxStaged || !s_appExe[0] || !s_stageDir[0])
        return 0;

    snprintf(pkgBin, sizeof(pkgBin), "%s/atanua-linux/atanua", s_stageDir);
    if (access(pkgBin, R_OK) != 0)
    {
        cleanupStageNix();
        return 0;
    }
    /* Never replace a working binary with one that cannot load here. */
    if (!binaryDepsOkNix(pkgBin))
    {
        cleanupStageNix();
        return 0;
    }

    AppUpdate_CopyStr(exeDir, (int)sizeof(exeDir), s_appExe, (int)strlen(s_appExe));
    slash = strrchr(exeDir, '/');
    if (!slash)
    {
        cleanupStageNix();
        return 0;
    }
    *slash = '\0';

    snprintf(bakPath, sizeof(bakPath), "%s.bak", s_appExe);
    unlink(bakPath);
    if (!copyFileNix(s_appExe, bakPath))
    {
        cleanupStageNix();
        return 0;
    }

    if (!installBinaryNix(pkgBin, s_appExe))
    {
        unlink(bakPath);
        cleanupStageNix();
        return 0;
    }
    if (!installDataNix(s_stageDir, exeDir))
    {
        rename(bakPath, s_appExe);
        cleanupStageNix();
        return 0;
    }

    child = fork();
    if (child < 0)
    {
        rename(bakPath, s_appExe);
        cleanupStageNix();
        return 0;
    }
    if (child == 0)
    {
        /* Detach from the dying parent's session; keep environ so Wayland /
         * display / library path match the process the user was running. */
        setsid();
        args[0] = s_appExe;
        args[1] = NULL;
        execve(s_appExe, args, environ);
        _exit(127);
    }

    /* If the new process dies immediately, restore the previous binary so
     * the user is not left with a closed app and nothing that can start. */
    for (i = 0; i < 15; i++)
    {
        SDL_Delay(100);
        if (waitpid(child, &status, WNOHANG) == child)
        {
            rename(bakPath, s_appExe);
            cleanupStageNix();
            return 0;
        }
    }

    unlink(bakPath);
    cleanupStageNix();
    return 1;
#endif
}
