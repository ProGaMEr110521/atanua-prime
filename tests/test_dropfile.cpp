// Unit tests for src/include/dropfile.h: extension checks, drop acceptance
// and basename helpers used by argv/drop file opening. No window needed.
#include <cstdio>
#include <cstring>
#include "dropfile.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } else { printf("PASS: %s\n", msg); } } while (0)

int main()
{
    using namespace DropFile;

    CHECK(strcmp(extension(), ".atanua") == 0, "canonical extension is .atanua");

    CHECK(hasAtanuaExtension("foo.atanua"), "lowercase extension accepted");
    CHECK(hasAtanuaExtension("foo.ATANUA"), "uppercase extension accepted");
    CHECK(hasAtanuaExtension("foo.AtAnUa"), "mixed-case extension accepted");
    CHECK(hasAtanuaExtension("C:\\dir\\foo.atanua"), "windows path accepted");
    CHECK(hasAtanuaExtension("/tmp/foo.atanua"), "posix path accepted");
    CHECK(hasAtanuaExtension("a.atanua"), "short name accepted");

    CHECK(!hasAtanuaExtension(0), "null path rejected");
    CHECK(!hasAtanuaExtension(""), "empty path rejected");
    CHECK(!hasAtanuaExtension(".atanua"), "bare extension without name rejected");
    CHECK(!hasAtanuaExtension("foo.txt"), "other extension rejected");
    CHECK(!hasAtanuaExtension("foo.atanua.bak"), "trailing suffix rejected");
    CHECK(!hasAtanuaExtension("fooatanua"), "missing dot rejected");
    CHECK(!hasAtanuaExtension("foo.atanu"), "truncated extension rejected");
    CHECK(!hasAtanuaExtension("foo.atanuax"), "overlong extension rejected");

    CHECK(shouldAcceptDrop("foo.atanua"), "valid drop accepted");
    CHECK(shouldAcceptDrop("C:\\dir\\Foo.ATANUA"), "valid windows drop accepted");
    CHECK(!shouldAcceptDrop(0), "null drop rejected");
    CHECK(!shouldAcceptDrop(""), "empty drop rejected");
    CHECK(!shouldAcceptDrop("foo.txt"), "non-atanua drop rejected");
    CHECK(!shouldAcceptDrop("evil.exe"), "executable drop rejected");

    CHECK(strcmp(baseName("C:\\dir\\foo.atanua"), "foo.atanua") == 0, "windows basename");
    CHECK(strcmp(baseName("/tmp/dir/foo.atanua"), "foo.atanua") == 0, "posix basename");
    CHECK(strcmp(baseName("foo.atanua"), "foo.atanua") == 0, "bare basename");
    CHECK(strcmp(baseName(0), "") == 0, "null basename is empty");
    CHECK(strcmp(baseName(""), "") == 0, "empty basename is empty");

    if (failures == 0)
        printf("ALL DROPFILE TESTS PASSED\n");
    else
        printf("%d DROPFILE TESTS FAILED\n", failures);
    return failures ? 1 : 0;
}
