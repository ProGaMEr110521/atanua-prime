#include <cstdio>
#include <cstring>
#include "quick_find.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } else { printf("PASS: %s\n", msg); } } while (0)

int main()
{
    using namespace QuickFind;

    CHECK(tapWindowMs() == 400, "tap window is 400ms");

    /* Matching mirrors the sidebar filter exactly. */
    CHECK(substringMatch("logic AND", "") == 1, "empty filter matches");
    CHECK(substringMatch("logic AND", 0) == 1, "null filter matches");
    CHECK(substringMatch(0, "and") == 0, "null name never matches");
    CHECK(substringMatch("logic AND", "and") == 1, "case-insensitive tail matches");
    CHECK(substringMatch("logic AND", "AND") == 1, "exact case matches");
    CHECK(substringMatch("7400", "74") == 1, "head matches");
    CHECK(substringMatch("button ('a')", "('A')") == 1, "punctuation matches");
    CHECK(substringMatch("LED (red)", "blue") == 0, "non-match rejected");
    CHECK(substringMatch("logic AND", "logic AND gate") == 0, "longer filter rejected");

    /* Double-tap timing: arm, fire in-window, disarm, expiry. */
    unsigned last = 0;
    CHECK(doubleTapTick(&last, 1000, 400) == 0, "first tap arms without firing");
    CHECK(doubleTapTick(&last, 1200, 400) == 1, "second tap in-window fires");
    CHECK(last == 0, "fire disarms the detector");
    CHECK(doubleTapTick(&last, 1300, 400) == 0, "tap after fire re-arms");
    CHECK(doubleTapTick(&last, 2000, 400) == 0, "tap past the window does not fire");
    CHECK(doubleTapTick(0, 1000, 400) == 0, "null state never fires");
    /* Tick-counter wrap: 0xFFFFFF00 -> 100 is 356ms, inside the window. */
    unsigned wrap = 0;
    CHECK(doubleTapTick(&wrap, 0xFFFFFF00u, 400) == 0, "wrap base arms");
    CHECK(doubleTapTick(&wrap, 100u, 400) == 1, "wrap-around delta fires");

    if (failures == 0)
        printf("ALL QUICKFIND TESTS PASSED\n");
    return failures;
}
