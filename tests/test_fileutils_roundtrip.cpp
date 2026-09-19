// Round-trip test for the REAL shipped snapshot primitives (fileutils.cpp).
// do_savebinary/do_loadbinary (undo, redo, box cache) persist every chip
// name through File::readchars, so this drives the exact write/read path a
// snapshot takes: magic, counts, words, names, ints, then truncation safety.
#include <cstdio>
#include <cstring>
#include "fileutils.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } else { printf("PASS: %s\n", msg); } } while (0)

int main()
{
    // Snapshot-like sequence: magic, counts, one chip record, one wire.
    {
        MemoryFile f;
        f.writeint(0x02617441);
        f.writeint(1);
        f.writeint(1);
        f.writeword(9);
        f.writechars("logic AND", 9);
        f.writeint(196608);
        f.writeint(327680);
        f.writeint(0);
        f.writeint(0);
        f.writeint((0 << 16) | 0);
        f.writeint((1 << 16) | 0);
        f.writeint(0);
        CHECK((int)f.mData.size() == 4 + 4 + 4 + 2 + 9 + 4 + 4 + 4 + 4 + 4 + 4 + 4,
            "snapshot bytes actually stored (names must be written)");
        f.seek(0);
        CHECK(f.readint() == 0x02617441, "magic round-trips");
        CHECK(f.readint() == 1, "chipcount round-trips");
        CHECK(f.readint() == 1, "wirecount round-trips");
        CHECK(f.readword() == 9, "name length round-trips");
        char name[32];
        memset(name, 0, sizeof(name));
        f.readchars(name, 9);
        CHECK(strcmp(name, "logic AND") == 0, "chip name survives readchars");
        CHECK(f.readint() == 196608, "stream stays aligned after the name");
        CHECK(f.readint() == 327680, "second int aligned");
        CHECK(f.readint() == 0, "angle aligned");
        CHECK(f.readint() == 0, "box aligned");
        CHECK(f.readint() == 0, "first wire endpoint aligned");
        CHECK(f.readint() == (1 << 16), "second wire endpoint aligned");
        CHECK(f.readint() == 0, "wire box aligned");
    }
    // Negative and edge integers keep their bits.
    {
        MemoryFile f;
        f.writeint(-5);
        f.writeint(0x7fffffff);
        f.writeword(-1);
        f.writebyte(-128);
        f.seek(0);
        CHECK(f.readint() == -5, "negative int round-trips");
        CHECK(f.readint() == 0x7fffffff, "max int round-trips");
        CHECK(f.readword() == -1, "negative word round-trips");
        CHECK(f.readbyte() == -128, "negative byte round-trips");
    }
    // Truncated snapshots must zero-fill, never crash or desync callers.
    {
        MemoryFile f;
        f.writeint(1234);
        f.seek(0);
        CHECK(f.readint() == 1234, "exact-size read works");
        CHECK(f.readint() == 0, "over-read int returns 0");
        CHECK(f.readword() == 0, "over-read word returns 0");
        CHECK(f.readbyte() == 0, "over-read byte returns 0");
        char buf[8];
        memset(buf, 0x7f, sizeof(buf));
        f.readchars(buf, 4);
        int clean = 1;
        for (int i = 0; i < 8; i++)
            if (buf[i] != (i < 4 ? 0 : 0x7f)) clean = 0;
        CHECK(clean, "over-read chars zero-fill without touching the tail");
    }
    // Disk-backed Files keep working through the same virtuals.
    {
        FILE *tmp = tmpfile();
        CHECK(tmp != NULL, "tmpfile available");
        if (tmp)
        {
            File f(tmp);
            f.writeint(0x02617441);
            f.writeword(3);
            f.writechars("Box", 3);
            f.seek(0);
            CHECK(f.readint() == 0x02617441, "disk magic round-trips");
            CHECK(f.readword() == 3, "disk word round-trips");
            char name[8];
            memset(name, 0, sizeof(name));
            f.readchars(name, 3);
            CHECK(strcmp(name, "Box") == 0, "disk name round-trips");
        }
    }
    if (failures == 0)
        printf("ALL FILEUTILS TESTS PASSED\n");
    return failures;
}
