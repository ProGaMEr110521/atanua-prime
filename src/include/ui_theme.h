/*
Atanua Real-Time Logic Simulator - modern theme helpers.
Pure, window-system independent helpers behind the SDL/OpenGL UI.
*/
#ifndef UI_THEME_H
#define UI_THEME_H

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

} // namespace UiTheme

#endif
