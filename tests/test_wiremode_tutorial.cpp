#include <cstdio>
#include <cstring>
#include "ui_theme.h"
#include "app_tutorial.h"
#include "app_settings.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } else { printf("PASS: %s\n", msg); } } while (0)

int main()
{
    using namespace UiTheme;

    /* Projection along a wire: 0 at a, 1 at b, middle in the middle. */
    CHECK(wireProjectionT(5, 0, 0, 0, 10, 0) == 0.5f, "midpoint projects to 0.5");
    CHECK(wireProjectionT(0, 0, 0, 0, 10, 0) == 0.0f, "start projects to 0");
    CHECK(wireProjectionT(10, 0, 0, 0, 10, 0) == 1.0f, "end projects to 1");
    CHECK(wireProjectionT(0, 5, 0, 0, 0, 10) == 0.5f, "vertical midpoint projects");
    CHECK(wireProjectionT(3, 4, 1, 1, 1, 1) == 0.5f, "degenerate segment reports middle");

    /* Modern mode keeps one bend/split zone everywhere on the wire. */
    CHECK(wireZoneAt(0.0f, 0) == WIRE_ZONE_MODERN, "modern start stays modern");
    CHECK(wireZoneAt(0.5f, 0) == WIRE_ZONE_MODERN, "modern center stays modern");
    CHECK(wireZoneAt(1.0f, 0) == WIRE_ZONE_MODERN, "modern end stays modern");

    /* Legacy mode: center band moves, outer squares connect. */
    CHECK(wireZoneAt(0.5f, 1) == WIRE_ZONE_MOVE, "legacy center moves");
    CHECK(wireZoneAt(0.4f, 1) == WIRE_ZONE_MOVE, "legacy center band starts at 0.4");
    CHECK(wireZoneAt(0.6f, 1) == WIRE_ZONE_MOVE, "legacy center band ends at 0.6");
    CHECK(wireZoneAt(0.0f, 1) == WIRE_ZONE_CONNECT, "legacy start connects");
    CHECK(wireZoneAt(1.0f, 1) == WIRE_ZONE_CONNECT, "legacy end connects");
    CHECK(wireZoneAt(0.2f, 1) == WIRE_ZONE_CONNECT, "legacy off-center connects");
    CHECK(wireZoneAt(0.8f, 1) == WIRE_ZONE_CONNECT, "legacy far side connects");
    CHECK(wireZoneAt(-0.5f, 1) == WIRE_ZONE_CONNECT, "legacy beyond start connects");
    CHECK(wireZoneAt(1.5f, 1) == WIRE_ZONE_CONNECT, "legacy beyond end connects");

    using namespace AppTutorial;

    /* First start: briefing shows at welcome. */
    State fresh;
    init(fresh, 0);
    CHECK(visible(fresh) == 1, "first start shows the briefing");
    CHECK(fresh.step == TUT_WELCOME, "briefing starts at welcome");

    /* Walk the whole tour in order. */
    CHECK(advance(fresh) == 0 && fresh.step == TUT_TOOLS, "advance reaches tools");
    CHECK(advance(fresh) == 0 && fresh.step == TUT_COMPONENTS, "advance reaches components");
    CHECK(advance(fresh) == 0 && fresh.step == TUT_SETTINGS, "advance reaches settings");
    CHECK(advance(fresh) == 0 && fresh.step == TUT_HELP, "advance reaches help");
    CHECK(visible(fresh) == 1, "briefing stays visible through help");
    CHECK(advance(fresh) == 1, "advance past help finishes");
    CHECK(visible(fresh) == 0, "finished briefing hides");
    CHECK(fresh.seen == 1, "finish persists the seen flag");

    /* Back never leaves the tour. */
    State mid;
    init(mid, 0);
    advance(mid);
    advance(mid);
    back(mid);
    CHECK(mid.step == TUT_TOOLS, "back returns to tools");
    back(mid);
    back(mid);
    CHECK(mid.step == TUT_WELCOME, "back stops at welcome");

    /* Skip dismisses at once from any step. */
    State early;
    init(early, 0);
    advance(early);
    skip(early);
    CHECK(visible(early) == 0 && early.seen == 1, "skip hides and marks seen");

    /* Later starts stay quiet. */
    State later;
    init(later, 1);
    CHECK(visible(later) == 0, "seen flag hides the briefing on later starts");

    /* Spotlight geometry from live layout metrics. */
    Rect tools = focusRect(TUT_TOOLS, 1280, 800, 220, 48);
    CHECK(tools.hasFocus && tools.w == 1280 && tools.h == 48, "tools spotlights the top bar");
    Rect comp = focusRect(TUT_COMPONENTS, 1280, 800, 220, 48);
    CHECK(comp.hasFocus && comp.x == 0 && comp.w == 220 && comp.h == 752, "components spotlights the palette");
    Rect sett = focusRect(TUT_SETTINGS, 1280, 800, 220, 48);
    CHECK(sett.hasFocus && sett.x + sett.w <= 1190 && sett.x >= 1020 && sett.h == 48,
        "settings fallback stays left of the Quit corner");
    Rect help = focusRect(TUT_HELP, 1280, 800, 220, 48);
    CHECK(help.hasFocus && help.x + help.w <= sett.x,
        "help fallback sits left of settings, never on Quit");
    Rect wel = focusRect(TUT_WELCOME, 1280, 800, 220, 48);
    CHECK(!wel.hasFocus, "welcome centers a dialog with no focus rect");

    /* Every step carries text in both languages. */
    for (int s = 0; s < stepCount(); s++)
    {
        Text t = stepText(s);
        CHECK(t.titleEn[0] && t.titleRu[0] && t.bodyEn[0] && t.bodyRu[0], "step has bilingual text");
        CHECK(strcmp(t.titleEn, t.titleRu) != 0, "step title differs by language");
    }

    /* Persisted values cover the new fields with modern/unseen defaults. */
    AppSettings::Values v;
    AppSettings::defaults(v);
    CHECK(v.wireLegacy == 0 && v.tutorialSeen == 0, "wire/tutorial defaults are modern/unseen");
    char buf[16];
    CHECK(AppSettings::getField(v, "WireLegacy", buf, sizeof(buf)) && strcmp(buf, "0") == 0,
        "wire style serializes");
    AppSettings::Values w;
    AppSettings::defaults(w);
    CHECK(AppSettings::setField(w, "WireLegacy", "1") && w.wireLegacy == 1, "wire style parses");

    if (failures == 0)
        printf("ALL WIREMODE+TUTORIAL TESTS PASSED\n");
    return failures;
}
