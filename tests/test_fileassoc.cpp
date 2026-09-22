// Unit tests for src/include/fileassoc.h: ProgID layout, command/icon
// formatting and open-command matching for the per-user .atanua
// association. No registry access, no window.
#include <cstdio>
#include <cstring>
#include "fileassoc.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } else { printf("PASS: %s\n", msg); } } while (0)

int main()
{
    using namespace FileAssoc;

    CHECK(strcmp(progId(), "AtanuaPrime.Design") == 0, "progid stable");
    CHECK(strcmp(extensionKey(), ".atanua") == 0, "extension key dotted");
    CHECK(strcmp(openCommandSubkey(), "shell\\open\\command") == 0, "open verb subkey");
    CHECK(strcmp(defaultIconSubkey(), "DefaultIcon") == 0, "icon subkey");

    char buf[512];
    CHECK(formatOpenCommand("C:\\app\\atanua.exe", buf, sizeof(buf)), "open command formats");
    CHECK(strcmp(buf, "\"C:\\app\\atanua.exe\" \"%1\"") == 0, "open command quoted with %1");
    CHECK(formatDefaultIcon("C:\\app\\atanua.exe", buf, sizeof(buf)), "icon value formats");
    CHECK(strcmp(buf, "\"C:\\app\\atanua.exe\",0") == 0, "icon value points at exe index 0");

    CHECK(!formatOpenCommand(0, buf, sizeof(buf)), "null exe rejected");
    CHECK(!formatOpenCommand("", buf, sizeof(buf)), "empty exe rejected");
    CHECK(!formatOpenCommand("C:\\app\\atanua.exe", 0, sizeof(buf)), "null buffer rejected");
    CHECK(!formatOpenCommand("C:\\app\\atanua.exe", buf, 0), "zero cap rejected");
    char tiny[8];
    CHECK(!formatOpenCommand("C:\\app\\atanua.exe", tiny, sizeof(tiny)), "small buffer rejected");
    CHECK(!formatDefaultIcon("", buf, sizeof(buf)), "empty exe icon rejected");
    CHECK(!formatDefaultIcon("C:\\app\\atanua.exe", tiny, sizeof(tiny)), "small icon buffer rejected");

    CHECK(openCommandMatchesExe("\"C:\\app\\atanua.exe\" \"%1\"", "C:\\app\\atanua.exe"), "quoted command matches");
    CHECK(openCommandMatchesExe("C:\\app\\atanua.exe \"%1\"", "C:\\app\\atanua.exe"), "bare command matches");
    CHECK(openCommandMatchesExe("\"C:\\APP\\ATANUA.EXE\" \"%1\"", "c:\\app\\atanua.exe"), "match is case-insensitive");
    CHECK(!openCommandMatchesExe(0, "C:\\app\\atanua.exe"), "null stored never matches");
    CHECK(!openCommandMatchesExe("\"C:\\app\\atanua.exe\" \"%1\"", 0), "null exe never matches");
    CHECK(!openCommandMatchesExe("", "C:\\app\\atanua.exe"), "empty stored never matches");
    CHECK(!openCommandMatchesExe("\"C:\\other\\app.exe\" \"%1\"", "C:\\app\\atanua.exe"), "foreign exe rejected");
    CHECK(!openCommandMatchesExe("\"C:\\app\\atanua.exe\"", "C:\\app\\atanua.exe"), "command without %1 rejected");
    CHECK(!openCommandMatchesExe("notepad.exe \"%1\"", "C:\\app\\atanua.exe"), "short foreign rejected");

    if (failures == 0)
        printf("ALL FILEASSOC TESTS PASSED\n");
    else
        printf("%d FILEASSOC TESTS FAILED\n", failures);
    return failures ? 1 : 0;
}
