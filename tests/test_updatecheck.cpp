// Unit test for the pure update-check logic (appupdate.h): version parse
// and compare plus newest-release selection from a releases-API-shaped
// payload. No platform or UI dependencies.
#include <cstdio>
#include <cstring>
#include "appupdate.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } else { printf("PASS: %s\n", msg); } } while (0)

static const char kPayload[] =
    "["
    "{\"url\":\"https://api.github.com/repos/ProGaMEr110521/atanua-prime/releases/391467687\","
    "\"html_url\":\"https://github.com/ProGaMEr110521/atanua-prime/releases/tag/v1.3.141223\","
    "\"tag_name\":\"v1.3.141223\",\"draft\":false,\"prerelease\":true,"
    "\"assets\":["
    "{\"url\":\"https://api.github.com/x/1\",\"name\":\"atanua-linux-x86_64.tar.gz\","
    "\"browser_download_url\":\"https://github.com/ProGaMEr110521/atanua-prime/releases/download/v1.3.141223/atanua-linux-x86_64.tar.gz\"},"
    "{\"url\":\"https://api.github.com/x/2\",\"name\":\"atanua-windows-x64.zip\","
    "\"browser_download_url\":\"https://github.com/ProGaMEr110521/atanua-prime/releases/download/v1.3.141223/atanua-windows-x64.zip\"}"
    "]},"
    "{\"tag_name\":\"v1.3.141220\",\"draft\":false,\"prerelease\":false,"
    "\"html_url\":\"https://github.com/ProGaMEr110521/atanua-prime/releases/tag/v1.3.141220\","
    "\"assets\":[]},"
    "{\"tag_name\":\"v9.9.9\",\"draft\":true,\"prerelease\":false,"
    "\"html_url\":\"https://github.com/ProGaMEr110521/atanua-prime/releases/tag/v9.9.9\","
    "\"assets\":[]}"
    "]";

int main()
{
    int v[3];
    CHECK(AppUpdate_ParseVersion("v1.3.141223", v) && v[0] == 1 && v[1] == 3 && v[2] == 141223, "tagged version parses");
    CHECK(AppUpdate_ParseVersion("1.3.141220", v) && v[0] == 1 && v[1] == 3 && v[2] == 141220, "built-in version parses");
    CHECK(!AppUpdate_ParseVersion("v1.3", v), "short version rejected");
    CHECK(!AppUpdate_ParseVersion("", v), "empty version rejected");
    CHECK(!AppUpdate_ParseVersion("abc", v), "garbage version rejected");
    CHECK(!AppUpdate_ParseVersion(NULL, v), "null version rejected");
    {
        int a[3] = {1, 3, 141223}, b[3] = {1, 3, 141220}, c[3] = {1, 3, 141223}, d[3] = {2, 0, 0};
        CHECK(AppUpdate_CompareVersions(a, b) > 0, "newer tag compares greater");
        CHECK(AppUpdate_CompareVersions(b, a) < 0, "older tag compares smaller");
        CHECK(AppUpdate_CompareVersions(a, c) == 0, "equal tags compare equal");
        CHECK(AppUpdate_CompareVersions(a, d) < 0, "major version dominates");
    }
    {
        AppUpdateRelease rs[8];
        int n = AppUpdate_ParseReleases(kPayload, rs, 8);
        CHECK(n == 3, "all three releases parsed");
        if (n == 3)
        {
            int cur[3] = {1, 3, 141220};
            int picked = -1;
            CHECK(strcmp(rs[0].tag, "v1.3.141223") == 0, "tag kept with v prefix");
            CHECK(rs[0].prerelease == 1 && rs[0].draft == 0, "prerelease flag kept");
            CHECK(strstr(rs[0].windowsUrl, "atanua-windows-x64.zip") != NULL, "windows asset url picked");
            CHECK(strstr(rs[0].linuxUrl, "atanua-linux-x86_64.tar.gz") != NULL, "linux asset url picked");
            CHECK(strstr(rs[0].htmlUrl, "releases/tag/v1.3.141223") != NULL, "release page url kept");
            CHECK(AppUpdate_SelectNewest(rs, n, cur, &picked) && picked == 0, "prerelease newer than built-in is selected");
            {
                int even[3] = {1, 3, 141223};
                CHECK(!AppUpdate_SelectNewest(rs, n, even, &picked), "quiet when up to date");
            }
            {
                int ahead[3] = {9, 9, 10};
                CHECK(!AppUpdate_SelectNewest(rs, n, ahead, &picked), "quiet when ahead");
            }
        }
    }
    {
        AppUpdateRelease rs[8];
        CHECK(AppUpdate_ParseReleases("not json", rs, 8) == -1, "garbage payload rejected");
        CHECK(AppUpdate_ParseReleases("[]", rs, 8) == 0, "empty list parses to zero");
        CHECK(!AppUpdate_SelectNewest(rs, 0, NULL, NULL), "empty selection quiet");
    }
    {
        char host[128];
        CHECK(AppUpdate_ExtractHost("https://github.com/ProGaMEr110521/atanua-prime/releases/download/v1/x.zip", host, sizeof(host)) && strcmp(host, "github.com") == 0, "github host extracted");
        CHECK(AppUpdate_ExtractHost("https://objects.githubusercontent.com/foo/bar?x=1", host, sizeof(host)) && strcmp(host, "objects.githubusercontent.com") == 0, "object host extracted");
        CHECK(AppUpdate_ExtractHost("https://user@github.com:443/a", host, sizeof(host)) && strcmp(host, "github.com") == 0, "userinfo and port stripped");
        CHECK(!AppUpdate_ExtractHost("github.com/no-scheme", host, sizeof(host)), "scheme required");
        CHECK(!AppUpdate_ExtractHost("https://", host, sizeof(host)), "empty host rejected");
        CHECK(AppUpdate_HostAllowed("github.com"), "github allowed");
        CHECK(AppUpdate_HostAllowed("objects.githubusercontent.com"), "object CDN allowed");
        CHECK(AppUpdate_HostAllowed("release-assets.githubusercontent.com"), "asset CDN allowed");
        CHECK(!AppUpdate_HostAllowed("github.com.evil.com"), "lookalike domain denied");
        CHECK(!AppUpdate_HostAllowed("evilgithub.com"), "suffix without dot denied");
        CHECK(!AppUpdate_HostAllowed("example.com"), "random host denied");
        CHECK(!AppUpdate_HostAllowed(""), "empty host denied");
        CHECK(!AppUpdate_HostAllowed(NULL), "null host denied");
    }
    {
        char safe[64];
        AppUpdate_SafeTag("v1.3.141223", safe, sizeof(safe));
        CHECK(strcmp(safe, "v1.3.141223") == 0, "clean tag untouched");
        AppUpdate_SafeTag("a/b\\c:d*e?f\"g<h>i|j", safe, sizeof(safe));
        CHECK(strcmp(safe, "abcdefghij") == 0, "unsafe tag chars stripped");
        AppUpdate_SafeTag(NULL, safe, sizeof(safe));
        CHECK(strcmp(safe, "") == 0, "null tag empties");
    }
    if (failures == 0)
        printf("ALL UPDATE TESTS PASSED\n");
    return failures;
}
