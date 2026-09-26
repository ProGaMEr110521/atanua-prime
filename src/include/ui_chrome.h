/*
Atanua Prime - application chrome (Dear ImGui).
Header bar with menus and quick actions, component library, status bar,
and the settings / shortcuts / about dialogs. The canvas itself is still
drawn by main.cpp with immediate-mode GL; this module only owns the frame
around it and tells main.cpp where the canvas is.
*/
#ifndef UI_CHROME_H
#define UI_CHROME_H

// Chrome geometry shared with the canvas code (pixels).
extern int gTopbarH;
extern int gStatusH;

// Dialog visibility; main.cpp gates canvas input on these.
extern int gSettingsOpen;
extern int gShortcutsOpen;
extern int gAboutOpen;

namespace UiChrome {

// Fonts and style. Call once the GL context exists.
void init();

// Re-apply colors and metrics after a theme or UI scale change.
void applyStyle();

// Header, library and status bar. Call right after ImGui::NewFrame().
void drawFrame();

// Modal dialogs and the empty-canvas hint. Call just before ImGui::Render().
void drawOverlays();

// Close every chrome dialog (Escape).
void closeDialogs();

// Canvas hover card. partName: mono caption (may be NULL); text: the
// part/pin description, first line is the title; netState: NETSTATE_* for
// a live signal tag, or -1 for none.
void canvasTooltip(const char *partName, const char *text, int netState);

// Zoom the canvas around its center by a factor (menu / palette).
void zoomBy(float factor);

// Select every top-level chip and wire (Ctrl+A).
void selectAll();

// 1 while a modal dialog or the command palette is open.
int dialogOpen();

// 1 while the mouse belongs to the chrome (a menu, dialog, or panel over
// the canvas), so canvas picking and clicks must be skipped this frame.
int canvasBlocked();

// Ctrl+F: move keyboard focus to the library search field.
void focusSearch();

// Ctrl+K: command palette over every action and component.
void togglePalette();

// Draw canvas text (world units, current GL modelview) with the chrome's
// vector fonts, rasterized at the on-screen size. mono selects JetBrains
// Mono, else Inter. Top-left at (x, y); lineHeight in world units.
// Returns 0 when it cannot draw (no ImGui frame), so callers fall back.
int drawCanvasText(int mono, const char *text, float x, float y, int argb, float lineHeight);

} // namespace UiChrome

#endif
