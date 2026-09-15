/*
Atanua Real-Time Logic Simulator - modern theme helpers.
Pure, window-system independent helpers behind the SDL/OpenGL UI.
*/
#ifndef UI_THEME_H
#define UI_THEME_H

#include <math.h>

// Modern dark chrome (default). Kept in one place so tests can pin it.
#define UI_THEME_MENUBG     0xff20242c
#define UI_THEME_MENULINE   0xff333947
#define UI_THEME_WIDGETBG   0xff2c313c
#define UI_THEME_WIDGETTHUMB 0xff59637a
#define UI_THEME_WIDGETHOT  0xff4c8dff
#define UI_THEME_TEXT       0xffeef1f6
#define UI_THEME_TEXTDIM    0xff8b93a7
#define UI_THEME_HOTROW     0xff31406b
#define UI_THEME_ACCENTTEXT 0xff7ddf8a

#define UI_THEME_TOPBAR_H 48
#define UI_THEME_TAB_W 68
#define UI_THEME_BTN_W 68
#define UI_THEME_ROW_H 30

namespace UiTheme {

inline int clampSliderMax(int count, int rowH, int viewH)
{
    int max = count * rowH - viewH;
    return max < 0 ? 0 : max;
}

inline int clampSliderValue(int value, int max)
{
    if (max < 0) max = 0;
    if (value < 0) return 0;
    if (value > max) return max;
    return value;
}

// Returns list index or -1 when outside. Mirrors draw_screen sidebar math.
inline int chipListIndex(int mousey, int topH, int slider, int rowH, int count)
{
    if (rowH <= 0 || count <= 0)
        return -1;
    int loc = (mousey - topH + slider) / rowH;
    if (loc < 0 || loc >= count)
        return -1;
    return loc;
}

inline bool validChipIndex(int id, int size)
{
    return id >= 0 && id < size;
}

inline bool validWireIndex(int id, int size)
{
    return id >= 0 && id < size;
}

inline void clampWindowSize(int &w, int &h)
{
    if (w < 640) w = 640;
    if (h < 480) h = 480;
    if (w > 3840) w = 3840;
    if (h > 2160) h = 2160;
}

inline int undoDepthForDesign(unsigned int chips, unsigned int wires)
{
    unsigned int total = chips + wires;
    if (total > 2000) return 10;
    if (total > 800) return 20;
    return 50;
}

struct TopbarLayout {
    int rows;
    int topH;
    int tabW;
    int btnW;
    int quitW;
    int gapA;
    int gapB;
    int gapC;
    int quitX;
    bool compactLabels;
};

inline TopbarLayout topbarLayout(int screenW)
{
    TopbarLayout L;
    const int fullTab = UI_THEME_TAB_W;
    const int fullBtn = 64;
    const int fullQuit = UI_THEME_BTN_W;
    const int fullGapA = 36;
    const int fullGapB = 20;
    const int fullGapC = 20;
    const int rightMargin = 6;
    const int quitGap = 8;
    int singleNeed = 5 * fullTab + 12 * fullBtn + fullQuit + fullGapA + fullGapB + fullGapC + rightMargin + quitGap;
    if (screenW >= singleNeed)
    {
        L.rows = 1;
        L.topH = UI_THEME_TOPBAR_H;
        L.tabW = fullTab;
        L.btnW = fullBtn;
        L.quitW = fullQuit;
        L.gapA = fullGapA;
        L.gapB = fullGapB;
        L.gapC = fullGapC;
        L.quitX = screenW - fullQuit - rightMargin;
        L.compactLabels = false;
        return L;
    }
    // Two rows: tabs + quit on row 0, 12 actions on row 1 shrunk to fit.
    L.rows = 2;
    L.topH = UI_THEME_TOPBAR_H * 2;
    L.tabW = fullTab;
    if (screenW < 5 * fullTab + fullQuit + rightMargin + quitGap + 40)
        L.tabW = 52;
    L.quitW = fullQuit;
    if (screenW < 700)
        L.quitW = 56;
    L.quitX = screenW - L.quitW - rightMargin;
    L.gapA = 12;
    L.gapB = 8;
    L.gapC = 8;
    int avail = screenW - L.gapA - L.gapB - L.gapC - 8;
    int w = avail / 12;
    if (w > fullBtn) w = fullBtn;
    if (w < 44) w = 44;
    L.btnW = w;
    L.compactLabels = (w < 60);
    return L;
}

inline int topbarHeight(int screenW)
{
    return topbarLayout(screenW).topH;
}

// Wire grabbing in world units, floored to a constant screen size so thin
// wires stay clickable at any zoom. 6px to grab, 8px end zones.
inline float wirePickTolerance(float zoom, float configured)
{
    if (zoom < 1.0f) zoom = 1.0f;
    float minTol = 6.0f / zoom;
    return configured > minTol ? configured : minTol;
}

inline float wireEndTolerance(float zoom, float configured)
{
    if (zoom < 1.0f) zoom = 1.0f;
    float minTol = 8.0f / zoom;
    return configured > minTol ? configured : minTol;
}

// Snap a world coordinate to the 0.5 placement grid used everywhere else.
inline float snapWorld(float v, bool snapOn)
{
    if (!snapOn) return v;
    return floorf(v * 2.0f + 0.5f) / 2.0f;
}

// Grab padding (world units, per side) for tiny anchor chips so a 1x1 bend
// point stays clickable: at least half a world unit, at least ~7 screen px.
inline float anchorGrabPad(float zoom)
{
    if (zoom < 1.0f) zoom = 1.0f;
    float px = 7.0f / zoom;
    return px > 0.5f ? px : 0.5f;
}

// Anchor hover zones, measured from the pin center: 1 = inner wire-start
// square (0.8 x 0.8 around the pin), 0 = outer move area.
inline int anchorHotZone(float dx, float dy)
{
    const float h = 0.4f;
    return (dx >= -h && dx <= h && dy >= -h && dy <= h) ? 1 : 0;
}

// Pin connection zone (world units, per side) so grabbing a pin to start a
// wire beats nearby wires: the 0.5 pin box grows to ~5 screen px each side.
inline float pinGrabPad(float zoom)
{
    if (zoom < 1.0f) zoom = 1.0f;
    float px = 5.0f / zoom;
    return px > 0.15f ? px : 0.15f;
}

} // namespace UiTheme

#endif
