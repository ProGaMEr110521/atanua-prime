#include <cstdio>
#include "ui_theme.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } else { printf("PASS: %s\n", msg); } } while (0)

int main()
{
    CHECK(UI_THEME_MENUBG == 0xff20242c, "menubg is modern dark");
    CHECK(UI_THEME_TOPBAR_H == 48, "topbar height 48");
    CHECK(UI_THEME_TAB_W == 68 && UI_THEME_BTN_W == 68, "tab/btn widths 68");
    CHECK(UI_THEME_ROW_H == 30, "row height 30");

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

    CHECK(UiTheme::undoDepthForDesign(10, 10) == 50, "small design full undo");
    CHECK(UiTheme::undoDepthForDesign(500, 400) == 20, "medium design reduced undo");
    CHECK(UiTheme::undoDepthForDesign(1500, 1000) == 10, "heavy design minimal undo");

    {
        UiTheme::TopbarLayout wide = UiTheme::topbarLayout(1920);
        CHECK(wide.rows == 1, "wide screen single row");
        CHECK(wide.tabW == 68, "wide tabs full width");
        int newX = 5 * wide.tabW + wide.gapA;
        CHECK(newX == 5 * 68 + 36, "new button starts after full misc tab plus gap");
        int btnRight = newX + 12 * wide.btnW + wide.gapB + wide.gapC;
        CHECK(btnRight <= wide.quitX - 4, "wide buttons end before quit");
        UiTheme::TopbarLayout desk = UiTheme::topbarLayout(1280);
        int deskNewX = 5 * desk.tabW + desk.gapA;
        int deskRight = deskNewX + 12 * desk.btnW + desk.gapB + desk.gapC;
        if (desk.rows == 1)
            CHECK(deskRight <= desk.quitX - 4, "desktop buttons end before quit");
        else
            CHECK(12 * desk.btnW + desk.gapA + desk.gapB + desk.gapC <= 1280, "wrapped actions fit desktop");
        UiTheme::TopbarLayout small = UiTheme::topbarLayout(640);
        CHECK(small.rows == 2, "small screen wraps to two rows");
        CHECK(12 * small.btnW + small.gapA + small.gapB + small.gapC <= 640, "wrapped actions fit small");
        CHECK(small.quitX + small.quitW <= 640, "quit inside small screen");
        CHECK(UiTheme::topbarHeight(1920) == 48, "wide topbar height 48");
        CHECK(UiTheme::topbarHeight(640) == 96, "small topbar height 96");
    }

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

    if (failures == 0)
        printf("ALL UI THEME TESTS PASSED\n");
    return failures;
}
