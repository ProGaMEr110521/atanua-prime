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
        int btnRight = 5 * wide.tabW + wide.gapA + 5 * wide.btnW + wide.gapB + 2 * wide.btnW + wide.gapC + 5 * wide.btnW;
        CHECK(btnRight <= wide.quitX - 4, "wide buttons end before quit");
        UiTheme::TopbarLayout desk = UiTheme::topbarLayout(1280);
        int deskRight = 5 * desk.tabW + desk.gapA + 5 * desk.btnW + desk.gapB + 2 * desk.btnW + desk.gapC + 5 * desk.btnW;
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

    if (failures == 0)
        printf("ALL UI THEME TESTS PASSED\n");
    return failures;
}
