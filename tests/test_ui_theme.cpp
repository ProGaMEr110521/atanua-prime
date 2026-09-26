#include <cstdio>
#include "ui_theme.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } else { printf("PASS: %s\n", msg); } } while (0)

int main()
{
    for (int t = 0; t < UiTheme::PALETTE_COUNT; t++)
    {
        const UiTheme::Palette &p = UiTheme::palette(t);
        CHECK((p.chrome >> 24) == 0xff && (p.text >> 24) == 0xff && (p.accent >> 24) == 0xff,
            "palette colors are opaque");
        CHECK(p.text != p.panel && p.accent != p.surface, "palette keeps text and accent distinct");
    }
    CHECK(&UiTheme::palette(99) == &UiTheme::palette(UiTheme::PALETTE_DARK), "bad theme falls back to dark");
    CHECK((unsigned)UiTheme::withAlpha((int)0xff112233, 0x40) == 0x40112233u, "withAlpha swaps alpha");
    CHECK(UiTheme::gridMinorVisible(20.0f) && !UiTheme::gridMinorVisible(4.0f), "minor grid fades out");
    CHECK(UiTheme::clampSidebarWidth(150) == 200 && UiTheme::clampSidebarWidth(9999) == 460,
        "sidebar width clamps");

    CHECK(UiTheme::clampSliderMax(10, 30, 800) == 0, "short list clamps to 0");
    CHECK(UiTheme::clampSliderMax(100, 30, 800) == 2200, "tall list max");
    CHECK(UiTheme::clampSliderValue(-5, 100) == 0, "negative slider clamps");
    CHECK(UiTheme::clampSliderValue(500, 100) == 100, "overflow slider clamps");
    CHECK(UiTheme::clampSliderValue(50, 100) == 50, "in-range slider kept");

    CHECK(UiTheme::chipListIndex(100, 48, 0, 30, 10) == 1, "list index math");
    CHECK(UiTheme::chipListIndex(10, 48, 0, 30, 10) == -1, "above list is -1");
    CHECK(UiTheme::chipListIndex(10000, 48, 0, 30, 10) == -1, "below list is -1");
    CHECK(UiTheme::chipListIndex(100, 48, 0, 30, 0) == -1, "empty list is -1");

    CHECK(!UiTheme::validChipIndex(-1, 5), "negative chip invalid");
    CHECK(!UiTheme::validChipIndex(5, 5), "past-end chip invalid");
    CHECK(UiTheme::validChipIndex(4, 5), "last chip valid");
    CHECK(!UiTheme::validWireIndex(99, 3), "wire OOB invalid");

    int w = 100, h = 50;
    UiTheme::clampWindowSize(w, h);
    CHECK(w == 640 && h == 480, "tiny window clamped up");
    w = 5000; h = 5000;
    UiTheme::clampWindowSize(w, h);
    CHECK(w == 3840 && h == 2160, "huge window clamped down");

    CHECK(UiTheme::undoMaxEntries() == 100, "deep undo history kept");
    CHECK(UiTheme::undoMaxBytes() == 64u * 1024u * 1024u, "undo memory capped");

    // Top-bar/sidebar chrome is Dear ImGui windows now: buttons and rows
    // auto-size to their labels, so there is no manual layout math left
    // to pin here. Canvas helpers below are still hand-rolled and tested.

    CHECK(UiTheme::wirePickTolerance(20.0f, 0.05f) == 6.0f / 20.0f, "pick floors to 6px at default zoom");
    CHECK(UiTheme::wirePickTolerance(20.0f, 0.5f) == 0.5f, "large configured pick kept");
    CHECK(UiTheme::wirePickTolerance(200.0f, 0.12f) == 0.12f, "zoomed-in pick uses configured value");
    CHECK(UiTheme::wireEndTolerance(20.0f, 0.2f) == 8.0f / 20.0f, "end zone floors to 8px");
    CHECK(UiTheme::snapWorld(1.26f, true) == 1.5f, "snap rounds to halves");
    CHECK(UiTheme::snapWorld(1.26f, false) == 1.26f, "snap off keeps value");
    CHECK(UiTheme::anchorGrabPad(20.0f) == 0.5f, "anchor pad floors to half world unit");
    CHECK(UiTheme::anchorGrabPad(4.0f) == 7.0f / 4.0f, "anchor pad grows to 7px zoomed out");
    CHECK(UiTheme::anchorHotZone(0.0f, 0.0f) == 1, "pin center is wire zone");
    CHECK(UiTheme::anchorHotZone(0.39f, -0.39f) == 1, "inner square edge is wire zone");
    CHECK(UiTheme::anchorHotZone(0.41f, 0.0f) == 0, "outside square is move zone");
    CHECK(UiTheme::anchorHotZone(2.0f, 2.0f) == 0, "far corner is move zone");
    CHECK(UiTheme::pinGrabPad(20.0f) == 0.25f, "pin pad is 5px at default zoom");
    CHECK(UiTheme::pinGrabPad(4.0f) == 5.0f / 4.0f, "pin pad grows zoomed out");
    CHECK(UiTheme::pinGrabPad(200.0f) == 0.15f, "pin pad floors zoomed in");
    CHECK(UiTheme::shouldSaveNudge(1000, 0) == 1, "first nudge saves");
    CHECK(UiTheme::shouldSaveNudge(1200, 1000) == 0, "rapid nudges coalesce");
    CHECK(UiTheme::shouldSaveNudge(1600, 1000) == 1, "paused nudges save again");
    CHECK(UiTheme::wireFinishSnap(20.0f, 0.2f) == 12.0f / 20.0f, "finish snaps to 12px");
    CHECK(UiTheme::wireFinishSnap(20.0f, 0.8f) == 0.8f, "large end zone kept");

    if (failures == 0)
        printf("ALL UI THEME TESTS PASSED\n");
    return failures;
}
