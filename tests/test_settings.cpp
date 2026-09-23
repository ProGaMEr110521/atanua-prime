#include <cstdio>
#include <cstring>
#include "app_settings.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } else { printf("PASS: %s\n", msg); } } while (0)

// Decode one UTF-8 sequence; returns codepoint or -1, advances *p.
static int utf8dec(const char *&p)
{
    unsigned char c = (unsigned char)*p;
    if (c < 0x80) { p++; return c; }
    if ((c & 0xE0) == 0xC0)
    {
        unsigned char c2 = (unsigned char)p[1];
        if ((c2 & 0xC0) != 0x80) { p++; return -1; }
        p += 2;
        return ((c & 0x1F) << 6) | (c2 & 0x3F);
    }
    if ((c & 0xF0) == 0xE0)
    {
        unsigned char c2 = (unsigned char)p[1], c3 = (unsigned char)p[2];
        if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) { p++; return -1; }
        p += 3;
        return ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
    }
    p++;
    return -1;
}

int main()
{
    using namespace AppSettings;

    int n = 0;
    stringTable(&n);
    CHECK(n == S_COUNT, "table covers every StrKey");

    int distinctFull = 0, distinctShort = 0;
    for (int k = 0; k < S_COUNT; k++)
    {
        const char *en = text(k, LANG_EN, 0);
        const char *ru = text(k, LANG_RU, 0);
        const char *enS = text(k, LANG_EN, 1);
        const char *ruS = text(k, LANG_RU, 1);
        if (!en[0] || !ru[0] || !enS[0] || !ruS[0])
        {
            printf("FAIL: empty string for key %d\n", k);
            failures++;
            continue;
        }
        if (strcmp(en, ru) != 0) distinctFull++;
        // "PNG" is intentionally identical in both languages.
        if (k != S_PNG && strcmp(enS, ruS) != 0) distinctShort++;
        // Every codepoint must be renderable: ASCII/Latin-1 from the base
        // font page, or Cyrillic from the supplement page.
        for (int v = 0; v < 2; v++)
        {
            const char *s = v ? ruS : ru;
            const char *p = s;
            while (*p)
            {
                int cp = utf8dec(p);
                if (cp < 0 || (cp >= 0x100 && (cp < 0x400 || cp > 0x45F)))
                {
                    printf("FAIL: key %d has uncovered codepoint U+%04X\n", k, cp);
                    failures++;
                    break;
                }
            }
        }
    }
    CHECK(distinctFull == S_COUNT, "every RU full label differs from EN");
    CHECK(distinctShort == S_COUNT - 1, "every RU short label differs from EN except PNG");

    CHECK(strcmp(text(-1, LANG_EN, 0), "") == 0, "unknown key falls back to empty");
    CHECK(strcmp(text(S_COUNT + 5, LANG_RU, 1), "") == 0, "out-of-range key falls back to empty");
    CHECK(strcmp(text(S_QUIT, 99, 0), "Quit") == 0, "unknown language falls back to English");

    CHECK(clampLang(0) == LANG_EN && clampLang(1) == LANG_RU, "valid languages kept");
    CHECK(clampLang(-1) == LANG_EN && clampLang(7) == LANG_EN, "bad language clamps to English");
    CHECK(parseLang("ru") == LANG_RU && parseLang("RU") == LANG_RU, "ru code parses");
    CHECK(parseLang("1") == LANG_RU, "1 parses to Russian");
    CHECK(parseLang("en") == LANG_EN && parseLang("") == LANG_EN, "en/empty parse to English");
    CHECK(parseLang(0) == LANG_EN, "null parses to English");
    CHECK(strcmp(langCode(LANG_RU), "ru") == 0, "ru code round-trips");
    CHECK(strcmp(langName(LANG_RU, LANG_RU), text(S_RUSSIAN, LANG_RU, 0)) == 0, "language names localize");

    CHECK(clampTheme(0) == THEME_DARK && clampTheme(1) == THEME_CONTRAST, "valid themes kept");
    CHECK(clampTheme(9) == THEME_DARK, "bad theme clamps to dark");
    CHECK(clampAudio(5) == 1 && clampAudio(0) == 0, "audio normalizes to 0/1");
    CHECK(clampTooltipMs(-5) == 0 && clampTooltipMs(99999) == 10000, "tooltip delay clamps");
    CHECK(tooltipPreset(0) == 0 && tooltipPreset(2) == 1500, "tooltip presets");
    CHECK(tooltipPresetIndex(0) == 0 && tooltipPresetIndex(3000) == 3, "preset index maps");
    CHECK(clampUiScale(0.1f) == 0.8f && clampUiScale(9.0f) == 1.5f, "ui scale clamps");
    CHECK(uiScalePreset(0) == 0.85f && uiScalePreset(1) == 1.0f && uiScalePreset(2) == 1.25f, "scale presets");
    CHECK(uiScalePresetIndex(1.25f) == 2 && uiScalePresetIndex(0.85f) == 0, "scale index maps");
    CHECK(parseFloat("1.25", 0) == 1.25f && parseFloat("xx", 7.0f) == 7.0f, "float parses");
    char fbuf[16];
    CHECK(formatFloat2(1.25f, fbuf, sizeof(fbuf)) && strcmp(fbuf, "1.25") == 0, "float formats");

    CHECK(themeMenuBg(THEME_DARK) == 0xff20242c, "dark theme keeps legacy chrome");
    CHECK(themeAccent(THEME_DARK) == 0xff4c8dff, "dark theme keeps legacy accent");
    CHECK(themeMenuBg(THEME_CONTRAST) != themeMenuBg(THEME_DARK), "contrast theme differs");
    CHECK(themeAccent(THEME_CONTRAST) != themeAccent(THEME_DARK), "contrast accent differs");
    CHECK(themeHotRow(THEME_DARK) == 0xff31406b, "dark theme keeps legacy hot row");
    CHECK(themeHotRow(THEME_CONTRAST) != themeHotRow(THEME_DARK), "contrast hot row differs");

    CHECK(fieldCount() == 7, "seven persisted fields");
    CHECK(strcmp(fieldTag(0), "Language") == 0, "language tag matches atanua.xml style");
    CHECK(strcmp(fieldTag(1), "ThemeVariant") == 0, "theme tag named");
    CHECK(strcmp(fieldTag(4), "UiScale") == 0, "scale tag named");
    CHECK(strcmp(fieldTag(5), "CanvasDark") == 0, "canvas tag named");
    CHECK(strcmp(fieldTag(6), "LiveWires") == 0, "wires tag named");

    Values v;
    defaults(v);
    CHECK(v.language == LANG_EN && v.theme == THEME_DARK, "defaults are English dark");
    CHECK(v.tooltipMs == 1500 && v.audio == 1, "defaults match shipped config");
    CHECK(v.uiScale == 1.0f, "default scale is 1");
    CHECK(v.canvasDark == 1 && v.liveWires == 1, "defaults are dark canvas, live wires");
    char buf[16];
    CHECK(getField(v, "Language", buf, sizeof(buf)) && strcmp(buf, "0") == 0, "language serializes");
    CHECK(getField(v, "TooltipDelay", buf, sizeof(buf)) && strcmp(buf, "1500") == 0, "tooltip serializes");
    CHECK(!getField(v, "Nope", buf, sizeof(buf)), "unknown tag rejected");

    Values w;
    defaults(w);
    CHECK(setField(w, "Language", "ru") && w.language == LANG_RU, "ru string parses");
    CHECK(setField(w, "Language", "1") && w.language == LANG_RU, "1 parses");
    CHECK(setField(w, "Language", "xx") && w.language == LANG_EN, "garbage language clamps");
    CHECK(setField(w, "ThemeVariant", "1") && w.theme == THEME_CONTRAST, "theme parses");
    CHECK(setField(w, "ThemeVariant", "9") && w.theme == THEME_DARK, "bad theme clamps");
    CHECK(setField(w, "TooltipDelay", "750") && w.tooltipMs == 750, "tooltip parses");
    CHECK(setField(w, "TooltipDelay", "-3") && w.tooltipMs == 0, "negative tooltip clamps");
    CHECK(setField(w, "AudioEnable", "0") && w.audio == 0, "audio parses");
    CHECK(setField(w, "UiScale", "1.25") && w.uiScale == 1.25f, "scale parses");
    CHECK(setField(w, "UiScale", "99") && w.uiScale == 1.5f, "bad scale clamps");
    CHECK(setField(w, "CanvasDark", "0") && w.canvasDark == 0, "canvas parses");
    CHECK(setField(w, "CanvasDark", "9") && w.canvasDark == 1, "bad canvas clamps");
    CHECK(setField(w, "LiveWires", "0") && w.liveWires == 0, "wires parse");
    CHECK(!setField(w, "Nope", "1"), "unknown tag rejected on set");
    CHECK(!setField(w, 0, "1") && !setField(w, "Language", 0), "null guarded");

    // Full persistence round-trip: serialize every field, parse back.
    Values rt;
    defaults(rt);
    rt.language = LANG_RU;
    rt.theme = THEME_CONTRAST;
    rt.tooltipMs = 750;
    rt.audio = 0;
    rt.uiScale = 1.25f;
    rt.canvasDark = 0;
    rt.liveWires = 0;
    Values back;
    defaults(back);
    for (int i = 0; i < fieldCount(); i++)
    {
        CHECK(getField(rt, fieldTag(i), buf, sizeof(buf)), "round-trip serializes");
        CHECK(setField(back, fieldTag(i), buf), "round-trip parses");
    }
    validate(back);
    CHECK(back.language == LANG_RU && back.theme == THEME_CONTRAST, "round-trip keeps language+theme");
    CHECK(back.tooltipMs == 750 && back.audio == 0, "round-trip keeps tooltip+audio");
    CHECK(back.uiScale == 1.25f, "round-trip keeps scale");
    CHECK(back.canvasDark == 0 && back.liveWires == 0, "round-trip keeps canvas+wires");

    if (failures == 0)
        printf("ALL SETTINGS TESTS PASSED\n");
    return failures;
}
