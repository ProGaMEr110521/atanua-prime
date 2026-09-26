/*
Atanua Real-Time Logic Simulator - modern theme helpers.
Pure, window-system independent helpers behind the SDL/OpenGL UI.
*/
#ifndef UI_THEME_H
#define UI_THEME_H

#include <math.h>
#include <stddef.h>

namespace UiTheme {

// Design tokens. One palette per chrome theme (AppSettings::Theme order:
// dark, contrast, light), 0xAARRGGBB like the rest of the renderer. Chrome
// code reads colors only from here, never from literals, so a theme is a
// single table edit. Canvas colors come in dark and paper flavors because
// the canvas background is its own setting.
struct Palette
{
    // chrome surfaces, back to front
    unsigned chrome;      // header + status bar
    unsigned panel;       // library sidebar, modals
    unsigned surface;     // inputs, idle buttons
    unsigned surfaceHi;   // hover
    unsigned surfaceAct;  // pressed / selected
    unsigned border;
    unsigned borderHi;
    // type
    unsigned text;
    unsigned textDim;
    unsigned textFaint;
    // accent (selection, active toggles, focus)
    unsigned accent;
    unsigned accentHi;
    unsigned accentText;  // text drawn on an accent fill
    // semantic
    unsigned ok;
    unsigned danger;
    // canvas, dark flavor
    unsigned canvasDark;
    unsigned gridMinorDark;
    unsigned gridMajorDark;
    // canvas, paper flavor
    unsigned canvasPaper;
    unsigned gridMinorPaper;
    unsigned gridMajorPaper;
};

enum { PALETTE_DARK = 0, PALETTE_CONTRAST = 1, PALETTE_LIGHT = 2, PALETTE_COUNT = 3 };

inline const Palette &palette(int theme)
{
    static const Palette kPal[PALETTE_COUNT] = {
        // Graphite: neutral greys, warm amber accent (reads as "signal"
        // and never fights the green/red wire states on the canvas).
        { 0xff141619, 0xff191b1f, 0xff212429, 0xff2a2e34, 0xff333840,
          0xff26292e, 0xff3a3f47,
          0xffe6e7e9, 0xff9197a1, 0xff5f646d,
          0xffe8a33d, 0xfff3b75e, 0xff1b1406,
          0xff5cc98a, 0xffe5534b,
          0xff101114, 0xff171a1e, 0xff23272d,
          0xffe8e6e1, 0xffdcd9d2, 0xffc9c5bc },
        // Contrast: pure black and white, yellow accent, hard borders.
        { 0xff000000, 0xff050505, 0xff141414, 0xff262626, 0xff333333,
          0xff8a8a8a, 0xffffffff,
          0xffffffff, 0xffd6d6d6, 0xffa8a8a8,
          0xffffd400, 0xffffe45c, 0xff000000,
          0xff4cff8f, 0xffff5c5c,
          0xff000000, 0xff141414, 0xff3a3a3a,
          0xffffffff, 0xffd8d8d8, 0xff9a9a9a },
        // Light: warm paper chrome, burnt-orange accent for contrast.
        { 0xfff3f2ee, 0xfff8f7f4, 0xffffffff, 0xffeae8e2, 0xffdedbd3,
          0xffdedbd4, 0xffc4c0b6,
          0xff1c1d20, 0xff62666e, 0xff9a9ea6,
          0xffc2560f, 0xffd9691f, 0xffffffff,
          0xff1f8a4c, 0xffc62f28,
          0xff14161a, 0xff1b1e23, 0xff282c33,
          0xffebe9e4, 0xffdfdcd5, 0xffcbc7be },
    };
    if (theme < 0 || theme >= PALETTE_COUNT)
        theme = PALETTE_DARK;
    return kPal[theme];
}

// Replace the alpha byte of an 0xAARRGGBB color.
inline int withAlpha(int c, int a)
{
    if (a < 0) a = 0;
    if (a > 255) a = 255;
    return (int)(((unsigned)c & 0x00ffffffu) | ((unsigned)a << 24));
}

// Canvas grid: minor lines every world unit only once they are at least
// this many pixels apart, major lines every 10 units always.
inline int gridMinorVisible(float zoom)
{
    return zoom >= 9.0f ? 1 : 0;
}

// Wire half width in world units: the chip-art stroke (0.09) but never
// thinner on screen than 1.5 px.
inline float wireHalfWidth(float zoom)
{
    if (zoom < 0.01f) zoom = 0.01f;
    float w = 0.09f;
    if (w * zoom < 1.5f) w = 1.5f / zoom;
    return w * 0.5f;
}

// Ink for user text on the canvas (labels, readouts): near-white on the
// dark canvas, near-black on paper.
inline unsigned canvasInk(int darkCanvas)
{
    return darkCanvas ? 0xffdde1e7u : 0xff1f2126u;
}

// Spacing scale (px at 1.0 UI scale). Everything in the chrome snaps to
// these so paddings line up across header, sidebar and modals.
enum { SP_1 = 4, SP_2 = 8, SP_3 = 12, SP_4 = 16, SP_5 = 24 };

// Library sidebar width bounds (px); the user drags the splitter.
inline int clampSidebarWidth(int w)
{
    if (w < 200) return 200;
    if (w > 460) return 460;
    return w;
}

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

// Undo history bounds: keep plenty of steps; cap total snapshot bytes so
// huge designs cannot grow memory without bound. Trimming drops oldest first.
inline int undoMaxEntries()
{
    return 100;
}

inline size_t undoMaxBytes()
{
    return 64 * 1024 * 1024;
}

// Top-bar and sidebar chrome are Dear ImGui windows now: buttons and rows
// auto-size to their labels, so no manual layout math lives here anymore.

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

// Release-to-finish snap radius while routing: letting go near a pin
// finishes the wire there instead of stranding an anchor next to it.
inline float wireFinishSnap(float zoom, float endTol)
{
    if (zoom < 1.0f) zoom = 1.0f;
    float px = 12.0f / zoom;
    return endTol > px ? endTol : px;
}

// Keyboard-nudge undo coalescing: one undo step per burst of arrow moves.
inline int shouldSaveNudge(int nowTick, int lastTick)
{
    return (nowTick - lastTick > 500) ? 1 : 0;
}

} // namespace UiTheme

#endif
