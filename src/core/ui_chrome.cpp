/*
Atanua Prime - application chrome (Dear ImGui).

Layout, top to bottom:
  header    brand mark, File/Edit/View/Help menus, document name, quick actions
  library   left panel: search, category pills, component list (drag source)
  canvas    drawn by main.cpp between the library and the right window edge
  status    simulation state, counts, selection, zoom, snap/wire toggles

Every color comes from UiTheme::palette(), every spacing from the SP_*
scale, and icons are drawn as vectors so they stay crisp at any UI scale.
*/
#include "atanua.h"
#include "atanua_internal.h"
#include "ui_theme.h"
#include "ui_chrome.h"
#include "app_settings.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_opengl2.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

int gTopbarH = 40;
int gStatusH = 26;
int gSettingsOpen = 0;
int gShortcutsOpen = 0;
int gAboutOpen = 0;

extern vector<char *> gAvailableChip[5];
extern vector<ChipFactory *> gChipFactory;
extern vector<Chip *> gChip;
extern vector<Wire *> gWire;
extern vector<Net *> gNet;
extern vector<File *> gUndoStack;
extern vector<File *> gRedoStack;
extern vector<Chip *> gMultiSelectChip;
extern vector<Wire *> gMultiSelectWire;
extern int gVisibleChiplist;
extern int gSnap;
extern int gLiveWires;
extern int gSavePNG;
extern int gCloneKeyMask;
extern char *gFilename;
extern void storefilename(const char *fn);
extern void do_rotate();

namespace {

using UiTheme::Palette;

ImFont *sFontUI = NULL;      // Inter Regular (+ DejaVu fallback glyphs)
ImFont *sFontStrong = NULL;  // Inter SemiBold (+ DejaVu fallback glyphs)
ImFont *sFontMono = NULL;    // JetBrains Mono: readouts and key caps
int sFocusSearch = 0;
int sPaletteOpen = 0;
// Recently picked library entries, newest first, as (list << 16) | index.
int sRecent[6];
int sRecentCount = 0;
int sSplitterDrag = 0;
char sFilter[64] = { 0 };
char sUserBuf[64];
int sUserBufInit = 0;

const float kBaseFont = 15.0f;

int lang()
{
    return AppSettings::clampLang(gConfig.mLanguage);
}

const char *T(int key)
{
    return AppSettings::text(key, lang(), 0);
}

const char *TS(int key)
{
    return AppSettings::text(key, lang(), 1);
}

const Palette &pal()
{
    return UiTheme::palette(AppSettings::clampTheme(gConfig.mThemeVariant));
}

ImU32 u32(int c)
{
    return IM_COL32((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff, (c >> 24) & 0xff);
}

ImVec4 v4(int c)
{
    return ImGui::ColorConvertU32ToFloat4(u32(c));
}

float px(float v)
{
    return v * AppSettings::clampUiScale(gConfig.mUiScale);
}

void pushStrong(float size = 0.0f)
{
    ImGui::PushFont(sFontStrong ? sFontStrong : sFontUI, size);
}

// Uppercase for Latin and Russian Cyrillic (the two UI languages).
void upperUtf8(const char *in, char *out, int cap)
{
    int o = 0;
    const unsigned char *s = (const unsigned char *)in;
    while (*s && o < cap - 3)
    {
        if (*s < 0x80)
        {
            out[o++] = (char)((*s >= 'a' && *s <= 'z') ? *s - 32 : *s);
            s++;
            continue;
        }
        if ((s[0] & 0xE0) == 0xC0 && s[1])
        {
            int cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
            if (cp >= 0x430 && cp <= 0x44F)
                cp -= 0x20;
            else if (cp == 0x451)
                cp = 0x401;
            out[o++] = (char)(0xC0 | (cp >> 6));
            out[o++] = (char)(0x80 | (cp & 0x3F));
            s += 2;
            continue;
        }
        out[o++] = (char)*s++;
    }
    out[o] = 0;
}

// Section label: small uppercase semibold with open tracking, the one
// place the chrome uses letter-spacing. Advances the layout like Text().
void trackedLabel(const char *label, int color)
{
    char up[128];
    upperUtf8(label, up, sizeof(up));
    ImGui::PushFont(sFontStrong ? sFontStrong : sFontUI, kBaseFont * 0.76f);
    float track = ImMax(1.0f, ImGui::GetFontSize() * 0.08f);
    ImVec2 a = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    float x = a.x;
    const char *c = up;
    while (*c)
    {
        const char *e = c + 1;
        while (*e && ((unsigned char)*e & 0xC0) == 0x80)
            e++;
        dl->AddText(ImVec2(x, a.y), u32(color), c, e);
        x += ImGui::CalcTextSize(c, e).x + track;
        c = e;
    }
    ImGui::Dummy(ImVec2(x - a.x - track, ImGui::GetFontSize()));
    ImGui::PopFont();
}

// JetBrains Mono's ascent+descent is taller than Inter's, and Dear ImGui
// sizes fonts by that line height, so match the visual size explicitly.
void pushMono(float size = 0.0f)
{
    if (size <= 0.0f)
        size = ImGui::GetStyle().FontSizeBase;
    ImGui::PushFont(sFontMono ? sFontMono : sFontUI, sFontMono ? size * 1.1f : size);
}

void popFont()
{
    ImGui::PopFont();
}

//////////////////////////////////////////////////////////////////////////
// Vector icons. c = center, s = half extent, drawn with the text color.

enum Icon
{
    IC_UNDO, IC_REDO, IC_FIT, IC_SNAP, IC_WIRES, IC_SETTINGS, IC_HELP,
    IC_SEARCH, IC_CLOSE
};

void drawIcon(ImDrawList *dl, int ic, ImVec2 c, float s, ImU32 col, ImU32 bg)
{
    float t = ImMax(1.25f, s * 0.16f);
    switch (ic)
    {
    case IC_UNDO:
    case IC_REDO:
    {
        float dir = ic == IC_UNDO ? 1.0f : -1.0f;
        float r = s * 0.62f;
        ImVec2 ctr(c.x + dir * s * 0.12f, c.y + s * 0.12f);
        // Hook: from lower right, over the top, down at the left end.
        const int N = 16;
        float a0 = 0.30f * IM_PI, a1 = -IM_PI;
        for (int i = 0; i <= N; i++)
        {
            float a = a0 + (a1 - a0) * i / N;
            dl->PathLineTo(ImVec2(ctr.x + dir * cosf(a) * r, ctr.y + sinf(a) * r));
        }
        dl->PathStroke(col, 0, t);
        ImVec2 tip(ctr.x - dir * r, ctr.y + s * 0.42f);
        dl->AddTriangleFilled(tip,
            ImVec2(ctr.x - dir * r - s * 0.34f, ctr.y - s * 0.02f),
            ImVec2(ctr.x - dir * r + s * 0.34f, ctr.y - s * 0.02f), col);
        break;
    }
    case IC_FIT:
    {
        float h = s * 0.78f, l = s * 0.38f;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
            {
                ImVec2 k(c.x + sx * h, c.y + sy * h);
                dl->AddLine(k, ImVec2(k.x - sx * l, k.y), col, t);
                dl->AddLine(k, ImVec2(k.x, k.y - sy * l), col, t);
            }
        dl->AddRectFilled(ImVec2(c.x - s * 0.2f, c.y - s * 0.2f),
            ImVec2(c.x + s * 0.2f, c.y + s * 0.2f), col, s * 0.05f);
        break;
    }
    case IC_SNAP:
    {
        float d = s * 0.62f, r = ImMax(1.2f, s * 0.12f);
        for (int x = -1; x <= 1; x++)
            for (int y = -1; y <= 1; y++)
                dl->AddCircleFilled(ImVec2(c.x + x * d, c.y + y * d), r, col, 8);
        break;
    }
    case IC_WIRES:
    {
        const float pts[8][2] = {
            { -0.85f, 0.4f }, { -0.4f, 0.4f }, { -0.4f, -0.4f }, { 0.05f, -0.4f },
            { 0.05f, 0.4f }, { 0.5f, 0.4f }, { 0.5f, -0.4f }, { 0.85f, -0.4f } };
        for (int i = 0; i < 8; i++)
            dl->PathLineTo(ImVec2(c.x + pts[i][0] * s, c.y + pts[i][1] * s));
        dl->PathStroke(col, 0, t);
        break;
    }
    case IC_SETTINGS:
    {
        // Three sliders: the honest picture of what the dialog holds.
        const float ky[3] = { -0.55f, 0.0f, 0.55f };
        const float kx[3] = { -0.3f, 0.38f, -0.05f };
        for (int i = 0; i < 3; i++)
        {
            float y = c.y + ky[i] * s;
            dl->AddLine(ImVec2(c.x - 0.85f * s, y), ImVec2(c.x + 0.85f * s, y), col, t);
            ImVec2 k(c.x + kx[i] * s, y);
            dl->AddCircleFilled(k, s * 0.24f, bg, 12);
            dl->AddCircle(k, s * 0.24f, col, 12, t);
        }
        break;
    }
    case IC_HELP:
    {
        dl->AddCircle(c, s * 0.85f, col, 20, t);
        ImVec2 ts = ImGui::CalcTextSize("?");
        dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), col, "?");
        break;
    }
    case IC_SEARCH:
    {
        ImVec2 o(c.x - s * 0.15f, c.y - s * 0.15f);
        dl->AddCircle(o, s * 0.5f, col, 16, t);
        dl->AddLine(ImVec2(o.x + s * 0.36f, o.y + s * 0.36f),
            ImVec2(c.x + s * 0.75f, c.y + s * 0.75f), col, t);
        break;
    }
    case IC_CLOSE:
    {
        float h = s * 0.55f;
        dl->AddLine(ImVec2(c.x - h, c.y - h), ImVec2(c.x + h, c.y + h), col, t);
        dl->AddLine(ImVec2(c.x - h, c.y + h), ImVec2(c.x + h, c.y - h), col, t);
        break;
    }
    }
}

// Brand mark: the "At" monogram from the app icon (atanua.png), redrawn
// as vectors so it stays crisp at any UI scale. Geometry is in the icon's
// 256-unit grid; h is the drawn height of the letters. Returns the width.
float drawBrandMark(ImDrawList *dl, ImVec2 p, float h)
{
    const ImU32 cA = IM_COL32(0x5a, 0x84, 0xc3, 0xff);
    const ImU32 cT = IM_COL32(0x2a, 0x5c, 0x88, 0xff);
    const float k = h / 144.0f;
    auto R = [&](float x0, float y0, float x1, float y1, ImU32 c) {
        dl->AddRectFilled(ImVec2(ImFloor(p.x + (x0 - 43) * k), ImFloor(p.y + (y0 - 73) * k)),
            ImVec2(ImFloor(p.x + (x1 - 43) * k), ImFloor(p.y + (y1 - 73) * k)), c);
    };
    // A: solid block minus a counter and the leg gap
    R(43, 73, 90, 217, cA);
    R(106, 73, 153, 217, cA);
    R(90, 73, 106, 106, cA);
    R(90, 144, 106, 178, cA);
    // T: bar and stem
    R(156, 138, 216, 159, cT);
    R(173, 159, 199, 217, cT);
    return (216 - 43) * k;
}

// The full app tile (dark rounded square) for the About dialog.
void drawBrandTile(ImDrawList *dl, ImVec2 p, float size)
{
    float r = size * 0.14f;
    dl->AddRectFilled(p, ImVec2(p.x + size, p.y + size), IM_COL32(0x15, 0x19, 0x1c, 0xff), r);
    dl->AddRect(p, ImVec2(p.x + size, p.y + size), IM_COL32(0x33, 0x39, 0x47, 0xff), r, 0, ImMax(1.0f, size / 96.0f));
    float h = size * 144.0f / 256.0f;
    drawBrandMark(dl, ImVec2(p.x + size * 43.0f / 256.0f, p.y + size * 73.0f / 256.0f), h);
}

void tooltipWithKeys(const char *tip, const char *keys)
{
    if (!ImGui::BeginTooltip())
        return;
    ImGui::TextUnformatted(tip);
    if (keys && keys[0])
    {
        ImGui::SameLine(0, px(12));
        ImGui::TextColored(v4(pal().textDim), "%s", keys);
    }
    ImGui::EndTooltip();
}

bool iconButton(const char *id, int icon, bool active, const char *tip,
    const char *keys, bool enabled, int bg, float sz = 0.0f)
{
    const Palette &p = pal();
    if (sz <= 0.0f)
        sz = ImGui::GetFrameHeight();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    if (!enabled)
        ImGui::BeginDisabled();
    bool hit = ImGui::InvisibleButton(id, ImVec2(sz, sz));
    bool hov = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    bool held = ImGui::IsItemActive();
    if (!enabled)
        ImGui::EndDisabled();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    int fill = 0;
    if (held)
        fill = p.surfaceAct;
    else if (hov && enabled)
        fill = p.surfaceHi;
    if (fill)
        dl->AddRectFilled(pos, ImVec2(pos.x + sz, pos.y + sz), u32(fill), ImGui::GetStyle().FrameRounding);
    int fg = !enabled ? p.textFaint : active ? p.accent : hov ? p.text : p.textDim;
    drawIcon(dl, icon, ImVec2(pos.x + sz * 0.5f, pos.y + sz * 0.5f), sz * 0.3f, u32(fg),
        u32(fill ? fill : bg));
    if (hov && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
        tooltipWithKeys(tip, keys);
    return hit && enabled;
}

// Segmented control. Returns the chosen index (== sel when untouched).
int segmented(const char *id, const char *const *labels, int n, int sel)
{
    const Palette &p = pal();
    ImGuiStyle &st = ImGui::GetStyle();
    float padX = px(12);
    float segW = 0;
    for (int i = 0; i < n; i++)
        segW = ImMax(segW, ImGui::CalcTextSize(labels[i]).x + padX * 2);
    float h = ImGui::GetFrameHeight();
    ImVec2 o = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    float inset = px(2);
    dl->AddRectFilled(o, ImVec2(o.x + segW * n + inset * 2, o.y + h), u32(p.surface), st.FrameRounding);
    int out = sel;
    ImGui::PushID(id);
    for (int i = 0; i < n; i++)
    {
        ImVec2 a(o.x + inset + segW * i, o.y + inset);
        ImVec2 b(a.x + segW, o.y + h - inset);
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##seg", ImVec2(segW, b.y - a.y)))
            out = i;
        bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        if (i == sel)
        {
            dl->AddRectFilled(a, b, u32(p.surfaceAct), st.FrameRounding - inset);
            dl->AddRect(a, b, u32(p.borderHi), st.FrameRounding - inset, 0, 1.0f);
        }
        else if (hov)
            dl->AddRectFilled(a, b, u32(p.surfaceHi), st.FrameRounding - inset);
        ImVec2 ts = ImGui::CalcTextSize(labels[i]);
        dl->AddText(ImVec2(a.x + (segW - ts.x) * 0.5f, a.y + (b.y - a.y - ts.y) * 0.5f),
            u32(i == sel ? p.text : p.textDim), labels[i]);
    }
    ImGui::PopID();
    ImGui::SetCursorScreenPos(o);
    ImGui::Dummy(ImVec2(segW * n + inset * 2, h));
    return out;
}

//////////////////////////////////////////////////////////////////////////
// Header

void doQuit()
{
    if (okcancel(T(AppSettings::S_CONFIRM_EXIT)))
        exit(0);
}

void toggleLiveWires()
{
    gLiveWires = !gLiveWires;
    gConfig.mLiveWires = gLiveWires ? 1 : 0;
    gConfig.save();
}

void setCanvasDark(int dark)
{
    gBlackBackground = dark ? 1 : 0;
    gConfig.mCanvasDark = gBlackBackground;
    gConfig.save();
}

void undoKeepFocus(int redo)
{
    int active = gUIState.kbditem;
    if (redo)
        do_redo();
    else
        do_undo();
    gUIState.kbditem = active;
}

const char *docName()
{
    if (!gFilename || !gFilename[0])
        return T(AppSettings::S_UNTITLED);
    const char *b = gFilename;
    for (const char *c = gFilename; *c; c++)
        if (*c == '/' || *c == '\\')
            b = c + 1;
    return b;
}

void menuFile()
{
    if (!ImGui::BeginMenu(T(AppSettings::S_MENU_FILE)))
        return;
    if (ImGui::MenuItem(TS(AppSettings::S_NEW), "Ctrl+N"))
        do_resetdialog();
    if (ImGui::MenuItem(T(AppSettings::S_OPEN_ITEM), "Ctrl+L"))
        do_loaddialog();
    if (ImGui::MenuItem(T(AppSettings::S_MERGE_ITEM), "Ctrl+M"))
        do_loaddialog(1);
    if (ImGui::MenuItem(T(AppSettings::S_BOX_ITEM), "Ctrl+B"))
        do_loaddialog(2);
    ImGui::Separator();
    if (ImGui::MenuItem(T(AppSettings::S_SAVE_ITEM), "Ctrl+S"))
        do_savedialog();
    if (ImGui::MenuItem(T(AppSettings::S_PNG_ITEM), "Ctrl+G"))
        gSavePNG = 3; // a few frames later, so the menu is gone from the shot
    ImGui::Separator();
    if (ImGui::MenuItem(T(AppSettings::S_SETTINGS)))
        gSettingsOpen = 1;
    ImGui::Separator();
    if (ImGui::MenuItem(T(AppSettings::S_QUIT)))
        doQuit();
    ImGui::EndMenu();
}

void menuEdit()
{
    if (!ImGui::BeginMenu(T(AppSettings::S_MENU_EDIT)))
        return;
    if (ImGui::MenuItem(TS(AppSettings::S_UNDO), "Ctrl+Z", false, !gUndoStack.empty()))
        undoKeepFocus(0);
    if (ImGui::MenuItem(TS(AppSettings::S_REDO), "Ctrl+Y", false, !gRedoStack.empty()))
        undoKeepFocus(1);
    ImGui::Separator();
    bool haveSel = gUIState.kbditem != 0 || !gMultiSelectChip.empty() || !gMultiSelectWire.empty();
    if (ImGui::MenuItem(T(AppSettings::S_SELECT_ALL), "Ctrl+A", false, !gChip.empty()))
        UiChrome::selectAll();
    ImGui::Separator();
    if (ImGui::MenuItem(T(AppSettings::S_ROTATE), "Ctrl+R", false, IS_CHIP_ID(gUIState.kbditem)))
        do_rotate();
    if (ImGui::MenuItem(T(AppSettings::S_DELETE), "Del", false, haveSel))
        gUIState.keyentered = SDLK_DELETE; // handled by the canvas key path this frame
    ImGui::Separator();
    if (ImGui::MenuItem(T(AppSettings::S_OPTIMIZE), "Ctrl+O"))
    {
        save_undo();
        do_optimize_box(0);
    }
    ImGui::EndMenu();
}

void menuView()
{
    if (!ImGui::BeginMenu(T(AppSettings::S_MENU_VIEW)))
        return;
    if (ImGui::MenuItem(T(AppSettings::S_ZOOM_IN), "PgUp"))
        UiChrome::zoomBy(1.2f);
    if (ImGui::MenuItem(T(AppSettings::S_ZOOM_OUT), "PgDn"))
        UiChrome::zoomBy(1.0f / 1.2f);
    if (ImGui::MenuItem(T(AppSettings::S_FIT_ITEM), "Ctrl+E"))
        do_zoomext();
    if (ImGui::MenuItem(T(AppSettings::S_HOME_ITEM), "Ctrl+H"))
        do_home();
    ImGui::Separator();
    if (ImGui::MenuItem(T(AppSettings::S_SNAP_ITEM), "Ctrl+P", gSnap != 0))
        gSnap = !gSnap;
    if (ImGui::MenuItem(T(AppSettings::S_LIVE_ITEM), "Ctrl+W", gLiveWires != 0))
        toggleLiveWires();
    if (ImGui::BeginMenu(T(AppSettings::S_CANVAS)))
    {
        if (ImGui::MenuItem(T(AppSettings::S_THEME_DARK), NULL, gBlackBackground != 0))
            setCanvasDark(1);
        if (ImGui::MenuItem(T(AppSettings::S_PAPER), NULL, gBlackBackground == 0))
            setCanvasDark(0);
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem(T(AppSettings::S_PERF_ITEM), NULL, gConfig.mPerformanceIndicators != 0))
        gConfig.mPerformanceIndicators = !gConfig.mPerformanceIndicators;
    ImGui::EndMenu();
}

void menuHelp()
{
    if (!ImGui::BeginMenu(T(AppSettings::S_MENU_HELP)))
        return;
    if (ImGui::MenuItem(T(AppSettings::S_PALETTE), "Ctrl+K"))
        sPaletteOpen = 1;
    if (ImGui::MenuItem(T(AppSettings::S_SHORTCUTS), "F1"))
        gShortcutsOpen = 1;
    if (ImGui::MenuItem(T(AppSettings::S_REPORT_ISSUE)))
        ImGui::GetPlatformIO().Platform_OpenInShellFn(ImGui::GetCurrentContext(),
            "https://github.com/ProGaMEr110521/atanua-prime/issues");
    ImGui::Separator();
    if (ImGui::MenuItem(T(AppSettings::S_ABOUT)))
        gAboutOpen = 1;
    ImGui::EndMenu();
}

void drawHeader()
{
    const Palette &p = pal();
    ImGuiStyle &st = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(px(10), px(11)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(px(12), st.ItemSpacing.y));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, v4(p.chrome));
    ImGui::PushStyleColor(ImGuiCol_Header, v4(p.surfaceAct));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, v4(p.surfaceHi));
    if (!ImGui::BeginMainMenuBar())
    {
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        return;
    }
    ImGuiWindow *win = ImGui::GetCurrentWindow();
    gTopbarH = (int)win->Size.y;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    float barH = win->Size.y;
    float W = win->Size.x;

    // Brand
    ImGui::SetCursorPosX(px(14));
    ImVec2 cur = ImGui::GetCursorScreenPos();
    float markH = ImFloor(ImGui::GetFontSize() * 0.95f);
    float markW = drawBrandMark(dl, ImVec2(cur.x, win->Pos.y + ImFloor((barH - markH) * 0.5f)), markH);
    ImGui::Dummy(ImVec2(markW + px(8), 1));
    ImGui::SameLine(0, 0);
    pushStrong();
    ImGui::TextUnformatted("Atanua");
    popFont();
    ImGui::SameLine(0, px(18));

    menuFile();
    menuEdit();
    menuView();
    menuHelp();
    float menusEnd = ImGui::GetCursorScreenPos().x;

    // Quick actions, right aligned: 7 square buttons, 3 dividers.
    float btn = ImFloor(px(28));
    float gap = px(2);
    float sepW = px(13);
    float rightW = 7 * btn + 3 * gap + 3 * sepW + px(10);
    float rightX = W - rightW;

    // Document name, centered when it fits between menus and actions
    {
        const char *name = docName();
        ImVec2 ts = ImGui::CalcTextSize(name);
        float x = win->Pos.x + (W - ts.x) * 0.5f;
        if (x < menusEnd + px(24))
            x = menusEnd + px(24);
        if (x + ts.x < win->Pos.x + rightX - px(24))
            dl->AddText(ImVec2(x, win->Pos.y + (barH - ts.y) * 0.5f),
                u32(gFilename ? p.text : p.textDim), name);
    }

    float by = win->Pos.y + ImFloor((barH - btn) * 0.5f);
    float bx = win->Pos.x + rightX;
    struct Slot { const char *id; int icon; int active; const char *tip; const char *keys; int enabled; int sepAfter; };
    const Slot slots[7] = {
        { "##palette", IC_SEARCH, sPaletteOpen != 0, T(AppSettings::S_PALETTE), "Ctrl+K", 1, 1 },
        { "##undo", IC_UNDO, 0, TS(AppSettings::S_UNDO), "Ctrl+Z", !gUndoStack.empty(), 0 },
        { "##redo", IC_REDO, 0, TS(AppSettings::S_REDO), "Ctrl+Y", !gRedoStack.empty(), 1 },
        { "##fit", IC_FIT, 0, T(AppSettings::S_FIT_ITEM), "Ctrl+E", 1, 0 },
        { "##snap", IC_SNAP, gSnap != 0, T(AppSettings::S_SNAP_ITEM), "Ctrl+P", 1, 0 },
        { "##wires", IC_WIRES, gLiveWires != 0, T(AppSettings::S_LIVE_ITEM), "Ctrl+W", 1, 1 },
        { "##settings", IC_SETTINGS, gSettingsOpen != 0, T(AppSettings::S_SETTINGS), NULL, 1, 0 },
    };
    for (int i = 0; i < 7; i++)
    {
        ImGui::SetCursorScreenPos(ImVec2(bx, by));
        if (iconButton(slots[i].id, slots[i].icon, slots[i].active != 0, slots[i].tip,
            slots[i].keys, slots[i].enabled != 0, p.chrome, btn))
        {
            switch (i)
            {
            case 0: sPaletteOpen = !sPaletteOpen; break;
            case 1: undoKeepFocus(0); break;
            case 2: undoKeepFocus(1); break;
            case 3: do_zoomext(); break;
            case 4: gSnap = !gSnap; break;
            case 5: toggleLiveWires(); break;
            case 6: gSettingsOpen = !gSettingsOpen; break;
            }
        }
        bx += btn + gap;
        if (slots[i].sepAfter)
        {
            float sx = ImFloor(bx + (sepW - gap) * 0.5f) + 0.5f;
            dl->AddLine(ImVec2(sx, by + btn * 0.22f), ImVec2(sx, by + btn * 0.78f), u32(p.border), 1.0f);
            bx += sepW - gap;
        }
    }

    // Hairline under the header
    ImGui::GetForegroundDrawList()->AddLine(ImVec2(win->Pos.x, win->Pos.y + barH - 0.5f),
        ImVec2(win->Pos.x + W, win->Pos.y + barH - 0.5f), u32(p.border), 1.0f);

    ImGui::EndMainMenuBar();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

//////////////////////////////////////////////////////////////////////////
// Library

int asciiLower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

int nameMatches(const char *name, const char *filter)
{
    if (!filter || !filter[0])
        return 1;
    if (!name)
        return 0;
    for (const char *s = name; *s; s++)
    {
        const char *a = s;
        const char *b = filter;
        while (*a && *b && asciiLower(*a) == asciiLower(*b))
        {
            a++;
            b++;
        }
        if (!*b)
            return 1;
    }
    return 0;
}

// Legacy hidden slot in the Out list (kept for file compatibility, never
// meant to be picked from the palette).
int hiddenSlot(int list, int i)
{
    return list == 3 && i == 8;
}

const int kCatKey[5] = { AppSettings::S_BASE, AppSettings::S_CHIPS,
    AppSettings::S_IN, AppSettings::S_OUT, AppSettings::S_MISC };

void startChipDrag(int list, int i)
{
    if (gDragMode == DRAGMODE_NEWCHIP)
    {
        do_cancel();
        return;
    }
    if (gDragMode != DRAGMODE_NONE)
        return;
    gMultiSelectChip.clear();
    gMultiSelectWire.clear();
    gMultiselectDirty = 1;
    for (int j = 0; gNewChip == NULL && j < (signed)gChipFactory.size(); j++)
        gNewChip = gChipFactory[j]->build(gAvailableChip[list][i]);
    if (gNewChip)
    {
        gNewChipName = gAvailableChip[list][i];
        gDragMode = DRAGMODE_NEWCHIP;
        gUIState.mousedownkeymod &= ~gCloneKeyMask;
        int key = (list << 16) | i;
        int n = 0;
        int keep[6];
        keep[n++] = key;
        for (int r = 0; r < sRecentCount && n < 6; r++)
            if (sRecent[r] != key)
                keep[n++] = sRecent[r];
        memcpy(sRecent, keep, sizeof(int) * n);
        sRecentCount = n;
    }
}

// Tooltip text for a palette entry: the chip's own description, built once
// per hovered entry (chip construction is not free).
const char *chipTooltip(int list, int i)
{
    static int sKey = -1;
    static char *sText = NULL;
    int key = (list << 16) | i;
    if (key != sKey)
    {
        sKey = key;
        delete[] sText;
        sText = NULL;
        Chip *c = NULL;
        for (int j = 0; c == NULL && j < (signed)gChipFactory.size(); j++)
            c = gChipFactory[j]->build(gAvailableChip[list][i]);
        if (c)
        {
            sText = mystrdup(c->mTooltip ? c->mTooltip : gAvailableChip[list][i]);
            delete c;
        }
    }
    return sText;
}

// salt keeps IDs unique when an entry is listed twice (Recent + its list).
void libraryRow(int list, int i, float rowH, int salt = 0)
{
    const Palette &p = pal();
    const char *name = gAvailableChip[list][i];
    ImGui::PushID(salt * 0x100000 + list * 4096 + i);
    ImVec2 a = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("##row", ImVec2(w, rowH));
    bool hov = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    bool dragging = gDragMode == DRAGMODE_NEWCHIP && gNewChipName == name;
    ImGui::PopID();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 b(a.x + w, a.y + rowH);
    if (dragging)
        dl->AddRectFilled(a, b, u32(UiTheme::withAlpha(p.accent, 0x26)), ImGui::GetStyle().FrameRounding);
    else if (hov)
        dl->AddRectFilled(a, b, u32(p.surfaceHi), ImGui::GetStyle().FrameRounding);
    ImVec2 ts = ImGui::CalcTextSize(name);
    dl->AddText(ImVec2(a.x + px(10), a.y + (rowH - ts.y) * 0.5f),
        u32(dragging ? p.accent : hov ? p.text : UiTheme::withAlpha(p.text, 0xd8)), name);
    if (!hov)
        return;
    gUIState.hotitem = NEWCHIP_ID(i);
    if (gUIState.mousedown && gUIState.activeitem == 0)
    {
        startChipDrag(list, i);
        gUIState.activeitem = gUIState.hotitem;
    }
    if (gConfig.mTooltipDelay > 0 && gDragMode == DRAGMODE_NONE &&
        ImGui::GetCurrentContext()->HoveredIdTimer * 1000.0f > gConfig.mTooltipDelay)
    {
        const char *tip = chipTooltip(list, i);
        if (ImGui::BeginTooltip())
        {
            if (tip)
                ImGui::TextUnformatted(tip);
            ImGui::TextColored(v4(p.textDim), "%s", T(AppSettings::S_DRAG_HINT));
            ImGui::EndTooltip();
        }
    }
}

void categoryHeading(const char *label)
{
    ImGui::Dummy(ImVec2(1, px(4)));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + px(10));
    trackedLabel(label, pal().textFaint);
}

// Category tabs: plain labels, the current one in full text color with an
// accent underline sitting on the hairline below. Wraps when a language
// runs long; the underline always rides the row it is on.
void categoryTabs()
{
    const Palette &p = pal();
    float avail = ImGui::GetContentRegionAvail().x;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float rowH = ImGui::GetFontSize() + px(14);
    float gap = px(16);
    float lx = 0, ly = 0;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    for (int t = 0; t < 5; t++)
    {
        const char *label = T(kCatKey[t]);
        float w = ImGui::CalcTextSize(label).x;
        if (lx > 0 && lx + w > avail)
        {
            lx = 0;
            ly += rowH;
        }
        ImVec2 a(origin.x + lx, origin.y + ly);
        ImGui::SetCursorScreenPos(ImVec2(a.x - gap * 0.5f, a.y));
        ImGui::PushID(t);
        if (ImGui::InvisibleButton("##cat", ImVec2(w + gap, rowH)))
        {
            int active = gUIState.kbditem;
            do_cancel();
            gUIState.kbditem = active;
            gVisibleChiplist = t;
        }
        bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        bool sel = gVisibleChiplist == t;
        float ty = a.y + (rowH - ImGui::GetFontSize()) * 0.5f - px(1);
        dl->AddText(ImVec2(a.x, ty), u32(sel || hov ? p.text : p.textDim), label);
        if (sel)
            dl->AddRectFilled(ImVec2(a.x, a.y + rowH - px(2)), ImVec2(a.x + w, a.y + rowH),
                u32(p.accent), px(1));
        lx += w + gap;
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + ly + rowH));
    ImGui::Dummy(ImVec2(avail, 0));
}

void searchField()
{
    const Palette &p = pal();
    float h = ImGui::GetFrameHeight();
    ImVec2 a = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float iconW = h;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(iconW, ImGui::GetStyle().FramePadding.y));
    ImGui::SetNextItemWidth(w);
    if (sFocusSearch)
    {
        ImGui::SetKeyboardFocusHere();
        sFocusSearch = 0;
    }
    ImGui::InputTextWithHint("##search", T(AppSettings::S_SEARCH), sFilter, sizeof(sFilter));
    bool active = ImGui::IsItemActive();
    ImGui::PopStyleVar();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    if (active)
        dl->AddRect(a, ImVec2(a.x + w, a.y + h), u32(p.accent), ImGui::GetStyle().FrameRounding, 0, 1.0f);
    drawIcon(dl, IC_SEARCH, ImVec2(a.x + iconW * 0.55f, a.y + h * 0.5f), h * 0.22f,
        u32(active ? p.text : p.textFaint), u32(p.surface));
    if (sFilter[0])
    {
        ImVec2 keep = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(a.x + w - h, a.y));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        if (ImGui::InvisibleButton("##clear", ImVec2(h, h)))
            sFilter[0] = 0;
        bool hov = ImGui::IsItemHovered();
        ImGui::PopStyleVar();
        drawIcon(dl, IC_CLOSE, ImVec2(a.x + w - h * 0.5f, a.y + h * 0.5f), h * 0.2f,
            u32(hov ? p.text : p.textDim), u32(p.surface));
        ImGui::SetCursorScreenPos(keep);
    }
}

void drawSplitter(float x, float y, float h)
{
    const Palette &p = pal();
    float grab = px(6);
    ImGui::SetNextWindowPos(ImVec2(x - grab * 0.5f, y));
    ImGui::SetNextWindowSize(ImVec2(grab, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGuiWindowFlags fl = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("##libsplit", NULL, fl))
    {
        ImGui::InvisibleButton("##grip", ImVec2(grab, h));
        bool hov = ImGui::IsItemHovered();
        bool act = ImGui::IsItemActive();
        if (hov || act)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (act)
        {
            gConfig.mToolkitWidth = UiTheme::clampSidebarWidth((int)ImGui::GetIO().MousePos.x);
            sSplitterDrag = 1;
        }
        else if (sSplitterDrag)
        {
            sSplitterDrag = 0;
            gConfig.save();
        }
        int line = (hov || act) ? p.accent : p.border;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(x - 0.5f, y), ImVec2(x - 0.5f, y + h),
            u32(line), (hov || act) ? 2.0f : 1.0f);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void drawLibrary()
{
    const Palette &p = pal();
    float x = 0, y = (float)gTopbarH;
    float w = (float)gConfig.mToolkitWidth;
    float h = (float)(gScreenHeight - gTopbarH - gStatusH);
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, v4(p.panel));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(12), px(14)));
    ImGuiWindowFlags fl = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("##library", NULL, fl))
    {
        if (gVisibleChiplist < 0 || gVisibleChiplist > 4)
            gVisibleChiplist = 0;

        trackedLabel(T(AppSettings::S_LIBRARY), p.textFaint);
        ImGui::Dummy(ImVec2(1, px(2)));
        searchField();
        int searching = sFilter[0] != 0;
        if (!searching)
            categoryTabs();
        else
            ImGui::Dummy(ImVec2(1, px(8)));
        {
            ImVec2 a = ImGui::GetCursorScreenPos();
            a.y -= ImGui::GetStyle().ItemSpacing.y;
            ImGui::GetWindowDrawList()->AddLine(ImVec2(x, a.y), ImVec2(x + w, a.y), u32(p.border), 1.0f);
            ImGui::Dummy(ImVec2(1, px(2)));
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, px(1)));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, px(8));
        if (ImGui::BeginChild("##list", ImVec2(0, 0), 0, ImGuiWindowFlags_NoBackground))
        {
            float rowH = ImGui::GetFontSize() + px(10);
            int shown = 0;
            if (!searching && sRecentCount > 0)
            {
                categoryHeading(T(AppSettings::S_RECENT));
                for (int r = 0; r < sRecentCount && r < 4; r++)
                    libraryRow(sRecent[r] >> 16, sRecent[r] & 0xffff, rowH, 1);
                ImGui::Dummy(ImVec2(1, px(4)));
                categoryHeading(T(kCatKey[gVisibleChiplist]));
            }
            for (int list = 0; list < 5; list++)
            {
                if (!searching && list != gVisibleChiplist)
                    continue;
                int headed = 0;
                int pendingGap = 0;
                for (int i = 0; i < (signed)gAvailableChip[list].size(); i++)
                {
                    const char *name = gAvailableChip[list][i];
                    if (!name)
                    {
                        // NULL entries separate families (2-in, 3-in, ...).
                        pendingGap = shown > 0;
                        continue;
                    }
                    if (hiddenSlot(list, i) || !nameMatches(name, sFilter))
                        continue;
                    if (searching && !headed)
                    {
                        categoryHeading(T(kCatKey[list]));
                        headed = 1;
                        pendingGap = 0;
                    }
                    if (pendingGap && !searching)
                        ImGui::Dummy(ImVec2(1, px(8)));
                    pendingGap = 0;
                    libraryRow(list, i, rowH);
                    shown++;
                }
            }
            if (!shown)
            {
                ImGui::Dummy(ImVec2(1, px(12)));
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + px(10));
                ImGui::TextColored(v4(p.textDim), "%s", T(AppSettings::S_NO_MATCHES));
            }
            ImGui::Dummy(ImVec2(1, px(8)));
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    drawSplitter(w, y, h);
}

//////////////////////////////////////////////////////////////////////////
// Status bar

// Status bar. Everything is laid out by hand on one shared baseline, so
// Inter labels and JetBrains Mono numbers line up; toggles reuse the
// header's icons instead of generic status dots.

struct StatusLine
{
    ImDrawList *dl;
    float x;        // pen position
    float top, h;   // bar rect
    float baseline;
};

float statusRun(StatusLine &L, const char *text, int color, bool mono, bool draw = true)
{
    if (mono)
        pushMono(kBaseFont * 0.84f);
    ImFontBaked *fb = ImGui::GetFontBaked();
    ImVec2 ts = ImGui::CalcTextSize(text);
    if (draw)
        L.dl->AddText(ImVec2(L.x, ImFloor(L.baseline - fb->Ascent)), u32(color), text);
    if (mono)
        popFont();
    L.x += ts.x;
    return ts.x;
}

void statusDivider(StatusLine &L)
{
    L.x += px(12);
    float x = ImFloor(L.x) + 0.5f;
    L.dl->AddLine(ImVec2(x, L.top + L.h * 0.3f), ImVec2(x, L.top + L.h * 0.7f), u32(pal().border), 1.0f);
    L.x += px(12);
}

// Icon + label toggle. On: accent icon, normal label. Off: both faint.
bool statusToggle(StatusLine &L, const char *id, int icon, const char *label, int on,
    const char *tip, const char *keys)
{
    const Palette &p = pal();
    float pad = px(8), isz = px(12), gap = px(6);
    float w = pad + isz + gap + ImGui::CalcTextSize(label).x + pad;
    ImVec2 a(L.x, L.top);
    ImGui::SetCursorScreenPos(a);
    bool hit = ImGui::InvisibleButton(id, ImVec2(w, L.h));
    bool hov = ImGui::IsItemHovered();
    if (hov)
        L.dl->AddRectFilled(ImVec2(a.x, a.y + px(3)), ImVec2(a.x + w, a.y + L.h - px(3)),
            u32(p.surfaceHi), px(4));
    float cy = L.top + L.h * 0.5f;
    drawIcon(L.dl, icon, ImVec2(a.x + pad + isz * 0.5f, cy), isz * 0.5f,
        u32(on ? p.accent : p.textFaint), u32(hov ? p.surfaceHi : p.chrome));
    L.x = a.x + pad + isz + gap;
    statusRun(L, label, on ? (hov ? p.text : p.textDim) : p.textFaint, false);
    L.x = a.x + w;
    if (hov && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        tooltipWithKeys(tip, keys);
    return hit;
}

void drawStatus()
{
    const Palette &p = pal();
    ImGui::PushFont(NULL, kBaseFont * 0.87f);
    gStatusH = (int)(ImGui::GetFontSize() + px(12));
    float W = (float)gScreenWidth;
    float y = (float)(gScreenHeight - gStatusH);
    ImGui::SetNextWindowPos(ImVec2(0, y));
    ImGui::SetNextWindowSize(ImVec2(W, (float)gStatusH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, v4(p.chrome));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGuiWindowFlags fl = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin("##status", NULL, fl))
    {
        StatusLine L;
        L.dl = ImGui::GetWindowDrawList();
        L.top = y;
        L.h = (float)gStatusH;
        // Shared baseline: Inter cap height centered in the bar.
        L.baseline = ImFloor(y + gStatusH * 0.5f + ImGui::GetFontSize() * 0.36f);
        L.dl->AddLine(ImVec2(0, y + 0.5f), ImVec2(W, y + 0.5f), u32(p.border), 1.0f);

        // Left: simulation clock, then the circuit's size.
        L.x = px(12);
        drawIcon(L.dl, IC_WIRES, ImVec2(L.x + px(6), y + gStatusH * 0.5f), px(6),
            u32(p.textDim), u32(p.chrome));
        L.x += px(18);
        if (gConfig.mPhysicsKHz >= 1)
        {
            char khz[24];
            snprintf(khz, sizeof(khz), "%d kHz", gConfig.mPhysicsKHz);
            statusRun(L, khz, p.textDim, true);
        }
        else
            statusRun(L, T(AppSettings::S_ST_SIM), p.textDim, false);
        statusDivider(L);
        struct { int n; int key; } counts[3] = {
            { (int)gChip.size(), AppSettings::S_ST_CHIPS },
            { (int)gWire.size(), AppSettings::S_ST_WIRES },
            { (int)gNet.size(), AppSettings::S_ST_NETS } };
        for (int i = 0; i < 3; i++)
        {
            char num[16];
            snprintf(num, sizeof(num), "%d", counts[i].n);
            if (i)
                L.x += px(14);
            statusRun(L, num, p.text, true);
            L.x += px(5);
            statusRun(L, T(counts[i].key), p.textDim, false);
        }
        int sel = (int)(gMultiSelectChip.size() + gMultiSelectWire.size());
        if (sel == 0 && gUIState.kbditem != 0)
            sel = 1;
        if (sel > 0)
        {
            statusDivider(L);
            char num[16];
            snprintf(num, sizeof(num), "%d", sel);
            statusRun(L, num, p.accent, true);
            L.x += px(5);
            statusRun(L, T(AppSettings::S_SELECTION), p.textDim, false);
        }

        // Right: toggles, divider, zoom. Measure first, then draw.
        char zoom[32];
        snprintf(zoom, sizeof(zoom), "%.0f%%", gZoomFactor / 20.0f * 100.0f);
        pushMono(kBaseFont * 0.84f);
        float zoomW = ImGui::CalcTextSize("0000%").x + px(16);
        popFont();
        float tgl = px(8) * 2 + px(12) + px(6);
        float snapW = tgl + ImGui::CalcTextSize(T(AppSettings::S_SNAP_ITEM)).x;
        float liveW = tgl + ImGui::CalcTextSize(T(AppSettings::S_LIVE_ITEM)).x;
        float right = snapW + px(2) + liveW + px(25) + zoomW + px(4);
        float rx = W - right;
        if (rx > L.x + px(24))
        {
            L.x = rx;
            if (statusToggle(L, "##st_snap", IC_SNAP, T(AppSettings::S_SNAP_ITEM), gSnap,
                T(AppSettings::S_SNAP_ITEM), "Ctrl+P"))
                gSnap = !gSnap;
            L.x += px(2);
            if (statusToggle(L, "##st_live", IC_WIRES, T(AppSettings::S_LIVE_ITEM), gLiveWires,
                T(AppSettings::S_LIVE_ITEM), "Ctrl+W"))
                toggleLiveWires();
            statusDivider(L);
            L.x -= px(11);
            ImVec2 z(L.x, y);
            ImGui::SetCursorScreenPos(z);
            if (ImGui::InvisibleButton("##st_zoom", ImVec2(zoomW, (float)gStatusH)))
                do_zoomext();
            bool hov = ImGui::IsItemHovered();
            if (hov)
                L.dl->AddRectFilled(ImVec2(z.x, z.y + px(3)), ImVec2(z.x + zoomW, z.y + gStatusH - px(3)),
                    u32(p.surfaceHi), px(4));
            pushMono(kBaseFont * 0.84f);
            float zw = ImGui::CalcTextSize(zoom).x;
            popFont();
            L.x = z.x + (zoomW - zw) * 0.5f;
            statusRun(L, zoom, hov ? p.text : p.textDim, true);
            if (hov && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                tooltipWithKeys(T(AppSettings::S_FIT_ITEM), "Ctrl+E");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

//////////////////////////////////////////////////////////////////////////
// Dialogs

// Title row with a close button; returns false when closed.
bool dialogHeader(const char *title)
{
    const Palette &p = pal();
    pushStrong(kBaseFont * 1.2f);
    ImGui::TextUnformatted(title);
    popFont();
    float sz = ImGui::GetFrameHeight();
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - sz);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - px(2));
    bool close = iconButton("##close", IC_CLOSE, false, T(AppSettings::S_CLOSE), "Esc", true, p.panel);
    return !close;
}

void sectionLabel(const char *label)
{
    ImGui::Dummy(ImVec2(1, px(8)));
    trackedLabel(label, pal().textFaint);
    ImVec2 a = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(a.x, a.y), ImVec2(a.x + ImGui::GetContentRegionAvail().x, a.y),
        u32(pal().border), 1.0f);
    ImGui::Dummy(ImVec2(1, px(4)));
}

void rowLabel(const char *label, float col)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(v4(pal().textDim), "%s", label);
    ImGui::SameLine(col);
}

bool beginDialog(const char *id, int *open, float minW)
{
    if (*open && !ImGui::IsPopupOpen(id))
        ImGui::OpenPopup(id);
    ImGui::SetNextWindowPos(ImVec2(gScreenWidth * 0.5f, gScreenHeight * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(px(minW), 0), ImVec2(gScreenWidth - px(40), gScreenHeight - px(40)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(22), px(18)));
    ImGuiWindowFlags fl = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    bool vis = ImGui::BeginPopupModal(id, NULL, fl);
    ImGui::PopStyleVar();
    if (vis && !*open)
    {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return false;
    }
    return vis;
}

void settingsBody()
{
    if (!dialogHeader(T(AppSettings::S_SETTINGS)))
    {
        gSettingsOpen = 0;
        sUserBufInit = 0;
        return;
    }
    const int labels[] = { AppSettings::S_THEME, AppSettings::S_CANVAS, AppSettings::S_UISCALE,
        AppSettings::S_LANGUAGE, AppSettings::S_TOOLTIPS, AppSettings::S_SOUND,
        AppSettings::S_USERNAME, AppSettings::S_FILEASSOC };
    float col = 0;
    for (unsigned i = 0; i < sizeof(labels) / sizeof(labels[0]); i++)
        col = ImMax(col, ImGui::CalcTextSize(AppSettings::text(labels[i], lang(), labels[i] == AppSettings::S_FILEASSOC)).x);
    col += px(28) + ImGui::GetStyle().WindowPadding.x;

    sectionLabel(T(AppSettings::S_APPEARANCE));
    {
        rowLabel(T(AppSettings::S_THEME), col);
        const Palette &p = pal();
        // Order on screen: Dark, Light, Contrast; each a miniature of the
        // app drawn from that theme's own palette.
        const int order[3] = { AppSettings::THEME_DARK, AppSettings::THEME_LIGHT, AppSettings::THEME_CONTRAST };
        const char *lbl[3] = { T(AppSettings::S_THEME_DARK), T(AppSettings::S_THEME_LIGHT), T(AppSettings::S_THEME_CONTRAST) };
        int cur = AppSettings::clampTheme(gConfig.mThemeVariant);
        float cw = px(96), ch = px(60);
        for (int i = 0; i < 3; i++)
        {
            if (i)
                ImGui::SameLine(0, px(10));
            ImGui::BeginGroup();
            ImVec2 a = ImGui::GetCursorScreenPos();
            ImGui::PushID(i);
            bool hit = ImGui::InvisibleButton("##theme", ImVec2(cw, ch));
            bool hov = ImGui::IsItemHovered();
            ImGui::PopID();
            if (hit && order[i] != cur)
            {
                gConfig.mThemeVariant = order[i];
                gBlackBackground = AppSettings::themeWantsDarkCanvas(order[i]);
                gConfig.mCanvasDark = gBlackBackground;
                gConfig.save();
                UiChrome::applyStyle();
            }
            const Palette &tp = UiTheme::palette(order[i]);
            ImDrawList *dl = ImGui::GetWindowDrawList();
            float r = px(6);
            ImVec2 b(a.x + cw, a.y + ch);
            int canvas = order[i] == AppSettings::THEME_LIGHT ? tp.canvasPaper : tp.canvasDark;
            int grid = order[i] == AppSettings::THEME_LIGHT ? tp.gridMajorPaper : tp.gridMajorDark;
            dl->AddRectFilled(a, b, u32(canvas), r);
            for (float gx = a.x + cw * 0.36f + px(12); gx < b.x - 1; gx += px(12))
                dl->AddLine(ImVec2(gx, a.y + ch * 0.18f), ImVec2(gx, b.y - ch * 0.14f), u32(grid), 1.0f);
            // header, sidebar, status bar
            dl->AddRectFilled(a, ImVec2(b.x, a.y + ch * 0.18f), u32(tp.chrome), r, ImDrawFlags_RoundCornersTop);
            dl->AddRectFilled(ImVec2(a.x, a.y + ch * 0.18f), ImVec2(a.x + cw * 0.34f, b.y - ch * 0.14f), u32(tp.panel));
            dl->AddRectFilled(ImVec2(a.x, b.y - ch * 0.14f), b, u32(tp.chrome), r, ImDrawFlags_RoundCornersBottom);
            for (int k = 0; k < 4; k++)
                dl->AddRectFilled(ImVec2(a.x + px(5), a.y + ch * 0.28f + k * px(7)),
                    ImVec2(a.x + cw * (k == 1 ? 0.24f : 0.28f), a.y + ch * 0.28f + k * px(7) + px(2.5f)),
                    u32(k == 1 ? tp.accent : tp.textFaint), px(1));
            // a tiny gate in the theme's ink
            int ink = order[i] == AppSettings::THEME_LIGHT ? 0xff1c1d20 : 0xffe6e7e9;
            ImVec2 g(a.x + cw * 0.58f, a.y + ch * 0.36f);
            float gh = ch * 0.3f;
            dl->PathLineTo(g);
            dl->PathLineTo(ImVec2(g.x + gh * 0.5f, g.y));
            dl->PathArcTo(ImVec2(g.x + gh * 0.5f, g.y + gh * 0.5f), gh * 0.5f, -IM_PI * 0.5f, IM_PI * 0.5f, 12);
            dl->PathLineTo(ImVec2(g.x, g.y + gh));
            dl->PathStroke(u32(ink), ImDrawFlags_Closed, 1.2f);
            dl->AddLine(ImVec2(g.x + gh, g.y + gh * 0.5f), ImVec2(g.x + gh * 1.5f, g.y + gh * 0.5f), u32(0xff4ded78), 1.5f);
            bool sel = order[i] == cur;
            if (sel)
                dl->AddRect(ImVec2(a.x - 1.5f, a.y - 1.5f), ImVec2(b.x + 1.5f, b.y + 1.5f), u32(p.accent), r + 1.5f, 0, 2.0f);
            else
                dl->AddRect(a, b, u32(hov ? p.borderHi : p.border), r, 0, 1.0f);
            ImVec2 ts = ImGui::CalcTextSize(lbl[i]);
            ImGui::SetCursorScreenPos(ImVec2(a.x + (cw - ts.x) * 0.5f, b.y + px(5)));
            ImGui::TextColored(v4(sel ? p.text : p.textDim), "%s", lbl[i]);
            ImGui::EndGroup();
        }
        ImGui::Dummy(ImVec2(1, px(4)));
    }
    {
        rowLabel(T(AppSettings::S_CANVAS), col);
        const char *lbl[2] = { T(AppSettings::S_THEME_DARK), T(AppSettings::S_PAPER) };
        int idx = gBlackBackground ? 0 : 1;
        int n = segmented("canvas", lbl, 2, idx);
        if (n != idx)
            setCanvasDark(n == 0);
    }
    {
        rowLabel(T(AppSettings::S_UISCALE), col);
        const char *lbl[3] = { T(AppSettings::S_SCALE_SMALL), T(AppSettings::S_SCALE_NORMAL), T(AppSettings::S_SCALE_LARGE) };
        int idx = AppSettings::uiScalePresetIndex(AppSettings::clampUiScale(gConfig.mUiScale));
        int n = segmented("scale", lbl, 3, idx);
        if (n != idx)
        {
            gConfig.mUiScale = AppSettings::uiScalePreset(n);
            gConfig.save();
            UiChrome::applyStyle();
        }
    }
    {
        rowLabel(T(AppSettings::S_LANGUAGE), col);
        const char *lbl[2] = { "English", "Русский" };
        int idx = AppSettings::clampLang(gConfig.mLanguage);
        int n = segmented("lang", lbl, 2, idx);
        if (n != idx)
        {
            gConfig.mLanguage = n;
            gConfig.save();
        }
    }

    sectionLabel(T(AppSettings::S_BEHAVIOR));
    {
        rowLabel(T(AppSettings::S_TOOLTIPS), col);
        const char *lbl[4] = { T(AppSettings::S_TT_OFF), T(AppSettings::S_TT_SHORT),
            T(AppSettings::S_TT_NORMAL), T(AppSettings::S_TT_LONG) };
        int idx = AppSettings::tooltipPresetIndex(gConfig.mTooltipDelay);
        int n = segmented("tips", lbl, 4, idx);
        if (n != idx)
        {
            gConfig.mTooltipDelay = AppSettings::tooltipPreset(n);
            gConfig.save();
        }
    }
    {
        rowLabel(T(AppSettings::S_SOUND), col);
        const char *lbl[2] = { T(AppSettings::S_ON), T(AppSettings::S_OFF) };
        int idx = AppSettings::clampAudio(gConfig.mAudioEnable) ? 0 : 1;
        int n = segmented("sound", lbl, 2, idx);
        if (n != idx)
        {
            gConfig.mAudioEnable = n == 0 ? 1 : 0;
            gConfig.save();
        }
        ImGui::SameLine(0, px(12));
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(v4(pal().textFaint), "%s", T(AppSettings::S_SOUND_RESTART_NOTE));
    }

    sectionLabel(T(AppSettings::S_SYSTEM));
    {
        rowLabel(T(AppSettings::S_USERNAME), col);
        if (!sUserBufInit)
        {
            strncpy(sUserBuf, gConfig.mUserInfo ? gConfig.mUserInfo : "", sizeof(sUserBuf) - 1);
            sUserBuf[sizeof(sUserBuf) - 1] = 0;
            sUserBufInit = 1;
        }
        ImGui::SetNextItemWidth(px(260));
        bool commit = ImGui::InputText("##username", sUserBuf, sizeof(sUserBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        // Clicking away commits too, so a lost Enter key can never strand text.
        commit = commit || ImGui::IsItemDeactivatedAfterEdit();
        if (commit)
        {
            // Cap the visible name so the title bar and canvas corner stay
            // one-liners; persist and refresh the window title live.
            sUserBuf[48] = 0;
            if (!gConfig.mUserInfo || strcmp(sUserBuf, gConfig.mUserInfo) != 0)
            {
                delete[] gConfig.mUserInfo;
                gConfig.mUserInfo = mystrdup(sUserBuf);
                gConfig.save();
                char *cur = gFilename ? mystrdup(gFilename) : NULL;
                storefilename(cur);
                delete[] cur;
            }
        }
    }
    {
        rowLabel(TS(AppSettings::S_FILEASSOC), col);
        // Reads registry state once; writes only on explicit clicks.
        static int assocCache = -2;
        if (assocCache == -2)
            assocCache = assocState(0);
        const char *lbl[2] = { T(AppSettings::S_ON), T(AppSettings::S_OFF) };
        int idx = assocCache == 1 ? 0 : 1;
        int n = segmented("assoc", lbl, 2, idx);
        if (n != idx)
        {
            if (n == 0)
                assocCache = assocInstall(0) ? 1 : assocState(0);
            else
                assocCache = assocRemove() ? 0 : assocState(0);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", T(AppSettings::S_FILEASSOC));
    }

    ImGui::Dummy(ImVec2(1, px(14)));
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(v4(pal().textFaint), "%s", T(AppSettings::S_SAVED_NOTE));
    const char *closeLbl = T(AppSettings::S_CLOSE);
    float bw = ImGui::CalcTextSize(closeLbl).x + px(36);
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - bw);
    ImGui::PushStyleColor(ImGuiCol_Button, v4(pal().accent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, v4(pal().accentHi));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, v4(pal().accent));
    ImGui::PushStyleColor(ImGuiCol_Text, v4(pal().accentText));
    if (ImGui::Button(closeLbl, ImVec2(bw, 0)))
    {
        gSettingsOpen = 0;
        sUserBufInit = 0;
    }
    ImGui::PopStyleColor(4);
}

// "Ctrl+Shift+Z" -> separate key caps.
void keyCaps(const char *combo)
{
    pushMono(kBaseFont * 0.87f);
    const Palette &p = pal();
    char buf[64];
    strncpy(buf, combo, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    char *s = buf;
    int first = 1;
    while (*s)
    {
        char *e = s;
        while (*e && *e != '+' && *e != '/')
            e++;
        char sep = *e;
        *e = 0;
        // trim spaces
        while (*s == ' ')
            s++;
        char *t = s + strlen(s);
        while (t > s && t[-1] == ' ')
            *--t = 0;
        if (*s)
        {
            if (!first)
                ImGui::SameLine(0, px(4));
            ImVec2 ts = ImGui::CalcTextSize(s);
            ImVec2 a = ImGui::GetCursorScreenPos();
            float h = ImGui::GetFontSize() + px(6);
            float w = ImMax(h, ts.x + px(12));
            ImGui::Dummy(ImVec2(w, h));
            dl->AddRectFilled(a, ImVec2(a.x + w, a.y + h), u32(p.surface), px(4));
            dl->AddRect(a, ImVec2(a.x + w, a.y + h), u32(p.borderHi), px(4), 0, 1.0f);
            dl->AddText(ImVec2(a.x + (w - ts.x) * 0.5f, a.y + (h - ts.y) * 0.5f), u32(p.text), s);
            first = 0;
        }
        if (!sep)
            break;
        if (sep == '/')
        {
            ImGui::SameLine(0, px(5));
            ImVec2 a = ImGui::GetCursorScreenPos();
            ImVec2 ts = ImGui::CalcTextSize("/");
            float h = ImGui::GetFontSize() + px(6);
            ImGui::Dummy(ImVec2(ts.x, h));
            dl->AddText(ImVec2(a.x, a.y + (h - ts.y) * 0.5f), u32(p.textFaint), "/");
            first = 1;
            ImGui::SameLine(0, px(5));
        }
        s = e + 1;
    }
    popFont();
}

void shortcutsBody()
{
    if (!dialogHeader(T(AppSettings::S_SHORTCUTS)))
    {
        gShortcutsOpen = 0;
        return;
    }
    struct Row { int key; int shortLbl; const char *combo; };
    static const Row rows[] = {
        { AppSettings::S_NEW, 1, "Ctrl+N" },
        { AppSettings::S_OPEN_ITEM, 0, "Ctrl+L" },
        { AppSettings::S_MERGE_ITEM, 0, "Ctrl+M" },
        { AppSettings::S_BOX_ITEM, 0, "Ctrl+B" },
        { AppSettings::S_SAVE_ITEM, 0, "Ctrl+S" },
        { AppSettings::S_PNG_ITEM, 0, "Ctrl+G" },
        { AppSettings::S_UNDO, 1, "Ctrl+Z" },
        { AppSettings::S_REDO, 1, "Ctrl+Y" },
        { AppSettings::S_SELECT_ALL, 0, "Ctrl+A" },
        { AppSettings::S_ROTATE, 0, "Ctrl+R" },
        { AppSettings::S_DELETE, 0, "Del / Ctrl+D" },
        { AppSettings::S_NUDGE, 0, NULL },
        { AppSettings::S_OPTIMIZE, 0, "Ctrl+O" },
        { AppSettings::S_ZOOM_IN, 0, "PgUp" },
        { AppSettings::S_ZOOM_OUT, 0, "PgDn" },
        { AppSettings::S_FIT_ITEM, 0, "Ctrl+E" },
        { AppSettings::S_HOME_ITEM, 0, "Ctrl+H" },
        { AppSettings::S_SNAP_ITEM, 0, "Ctrl+P" },
        { AppSettings::S_LIVE_ITEM, 0, "Ctrl+W" },
        { AppSettings::S_SEARCH, 0, "Ctrl+F" },
        { AppSettings::S_PALETTE, 0, "Ctrl+K" },
        { AppSettings::S_CANCEL, 0, "Esc" },
    };
    const int n = (int)(sizeof(rows) / sizeof(rows[0]));
    const int half = (n + 1) / 2;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(px(6), px(4)));
    if (ImGui::BeginTable("##keys", 4, ImGuiTableFlags_SizingFixedFit))
    {
        for (int r = 0; r < half; r++)
        {
            ImGui::TableNextRow();
            for (int c = 0; c < 2; c++)
            {
                int k = r + c * half;
                ImGui::TableNextColumn();
                if (k >= n)
                {
                    ImGui::TableNextColumn();
                    continue;
                }
                if (c == 1)
                    ImGui::Dummy(ImVec2(px(18), 1)), ImGui::SameLine(0, 0);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + px(3));
                ImGui::TextUnformatted(AppSettings::text(rows[k].key, lang(), rows[k].shortLbl));
                ImGui::TableNextColumn();
                if (rows[k].combo)
                    keyCaps(rows[k].combo);
                else
                    keyCaps("\xe2\x86\x90 \xe2\x86\x91 \xe2\x86\x92 \xe2\x86\x93");
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void aboutBody()
{
    const Palette &p = pal();
    if (!dialogHeader(T(AppSettings::S_ABOUT)))
    {
        gAboutOpen = 0;
        return;
    }
    ImVec2 a = ImGui::GetCursorScreenPos();
    float mh = px(48);
    drawBrandTile(ImGui::GetWindowDrawList(), a, mh);
    ImGui::Dummy(ImVec2(mh + px(8), mh));
    ImGui::SameLine();
    ImGui::BeginGroup();
    pushStrong(kBaseFont * 1.35f);
    ImGui::TextUnformatted("Atanua Prime");
    popFont();
    pushMono(kBaseFont * 0.87f);
    ImGui::TextColored(v4(p.textDim), "%s  %s", ATANUAVERSION, ATANUAPLATFORM);
    popFont();
    ImGui::EndGroup();
    ImGui::Dummy(ImVec2(1, px(8)));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + px(440));
    ImGui::TextColored(v4(p.textDim), "%s", T(AppSettings::S_CREDIT));
    ImGui::TextLinkOpenURL("iki.fi/sol", "http://iki.fi/sol/");
    ImGui::Dummy(ImVec2(1, px(6)));
    ImGui::TextColored(v4(p.textDim), "%s", T(AppSettings::S_ISSUES));
    ImGui::TextLinkOpenURL("github.com/ProGaMEr110521/atanua-prime/issues",
        "https://github.com/ProGaMEr110521/atanua-prime/issues");
    ImGui::TextColored(v4(p.textDim), "%s", T(AppSettings::S_EMAIL));
    ImGui::TextLinkOpenURL("vladtem3943@gmail.com", "mailto:vladtem3943@gmail.com");
    ImGui::PopTextWrapPos();
}

//////////////////////////////////////////////////////////////////////////
// Command palette (Ctrl+K): every menu action plus every library entry,
// fuzzy matched, keyboard driven.

enum Cmd
{
    CMD_NEW, CMD_OPEN, CMD_MERGE, CMD_BOX, CMD_SAVE, CMD_PNG, CMD_UNDO, CMD_REDO,
    CMD_ROTATE, CMD_DELETE, CMD_OPTIMIZE, CMD_FIT, CMD_HOME, CMD_SNAP, CMD_LIVE,
    CMD_CANVAS_DARK, CMD_CANVAS_PAPER, CMD_THEME_DARK, CMD_THEME_LIGHT,
    CMD_THEME_CONTRAST, CMD_LANG_EN, CMD_LANG_RU, CMD_SETTINGS, CMD_SHORTCUTS,
    CMD_ABOUT, CMD_QUIT, CMD_ZOOM_IN, CMD_ZOOM_OUT, CMD_SELECT_ALL, CMD_COUNT
};

struct PaletteEntry
{
    char label[96];
    const char *hint;   // right-hand text: shortcut or category
    int cmd;            // Cmd, or -1 for a component
    int list, index;    // component slot
    int score;
    unsigned char hit[48]; // matched codepoint positions
    int hits;
};

char sPaletteQuery[64];
int sPaletteSel = 0;
int sPaletteFocus = 0;

int decodeLower(const char *&s)
{
    const unsigned char *u = (const unsigned char *)s;
    int cp;
    if (u[0] < 0x80)
    {
        cp = u[0];
        s += 1;
    }
    else if ((u[0] & 0xE0) == 0xC0 && u[1])
    {
        cp = ((u[0] & 0x1F) << 6) | (u[1] & 0x3F);
        s += 2;
    }
    else if ((u[0] & 0xF0) == 0xE0 && u[1] && u[2])
    {
        cp = ((u[0] & 0x0F) << 12) | ((u[1] & 0x3F) << 6) | (u[2] & 0x3F);
        s += 3;
    }
    else
    {
        cp = u[0];
        s += 1;
    }
    if (cp >= 'A' && cp <= 'Z')
        cp += 32;
    else if (cp >= 0x410 && cp <= 0x42F)
        cp += 0x20;
    else if (cp == 0x401)
        cp = 0x451;
    return cp;
}

// Ranking: a contiguous hit beats a scattered one, a hit at a word start
// beats one mid-word, earlier and shorter labels win ties. Falls back to
// a subsequence match (every query character in order). -1 = no match.
int fuzzyScore(const char *text, const char *query, PaletteEntry *e)
{
    e->hits = 0;
    if (!query[0])
        return 0;
    int tc[128], qc[64];
    int tn = 0, qn = 0;
    for (const char *t = text; *t && tn < 128;)
        tc[tn++] = decodeLower(t);
    for (const char *q = query; *q && qn < 64;)
        qc[qn++] = decodeLower(q);
    auto wordStart = [&](int i) {
        return i == 0 || tc[i - 1] == ' ' || tc[i - 1] == '(' || tc[i - 1] == '-' || tc[i - 1] == ':';
    };
    int best = -1, bestAt = -1;
    for (int i = 0; i + qn <= tn; i++)
    {
        int k = 0;
        while (k < qn && tc[i + k] == qc[k])
            k++;
        if (k < qn)
            continue;
        int sc = 10000 + (wordStart(i) ? 2000 : 0) - i * 10 - tn;
        if (sc > best)
        {
            best = sc;
            bestAt = i;
        }
    }
    if (best >= 0)
    {
        for (int k = 0; k < qn && e->hits < (int)sizeof(e->hit); k++)
            e->hit[e->hits++] = (unsigned char)(bestAt + k);
        return best;
    }
    int score = 0, k = 0, prev = -2;
    for (int i = 0; i < tn && k < qn; i++)
    {
        if (tc[i] != qc[k])
            continue;
        score += 10 + (prev == i - 1 ? 30 : 0) + (wordStart(i) ? 20 : 0);
        if (e->hits < (int)sizeof(e->hit))
            e->hit[e->hits++] = (unsigned char)i;
        prev = i;
        k++;
    }
    if (k < qn)
        return -1;
    return score - tn;
}

void cmdLabel(int cmd, char *out, int cap, const char **hint)
{
    struct Def { int cmd; int key; int compact; const char *keys; };
    static const Def defs[] = {
        { CMD_NEW, AppSettings::S_NEW, 1, "Ctrl+N" },
        { CMD_OPEN, AppSettings::S_OPEN_ITEM, 0, "Ctrl+L" },
        { CMD_MERGE, AppSettings::S_MERGE_ITEM, 0, "Ctrl+M" },
        { CMD_BOX, AppSettings::S_BOX_ITEM, 0, "Ctrl+B" },
        { CMD_SAVE, AppSettings::S_SAVE_ITEM, 0, "Ctrl+S" },
        { CMD_PNG, AppSettings::S_PNG_ITEM, 0, "Ctrl+G" },
        { CMD_UNDO, AppSettings::S_UNDO, 1, "Ctrl+Z" },
        { CMD_REDO, AppSettings::S_REDO, 1, "Ctrl+Y" },
        { CMD_ROTATE, AppSettings::S_ROTATE, 0, "Ctrl+R" },
        { CMD_DELETE, AppSettings::S_DELETE, 0, "Del" },
        { CMD_OPTIMIZE, AppSettings::S_OPTIMIZE, 0, "Ctrl+O" },
        { CMD_FIT, AppSettings::S_FIT_ITEM, 0, "Ctrl+E" },
        { CMD_HOME, AppSettings::S_HOME_ITEM, 0, "Ctrl+H" },
        { CMD_SNAP, AppSettings::S_SNAP_ITEM, 0, "Ctrl+P" },
        { CMD_LIVE, AppSettings::S_LIVE_ITEM, 0, "Ctrl+W" },
        { CMD_SETTINGS, AppSettings::S_SETTINGS, 0, "" },
        { CMD_SHORTCUTS, AppSettings::S_SHORTCUTS, 0, "F1" },
        { CMD_ABOUT, AppSettings::S_ABOUT, 0, "" },
        { CMD_QUIT, AppSettings::S_QUIT, 0, "" },
        { CMD_ZOOM_IN, AppSettings::S_ZOOM_IN, 0, "PgUp" },
        { CMD_ZOOM_OUT, AppSettings::S_ZOOM_OUT, 0, "PgDn" },
        { CMD_SELECT_ALL, AppSettings::S_SELECT_ALL, 0, "Ctrl+A" },
    };
    *hint = "";
    for (unsigned i = 0; i < sizeof(defs) / sizeof(defs[0]); i++)
        if (defs[i].cmd == cmd)
        {
            snprintf(out, cap, "%s", AppSettings::text(defs[i].key, lang(), defs[i].compact));
            *hint = defs[i].keys;
            return;
        }
    int group = 0, value = 0;
    switch (cmd)
    {
    case CMD_CANVAS_DARK: group = AppSettings::S_CANVAS; value = AppSettings::S_THEME_DARK; break;
    case CMD_CANVAS_PAPER: group = AppSettings::S_CANVAS; value = AppSettings::S_PAPER; break;
    case CMD_THEME_DARK: group = AppSettings::S_THEME; value = AppSettings::S_THEME_DARK; break;
    case CMD_THEME_LIGHT: group = AppSettings::S_THEME; value = AppSettings::S_THEME_LIGHT; break;
    case CMD_THEME_CONTRAST: group = AppSettings::S_THEME; value = AppSettings::S_THEME_CONTRAST; break;
    case CMD_LANG_EN:
        snprintf(out, cap, "%s: English", T(AppSettings::S_LANGUAGE));
        return;
    case CMD_LANG_RU:
        snprintf(out, cap, "%s: Русский", T(AppSettings::S_LANGUAGE));
        return;
    }
    snprintf(out, cap, "%s: %s", T(group), T(value));
}

void setTheme(int theme)
{
    gConfig.mThemeVariant = theme;
    gBlackBackground = AppSettings::themeWantsDarkCanvas(theme);
    gConfig.mCanvasDark = gBlackBackground;
    gConfig.save();
    UiChrome::applyStyle();
}

void runCmd(int cmd)
{
    switch (cmd)
    {
    case CMD_NEW: do_resetdialog(); break;
    case CMD_OPEN: do_loaddialog(); break;
    case CMD_MERGE: do_loaddialog(1); break;
    case CMD_BOX: do_loaddialog(2); break;
    case CMD_SAVE: do_savedialog(); break;
    case CMD_PNG: gSavePNG = 3; break;
    case CMD_UNDO: undoKeepFocus(0); break;
    case CMD_REDO: undoKeepFocus(1); break;
    case CMD_ROTATE: do_rotate(); break;
    case CMD_DELETE: gUIState.keyentered = SDLK_DELETE; break;
    case CMD_OPTIMIZE: save_undo(); do_optimize_box(0); break;
    case CMD_FIT: do_zoomext(); break;
    case CMD_HOME: do_home(); break;
    case CMD_SNAP: gSnap = !gSnap; break;
    case CMD_LIVE: toggleLiveWires(); break;
    case CMD_CANVAS_DARK: setCanvasDark(1); break;
    case CMD_CANVAS_PAPER: setCanvasDark(0); break;
    case CMD_THEME_DARK: setTheme(AppSettings::THEME_DARK); break;
    case CMD_THEME_LIGHT: setTheme(AppSettings::THEME_LIGHT); break;
    case CMD_THEME_CONTRAST: setTheme(AppSettings::THEME_CONTRAST); break;
    case CMD_LANG_EN: gConfig.mLanguage = AppSettings::LANG_EN; gConfig.save(); break;
    case CMD_LANG_RU: gConfig.mLanguage = AppSettings::LANG_RU; gConfig.save(); break;
    case CMD_SETTINGS: gSettingsOpen = 1; break;
    case CMD_SHORTCUTS: gShortcutsOpen = 1; break;
    case CMD_ABOUT: gAboutOpen = 1; break;
    case CMD_QUIT: doQuit(); break;
    case CMD_ZOOM_IN: UiChrome::zoomBy(1.2f); break;
    case CMD_ZOOM_OUT: UiChrome::zoomBy(1.0f / 1.2f); break;
    case CMD_SELECT_ALL: UiChrome::selectAll(); break;
    }
}

int paletteKeys(ImGuiInputTextCallbackData *d)
{
    if (d->EventFlag == ImGuiInputTextFlags_CallbackHistory)
    {
        if (d->EventKey == ImGuiKey_UpArrow)
            sPaletteSel--;
        else if (d->EventKey == ImGuiKey_DownArrow)
            sPaletteSel++;
    }
    return 0;
}

// Label with the matched characters in the accent color.
void drawMatched(ImDrawList *dl, ImVec2 at, const PaletteEntry &e, int base, int accent)
{
    const char *c = e.label;
    int pos = 0, h = 0;
    float x = at.x;
    while (*c)
    {
        const char *n = c;
        decodeLower(n);
        bool hit = h < e.hits && e.hit[h] == pos;
        if (hit)
            h++;
        dl->AddText(ImVec2(x, at.y), u32(hit ? accent : base), c, n);
        x += ImGui::CalcTextSize(c, n).x;
        c = n;
        pos++;
    }
}

void drawPalette()
{
    static const char *kId = "##palette";
    if (!sPaletteOpen)
    {
        if (ImGui::IsPopupOpen(kId))
        {
            // closed from outside (Esc handler); nothing else to do
        }
        return;
    }
    const Palette &p = pal();
    if (!ImGui::IsPopupOpen(kId))
    {
        ImGui::OpenPopup(kId);
        sPaletteQuery[0] = 0;
        sPaletteSel = 0;
        sPaletteFocus = 1;
    }
    float W = ImMin(px(600), gScreenWidth - px(40));
    ImGui::SetNextWindowPos(ImVec2(gScreenWidth * 0.5f, gTopbarH + px(56)), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(W, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(8), px(8)));
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0.25f));
    ImGuiWindowFlags fl = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
    bool open = ImGui::BeginPopupModal(kId, NULL, fl);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (!open)
    {
        sPaletteOpen = 0;
        return;
    }
    if (!sPaletteOpen)
    {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    // Search field, large and borderless, with the search glyph.
    ImDrawList *dl = ImGui::GetWindowDrawList();
    {
        ImGui::PushFont(NULL, kBaseFont * 1.15f);
        float h = ImGui::GetFontSize() + px(20);
        ImVec2 a = ImGui::GetCursorScreenPos();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(h, (h - ImGui::GetFontSize()) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
        ImGui::SetNextItemWidth(-1);
        if (sPaletteFocus)
        {
            ImGui::SetKeyboardFocusHere();
            sPaletteFocus = 0;
        }
        bool enter = ImGui::InputTextWithHint("##pq", T(AppSettings::S_PALETTE_HINT), sPaletteQuery,
            sizeof(sPaletteQuery), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory,
            paletteKeys);
        if (ImGui::IsItemEdited())
            sPaletteSel = 0;
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        drawIcon(dl, IC_SEARCH, ImVec2(a.x + h * 0.5f, a.y + h * 0.5f), h * 0.2f, u32(p.textDim), u32(p.panel));
        ImGui::PopFont();

        // Gather and rank.
        static PaletteEntry entries[1024];
        int n = 0;
        const char *q = sPaletteQuery;
        for (int c = 0; c < CMD_COUNT && n < 1024; c++)
        {
            PaletteEntry &e = entries[n];
            cmdLabel(c, e.label, sizeof(e.label), &e.hint);
            e.cmd = c;
            e.score = fuzzyScore(e.label, q, &e);
            if (e.score >= 0)
            {
                e.score += q[0] ? 0 : 1000 - c; // keep menu order when empty
                n++;
            }
        }
        if (q[0])
        {
            for (int list = 0; list < 5; list++)
                for (int i = 0; i < (int)gAvailableChip[list].size() && n < 1024; i++)
                {
                    const char *name = gAvailableChip[list][i];
                    if (!name || hiddenSlot(list, i))
                        continue;
                    PaletteEntry &e = entries[n];
                    snprintf(e.label, sizeof(e.label), "%s", name);
                    e.hint = T(kCatKey[list]);
                    e.cmd = -1;
                    e.list = list;
                    e.index = i;
                    e.score = fuzzyScore(e.label, q, &e);
                    if (e.score >= 0)
                        n++;
                }
        }
        else
        {
            // Empty query: recent components lead.
            for (int r = sRecentCount - 1; r >= 0 && n < 1024; r--)
            {
                PaletteEntry &e = entries[n];
                e.list = sRecent[r] >> 16;
                e.index = sRecent[r] & 0xffff;
                snprintf(e.label, sizeof(e.label), "%s", gAvailableChip[e.list][e.index]);
                e.hint = T(AppSettings::S_RECENT);
                e.cmd = -1;
                e.hits = 0;
                e.score = 2000 + r;
                n++;
            }
        }
        // Insertion sort by score (n is small).
        for (int i = 1; i < n; i++)
        {
            PaletteEntry tmp = entries[i];
            int j = i - 1;
            while (j >= 0 && entries[j].score < tmp.score)
            {
                entries[j + 1] = entries[j];
                j--;
            }
            entries[j + 1] = tmp;
        }
        if (n > 0)
        {
            if (sPaletteSel < 0)
                sPaletteSel = n - 1;
            if (sPaletteSel >= n)
                sPaletteSel = 0;
        }

        ImVec2 sep = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(sep.x - px(8), sep.y), ImVec2(sep.x + W, sep.y), u32(p.border), 1.0f);
        ImGui::Dummy(ImVec2(1, px(4)));

        float rowH = ImGui::GetFontSize() + px(14);
        int visible = n < 9 ? n : 9;
        int run = -1;
        if (ImGui::BeginChild("##pl", ImVec2(0, visible > 0 ? rowH * visible + px(2) : rowH), 0,
            ImGuiWindowFlags_NoBackground))
        {
            ImDrawList *cl = ImGui::GetWindowDrawList();
            for (int i = 0; i < n; i++)
            {
                PaletteEntry &e = entries[i];
                ImVec2 a = ImGui::GetCursorScreenPos();
                float w = ImGui::GetContentRegionAvail().x;
                ImGui::PushID(i);
                if (ImGui::InvisibleButton("##r", ImVec2(w, rowH)))
                    run = i;
                if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDelta.x * ImGui::GetIO().MouseDelta.x +
                    ImGui::GetIO().MouseDelta.y * ImGui::GetIO().MouseDelta.y > 0)
                    sPaletteSel = i;
                ImGui::PopID();
                bool sel = i == sPaletteSel;
                if (sel)
                {
                    cl->AddRectFilled(a, ImVec2(a.x + w, a.y + rowH), u32(p.surfaceHi), ImGui::GetStyle().FrameRounding);
                    cl->AddRectFilled(ImVec2(a.x, a.y + rowH * 0.25f), ImVec2(a.x + px(3), a.y + rowH * 0.75f),
                        u32(p.accent), px(2));
                    // Keep the keyboard selection in view.
                    float top = ImGui::GetWindowPos().y, bot = top + ImGui::GetWindowHeight();
                    if (a.y < top || a.y + rowH > bot)
                        ImGui::SetScrollHereY(a.y < top ? 0.0f : 1.0f);
                }
                float ty = a.y + (rowH - ImGui::GetFontSize()) * 0.5f;
                // kind glyph: slider icon for commands, chip outline for parts
                ImVec2 ic(a.x + px(18), a.y + rowH * 0.5f);
                if (e.cmd >= 0)
                    drawIcon(cl, IC_FIT, ic, px(6), u32(p.textFaint), u32(p.panel));
                else
                {
                    cl->AddRect(ImVec2(ic.x - px(6), ic.y - px(4)), ImVec2(ic.x + px(6), ic.y + px(4)),
                        u32(p.textFaint), px(1.5f), 0, 1.25f);
                    for (int k = -1; k <= 1; k += 2)
                    {
                        cl->AddLine(ImVec2(ic.x - px(3), ic.y + k * px(4)), ImVec2(ic.x - px(3), ic.y + k * px(6)), u32(p.textFaint), 1.25f);
                        cl->AddLine(ImVec2(ic.x + px(3), ic.y + k * px(4)), ImVec2(ic.x + px(3), ic.y + k * px(6)), u32(p.textFaint), 1.25f);
                    }
                }
                drawMatched(cl, ImVec2(a.x + px(36), ty), e, sel ? p.text : UiTheme::withAlpha(p.text, 0xd8), p.accent);
                if (e.hint && e.hint[0])
                {
                    bool keys = e.cmd >= 0;
                    if (keys)
                        pushMono(kBaseFont * 0.85f);
                    ImVec2 hs = ImGui::CalcTextSize(e.hint);
                    cl->AddText(ImVec2(a.x + w - hs.x - px(12), a.y + (rowH - hs.y) * 0.5f),
                        u32(p.textFaint), e.hint);
                    if (keys)
                        popFont();
                }
            }
            if (n == 0)
            {
                ImGui::SetCursorPos(ImVec2(px(36), (rowH - ImGui::GetFontSize()) * 0.5f));
                ImGui::TextColored(v4(p.textDim), "%s", T(AppSettings::S_NO_MATCHES));
            }
        }
        ImGui::EndChild();

        ImGui::Dummy(ImVec2(1, px(2)));
        ImGui::PushFont(NULL, kBaseFont * 0.85f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + px(8));
        ImGui::TextColored(v4(p.textFaint), "%s", T(AppSettings::S_PALETTE_FOOT));
        ImGui::PopFont();

        if (enter && n > 0)
            run = sPaletteSel;
        if (run >= 0 && run < n)
        {
            PaletteEntry e = entries[run];
            sPaletteOpen = 0;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            if (e.cmd >= 0)
                runCmd(e.cmd);
            else
                startChipDrag(e.list, e.index); // ghost follows the mouse; click to drop
            return;
        }
    }
    ImGui::EndPopup();
}

// Empty canvas: a faint AND gate where the first part would go and three
// concrete ways to begin, as key caps. No drop-zone box.
void emptyCanvasHint()
{
    if (!gChip.empty() || gDragMode != DRAGMODE_NONE)
        return;
    const Palette &p = pal();
    float x0 = (float)gConfig.mToolkitWidth, x1 = (float)gScreenWidth;
    float y0 = (float)gTopbarH, y1 = (float)(gScreenHeight - gStatusH);
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    bool dark = gBlackBackground != 0;
    int ink = dark ? 0xff8b919c : 0xff5c6068;
    int faint = dark ? 0xff4a4f58 : 0xffa8a49b;
    int capBg = dark ? 0xff1a1d22 : 0xfff3f1ec;
    int capEdge = dark ? 0xff343941 : 0xffc9c5bc;
    float sc = AppSettings::clampUiScale(gConfig.mUiScale);
    float cx = ImFloor((x0 + x1) * 0.5f), cy = ImFloor((y0 + y1) * 0.5f);

    // ghost gate, in the canvas' own schematic language
    float g = px(22);
    ImVec2 gp(cx - g * 1.1f, cy - px(78));
    float t = ImMax(1.0f, px(1.5f));
    dl->PathLineTo(ImVec2(gp.x, gp.y));
    dl->PathLineTo(ImVec2(gp.x + g, gp.y));
    dl->PathArcTo(ImVec2(gp.x + g, gp.y + g), g, -IM_PI * 0.5f, IM_PI * 0.5f, 24);
    dl->PathLineTo(ImVec2(gp.x, gp.y + g * 2));
    dl->PathStroke(u32(faint), ImDrawFlags_Closed, t);
    dl->AddLine(ImVec2(gp.x - g * 0.8f, gp.y + g * 0.5f), ImVec2(gp.x, gp.y + g * 0.5f), u32(faint), t);
    dl->AddLine(ImVec2(gp.x - g * 0.8f, gp.y + g * 1.5f), ImVec2(gp.x, gp.y + g * 1.5f), u32(faint), t);
    dl->AddLine(ImVec2(gp.x + g * 2, gp.y + g), ImVec2(gp.x + g * 2.8f, gp.y + g), u32(faint), t);

    ImFont *ft = sFontStrong ? sFontStrong : ImGui::GetFont();
    const char *title = T(AppSettings::S_EMPTY_TITLE);
    float fsT = kBaseFont * 1.2f * sc;
    ImVec2 ts = ft->CalcTextSizeA(fsT, FLT_MAX, 0, title);
    dl->AddText(ft, fsT, ImVec2(ImFloor(cx - ts.x * 0.5f), cy - px(10)), u32(ink), title);

    // three hints: [keys] label, centered as a row
    struct Hint { const char *keys; int label; int compact; };
    const Hint hints[3] = { { "Ctrl K", AppSettings::S_PALETTE, 0 }, { "Ctrl L", AppSettings::S_OPEN_ITEM, 1 },
        { "F1", AppSettings::S_SHORTCUTS, 0 } };
    ImFont *fm = sFontMono ? sFontMono : ImGui::GetFont();
    ImFont *fb = sFontUI ? sFontUI : ImGui::GetFont();
    float fsM = kBaseFont * 0.84f * 1.1f * sc, fsB = kBaseFont * 0.93f * sc;
    float capPad = px(6), capH = fsB + px(8), gapIn = px(8), gapOut = px(22);
    float widths[3], total = 0;
    for (int i = 0; i < 3; i++)
    {
        widths[i] = fm->CalcTextSizeA(fsM, FLT_MAX, 0, hints[i].keys).x + capPad * 2 + gapIn +
            fb->CalcTextSizeA(fsB, FLT_MAX, 0, AppSettings::text(hints[i].label, lang(), hints[i].compact)).x;
        total += widths[i] + (i ? gapOut : 0);
    }
    float hx = ImFloor(cx - total * 0.5f), hy = cy + px(26);
    for (int i = 0; i < 3; i++)
    {
        float kw = fm->CalcTextSizeA(fsM, FLT_MAX, 0, hints[i].keys).x;
        dl->AddRectFilled(ImVec2(hx, hy), ImVec2(hx + kw + capPad * 2, hy + capH), u32(capBg), px(4));
        dl->AddRect(ImVec2(hx, hy), ImVec2(hx + kw + capPad * 2, hy + capH), u32(capEdge), px(4), 0, 1.0f);
        dl->AddText(fm, fsM, ImVec2(hx + capPad, hy + (capH - fsM) * 0.5f), u32(ink), hints[i].keys);
        float lx = hx + kw + capPad * 2 + gapIn;
        dl->AddText(fb, fsB, ImVec2(lx, hy + (capH - fsB) * 0.5f), u32(ink), AppSettings::text(hints[i].label, lang(), hints[i].compact));
        hx += widths[i] + gapOut;
    }
}

} // namespace

//////////////////////////////////////////////////////////////////////////

namespace UiChrome {

void init()
{
    ImGuiIO &io = ImGui::GetIO();
    ImFontConfig merge;
    merge.MergeMode = true;
    const char *dejavu = "data/fonts/DejaVuSans.ttf";
    sFontUI = io.Fonts->AddFontFromFileTTF("data/fonts/Inter-Regular.otf", kBaseFont);
    if (sFontUI)
        io.Fonts->AddFontFromFileTTF(dejavu, kBaseFont, &merge);
    else
        sFontUI = io.Fonts->AddFontFromFileTTF(dejavu, kBaseFont);
    sFontStrong = io.Fonts->AddFontFromFileTTF("data/fonts/Inter-SemiBold.otf", kBaseFont);
    if (sFontStrong)
        io.Fonts->AddFontFromFileTTF(dejavu, kBaseFont, &merge);
    if (!sFontUI)
    {
        fprintf(stderr, "chrome fonts missing under data/fonts; using the built-in font\n");
        sFontUI = io.Fonts->AddFontDefault();
    }
    if (!sFontStrong)
        sFontStrong = sFontUI;
    sFontMono = io.Fonts->AddFontFromFileTTF("data/fonts/JetBrainsMono-Regular.ttf", kBaseFont);
    if (sFontMono)
        io.Fonts->AddFontFromFileTTF(dejavu, kBaseFont, &merge);
    io.FontDefault = sFontUI;
    if (gConfig.mToolkitWidth < 200)
        gConfig.mToolkitWidth = 248;
    gConfig.mToolkitWidth = UiTheme::clampSidebarWidth(gConfig.mToolkitWidth);
    applyStyle();
}

void applyStyle()
{
    const Palette &p = pal();
    float scale = AppSettings::clampUiScale(gConfig.mUiScale);
    ImGuiStyle st;
    st.WindowPadding = ImVec2(12, 12);
    st.FramePadding = ImVec2(9, 6);
    st.ItemSpacing = ImVec2(8, 7);
    st.ItemInnerSpacing = ImVec2(6, 4);
    st.CellPadding = ImVec2(8, 5);
    st.IndentSpacing = 16;
    st.ScrollbarSize = 10;
    st.GrabMinSize = 10;
    st.WindowBorderSize = 0;
    st.ChildBorderSize = 0;
    st.PopupBorderSize = 1;
    st.FrameBorderSize = 0;
    st.TabBorderSize = 0;
    st.WindowRounding = 10;
    st.ChildRounding = 6;
    st.FrameRounding = 6;
    st.PopupRounding = 8;
    st.ScrollbarRounding = 6;
    st.GrabRounding = 6;
    st.TabRounding = 6;
    st.SeparatorTextBorderSize = 1;
    st.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    st.SelectableTextAlign = ImVec2(0.0f, 0.5f);
    st.ScaleAllSizes(scale);
    st.FontSizeBase = kBaseFont;
    st.FontScaleMain = scale;

    ImVec4 *c = st.Colors;
    int theme = AppSettings::clampTheme(gConfig.mThemeVariant);
    c[ImGuiCol_Text] = v4(p.text);
    c[ImGuiCol_TextDisabled] = v4(p.textFaint);
    c[ImGuiCol_WindowBg] = v4(p.panel);
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = v4(p.panel);
    c[ImGuiCol_Border] = v4(p.border);
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = v4(p.surface);
    c[ImGuiCol_FrameBgHovered] = v4(p.surfaceHi);
    c[ImGuiCol_FrameBgActive] = v4(p.surfaceAct);
    c[ImGuiCol_TitleBg] = v4(p.chrome);
    c[ImGuiCol_TitleBgActive] = v4(p.chrome);
    c[ImGuiCol_TitleBgCollapsed] = v4(p.chrome);
    c[ImGuiCol_MenuBarBg] = v4(p.chrome);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = v4(p.surfaceAct);
    c[ImGuiCol_ScrollbarGrabHovered] = v4(p.borderHi);
    c[ImGuiCol_ScrollbarGrabActive] = v4(p.textFaint);
    c[ImGuiCol_CheckMark] = v4(p.accent);
    c[ImGuiCol_SliderGrab] = v4(p.accent);
    c[ImGuiCol_SliderGrabActive] = v4(p.accentHi);
    c[ImGuiCol_Button] = v4(p.surface);
    c[ImGuiCol_ButtonHovered] = v4(p.surfaceHi);
    c[ImGuiCol_ButtonActive] = v4(p.surfaceAct);
    c[ImGuiCol_Header] = v4(p.surfaceAct);
    c[ImGuiCol_HeaderHovered] = v4(p.surfaceHi);
    c[ImGuiCol_HeaderActive] = v4(p.surfaceAct);
    c[ImGuiCol_Separator] = v4(p.border);
    c[ImGuiCol_SeparatorHovered] = v4(p.borderHi);
    c[ImGuiCol_SeparatorActive] = v4(p.accent);
    c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered] = v4(p.borderHi);
    c[ImGuiCol_ResizeGripActive] = v4(p.accent);
    c[ImGuiCol_Tab] = v4(p.surface);
    c[ImGuiCol_TabHovered] = v4(p.surfaceHi);
    c[ImGuiCol_TabSelected] = v4(p.surfaceAct);
    c[ImGuiCol_TableHeaderBg] = v4(p.surface);
    c[ImGuiCol_TableBorderStrong] = v4(p.border);
    c[ImGuiCol_TableBorderLight] = v4(p.border);
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = v4(UiTheme::withAlpha(p.text, 0x08));
    c[ImGuiCol_TextLink] = v4(p.accent);
    c[ImGuiCol_TextSelectedBg] = v4(UiTheme::withAlpha(p.accent, 0x55));
    c[ImGuiCol_DragDropTarget] = v4(p.accent);
    c[ImGuiCol_NavCursor] = v4(p.accent);
    c[ImGuiCol_NavWindowingHighlight] = v4(p.accent);
    c[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.35f);
    c[ImGuiCol_ModalWindowDimBg] = theme == AppSettings::THEME_LIGHT
        ? ImVec4(0.10f, 0.09f, 0.08f, 0.28f) : ImVec4(0, 0, 0, 0.55f);
    c[ImGuiCol_PlotHistogram] = v4(p.accent);
    c[ImGuiCol_PlotHistogramHovered] = v4(p.accentHi);
    ImGui::GetStyle() = st;
}

void drawFrame()
{
    // Keyboard nav is off, so Escape closing an open menu is done here.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        ImGui::ClosePopupsExceptModals();
    // The header sets gTopbarH, the status bar gStatusH; the library sits
    // between them, so draw it last.
    drawHeader();
    drawStatus();
    drawLibrary();
}

void drawOverlays()
{
    emptyCanvasHint();
    drawPalette();
    if (beginDialog("##settings", &gSettingsOpen, 480))
    {
        settingsBody();
        ImGui::EndPopup();
    }
    if (beginDialog("##shortcuts", &gShortcutsOpen, 520))
    {
        shortcutsBody();
        ImGui::EndPopup();
    }
    if (beginDialog("##about", &gAboutOpen, 420))
    {
        aboutBody();
        ImGui::EndPopup();
    }
}

void closeDialogs()
{
    gSettingsOpen = 0;
    gShortcutsOpen = 0;
    gAboutOpen = 0;
    sUserBufInit = 0;
    sPaletteOpen = 0;
}

void canvasTooltip(const char *partName, const char *text, int netState)
{
    const Palette &p = pal();
    if ((!text || !text[0]) && (!partName || !partName[0]) && netState < 0)
        return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(12), px(10)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(px(6), px(4)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, px(8));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, v4(p.panel));
    ImGui::PushStyleColor(ImGuiCol_Border, v4(p.borderHi));
    if (ImGui::BeginTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + px(300));
        if (partName && partName[0])
        {
            pushMono(kBaseFont * 0.8f);
            ImGui::TextColored(v4(p.textFaint), "%s", partName);
            popFont();
        }
        if (text && text[0])
        {
            // First line is the title, the rest is description.
            const char *nl = strchr(text, '\n');
            pushStrong();
            if (nl)
                ImGui::TextUnformatted(text, nl);
            else
                ImGui::TextUnformatted(text);
            popFont();
            if (nl && nl[1])
            {
                // Old tooltips hard-wrap with newlines; let the card wrap.
                char body[512];
                int o = 0;
                for (const char *c = nl + 1; *c && o < (int)sizeof(body) - 1; c++)
                    body[o++] = (*c == '\n' && c[1] && c[1] != '\n' && o > 0 && body[o - 1] != '\n') ? ' ' : *c;
                body[o] = 0;
                ImGui::PushStyleColor(ImGuiCol_Text, v4(p.textDim));
                ImGui::TextWrapped("%s", body);
                ImGui::PopStyleColor();
            }
        }
        if (netState >= 0)
        {
            int key = AppSettings::S_NET_NC, col = 0xff8a929e;
            const char *note = NULL;
            switch (netState)
            {
            case NETSTATE_HIGH: key = AppSettings::S_NET_HIGH; col = 0xff4ded78; break;
            case NETSTATE_LOW: key = AppSettings::S_NET_LOW; col = 0xff2f9e5a; break;
            case NETSTATE_INVALID: key = AppSettings::S_NET_INVALID; col = 0xfff0544a;
                note = T(AppSettings::S_NET_INVALID_BODY); break;
            default: note = T(AppSettings::S_NET_NC_BODY); break;
            }
            const char *lbl = T(key);
            pushMono(kBaseFont * 0.8f);
            ImVec2 ts = ImGui::CalcTextSize(lbl);
            ImVec2 a = ImGui::GetCursorScreenPos();
            float padX = px(7), h = ts.y + px(4);
            ImDrawList *dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(a, ImVec2(a.x + ts.x + padX * 2, a.y + h), u32(UiTheme::withAlpha(col, 0x2a)), px(4));
            dl->AddRect(a, ImVec2(a.x + ts.x + padX * 2, a.y + h), u32(UiTheme::withAlpha(col, 0x80)), px(4), 0, 1.0f);
            dl->AddText(ImVec2(a.x + padX, a.y + px(2)), u32(col), lbl);
            ImGui::Dummy(ImVec2(ts.x + padX * 2, h));
            popFont();
            if (note)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, v4(p.textDim));
                ImGui::TextWrapped("%s", note);
                ImGui::PopStyleColor();
            }
        }
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void zoomBy(float factor)
{
    float old = gZoomFactor;
    float z = old * factor;
    if (z < 1.0f || z > 30000.0f)
        return;
    gZoomFactor = z;
    // keep the canvas center fixed
    float cx = (gScreenWidth - gConfig.mToolkitWidth) * 0.5f;
    float cy = (gScreenHeight - gTopbarH - gStatusH) * 0.5f;
    gWorldOfsX += cx / z - cx / old;
    gWorldOfsY += cy / z - cy / old;
}

void selectAll()
{
    gMultiSelectChip.clear();
    gMultiSelectWire.clear();
    for (int i = 0; i < (int)gChip.size(); i++)
        if (gChip[i] && gChip[i]->mBox == 0)
            gMultiSelectChip.push_back(gChip[i]);
    for (int i = 0; i < (int)gWire.size(); i++)
        if (gWire[i] && gWire[i]->mBox == 0)
            gMultiSelectWire.push_back(gWire[i]);
    gMultiselectDirty = 1;
}

int dialogOpen()
{
    return gSettingsOpen || gShortcutsOpen || gAboutOpen || sPaletteOpen;
}

int canvasBlocked()
{
    if (!ImGui::GetCurrentContext())
        return 0;
    if (gSettingsOpen || gShortcutsOpen || gAboutOpen || sPaletteOpen)
        return 1;
    // A drag that started in the library (a new chip) or on the canvas
    // keeps going wherever the mouse is.
    if (gDragMode != DRAGMODE_NONE)
        return 0;
    if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        return 1;
    return ImGui::GetIO().WantCaptureMouse ? 1 : 0;
}

void focusSearch()
{
    sFocusSearch = 1;
}

void togglePalette()
{
    sPaletteOpen = !sPaletteOpen;
}

int drawCanvasText(int mono, const char *text, float x, float y, int argb, float lineHeight)
{
    ImGuiContext *ctx = ImGui::GetCurrentContext();
    if (!ctx || !ctx->WithinFrameScope || !text || !text[0])
        return 0;
    ImFont *font = mono ? (sFontMono ? sFontMono : sFontUI) : sFontUI;
    if (!font)
        return 0;

    // World -> screen: projection * modelview (the canvas pan/zoom lives in
    // whichever matrix was current), then NDC -> pixels. 2D affine only.
    float mv[16], pr[16], m[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    glGetFloatv(GL_PROJECTION_MATRIX, pr);
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            m[c * 4 + r] = pr[0 * 4 + r] * mv[c * 4 + 0] + pr[1 * 4 + r] * mv[c * 4 + 1] +
                pr[2 * 4 + r] * mv[c * 4 + 2] + pr[3 * 4 + r] * mv[c * 4 + 3];
    // NDC -> pixel (y down), folded into the matrix
    float hw = gScreenWidth * 0.5f, hh = gScreenHeight * 0.5f;
    for (int c = 0; c < 4; c++)
    {
        float ax = m[c * 4 + 0], ay = m[c * 4 + 1], aw = m[c * 4 + 3];
        m[c * 4 + 0] = ax * hw + aw * hw;
        m[c * 4 + 1] = -ay * hh + aw * hh;
    }
    float sx = sqrtf(m[0] * m[0] + m[1] * m[1]);
    float px = lineHeight * sx;
    if (px < 2.5f)
        return 1; // below legibility: skip, like a tiny bitmap would
    // Rasterize at quarter-octave buckets and scale the rest, so zooming
    // does not bake a new glyph size every frame. Cap the bake size.
    float bucket = powf(2.0f, floorf(log2f(px) * 4.0f + 0.5f) / 4.0f);
    if (bucket > 160.0f)
        bucket = 160.0f;
    float k = lineHeight / bucket; // bucket pixels -> world units

    static ImDrawList *dl = NULL;
    if (!dl)
        dl = IM_NEW(ImDrawList)(ImGui::GetDrawListSharedData());
    dl->_ResetForNewFrame();
    dl->PushClipRect(ImVec2(-1e6f, -1e6f), ImVec2(1e6f, 1e6f));
    dl->PushTexture(ImGui::GetIO().Fonts->TexRef);
    ImU32 col = IM_COL32((argb >> 16) & 0xff, (argb >> 8) & 0xff, argb & 0xff, (argb >> 24) & 0xff);
    dl->AddText(font, bucket, ImVec2(0, 0), col, text);
    for (int i = 0; i < dl->VtxBuffer.Size; i++)
    {
        ImVec2 &v = dl->VtxBuffer[i].pos;
        float wx = x + v.x * k, wy = y + v.y * k;
        v = ImVec2(m[0] * wx + m[4] * wy + m[12], m[1] * wx + m[5] * wy + m[13]);
    }
    // Clip to whatever the canvas scissor is (GL box is bottom-left based).
    ImVec4 clip(0, 0, (float)gScreenWidth, (float)gScreenHeight);
    if (glIsEnabled(GL_SCISSOR_TEST))
    {
        GLint box[4];
        glGetIntegerv(GL_SCISSOR_BOX, box);
        clip = ImVec4((float)box[0], (float)(gScreenHeight - box[1] - box[3]),
            (float)(box[0] + box[2]), (float)(gScreenHeight - box[1]));
    }
    for (int i = 0; i < dl->CmdBuffer.Size; i++)
        dl->CmdBuffer[i].ClipRect = clip;

    ImDrawData dd;
    dd.Valid = true;
    dd.CmdLists.push_back(dl);
    dd.CmdListsCount = 1;
    dd.TotalVtxCount = dl->VtxBuffer.Size;
    dd.TotalIdxCount = dl->IdxBuffer.Size;
    dd.DisplayPos = ImVec2(0, 0);
    dd.DisplaySize = ImVec2((float)gScreenWidth, (float)gScreenHeight);
    dd.FramebufferScale = ImVec2(1, 1);
    dd.Textures = &ImGui::GetPlatformIO().Textures; // upload glyphs baked just now
    ImGui_ImplOpenGL2_RenderDrawData(&dd);
    return 1;
}

} // namespace UiChrome
