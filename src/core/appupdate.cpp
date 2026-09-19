/*
Atanua update check - background fetch plus one-shot prompt state.
Pure version/release logic lives in appupdate.h (unit tested); this file
only fetches the releases payload over HTTPS on a worker thread (Windows
WinInet, unavailable elsewhere) and latches a newer-than-builtin result
for the UI thread to surface once. Failures and up-to-date stay silent.
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
    (void)out;
    (void)cap;
    return 0;
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
