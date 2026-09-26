/*
Atanua settings + language units.
Pure, window-system independent helpers behind the SDL/OpenGL UI, so unit
tests can drive language mapping, string lookup and config persistence
without opening a window.
*/
#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <stddef.h>
#include <string.h>
#include "ui_theme.h"

namespace AppSettings {

// Supported UI languages. Stored in atanua.xml as a decimal string.
enum Language
{
    LANG_EN = 0,
    LANG_RU = 1,
    LANG_COUNT = 2
};

// Theme variants. Stored in atanua.xml as a decimal string.
enum Theme
{
    THEME_DARK = 0,
    THEME_CONTRAST = 1,
    THEME_LIGHT = 2,
    THEME_COUNT = 3
};

// Tooltip delay presets in milliseconds (0 = off).
inline int tooltipPreset(int index)
{
    static const int kPresets[4] = { 0, 750, 1500, 3000 };
    if (index < 0) index = 0;
    if (index > 3) index = 3;
    return kPresets[index];
}

inline int tooltipPresetIndex(int ms)
{
    if (ms <= 0) return 0;
    if (ms <= 750) return 1;
    if (ms <= 1500) return 2;
    return 3;
}

// UI scale presets as multipliers of the base chrome font sizes.
inline float uiScalePreset(int index)
{
    static const float kPresets[3] = { 0.85f, 1.0f, 1.25f };
    if (index < 0) index = 0;
    if (index > 2) index = 2;
    return kPresets[index];
}

inline int uiScalePresetIndex(float s)
{
    if (s < 0.925f) return 0;
    if (s < 1.125f) return 1;
    return 2;
}

inline float clampUiScale(float s)
{
    if (s < 0.8f) return 0.8f;
    if (s > 1.5f) return 1.5f;
    return s;
}

// Chrome + settings string keys.
enum StrKey
{
    S_BASE = 0,
    S_CHIPS,
    S_IN,
    S_OUT,
    S_MISC,
    S_NEW,
    S_LOAD,
    S_MERGE,
    S_BOX,
    S_SAVE,
    S_UNDO,
    S_REDO,
    S_HOME,
    S_ZOOM,
    S_SNAP_ON,
    S_SNAP_OFF,
    S_VIEW_LIVE,
    S_VIEW_GREY,
    S_PNG,
    S_QUIT,
    S_SETTINGS,
    S_LANGUAGE,
    S_ENGLISH,
    S_RUSSIAN,
    S_THEME,
    S_THEME_DARK,
    S_THEME_CONTRAST,
    S_TOOLTIPS,
    S_TT_OFF,
    S_TT_SHORT,
    S_TT_NORMAL,
    S_TT_LONG,
    S_SOUND,
    S_ON,
    S_OFF,
    S_CLOSE,
    S_SOUND_RESTART_NOTE,
    S_SAVED_NOTE,
    S_UISCALE,
    S_SCALE_SMALL,
    S_SCALE_NORMAL,
    S_SCALE_LARGE,
    S_SHORTCUTS,
    S_ACTION,
    S_SHORTCUT,
    S_ROTATE,
    S_OPTIMIZE,
    S_DELETE,
    S_CANCEL,
    S_NUDGE,
    S_ARROWS,
    S_ISSUES,
    S_EMAIL,
    S_FILEASSOC,
    S_OPENTITLE,
    S_SAVETITLE,
    S_CONFIRM_EXIT,
    S_CONFIRM_RESET,
    S_CONFIRM_OPEN,
    S_ERR_BOXLIMIT,
    S_ERR_BOXNOPINS,
    S_ERR_BADWIRE,
    S_USERNAME,
    S_CREDIT,
    S_CANVAS,
    S_PAPER,
    S_LIVE,
    S_GREY,
    S_THEME_LIGHT,
    S_MENU_FILE,
    S_MENU_EDIT,
    S_MENU_VIEW,
    S_MENU_HELP,
    S_LIBRARY,
    S_SEARCH,
    S_NO_MATCHES,
    S_DRAG_HINT,
    S_UNTITLED,
    S_OPEN_ITEM,
    S_MERGE_ITEM,
    S_BOX_ITEM,
    S_SAVE_ITEM,
    S_PNG_ITEM,
    S_FIT_ITEM,
    S_HOME_ITEM,
    S_SNAP_ITEM,
    S_LIVE_ITEM,
    S_PERF_ITEM,
    S_ABOUT,
    S_APPEARANCE,
    S_BEHAVIOR,
    S_SYSTEM,
    S_ST_CHIPS,
    S_ST_WIRES,
    S_ST_NETS,
    S_ST_SIM,
    S_SELECTION,
    S_EMPTY_TITLE,
    S_EMPTY_BODY,
    S_REPORT_ISSUE,
    S_PALETTE,
    S_PALETTE_HINT,
    S_KIND_COMMAND,
    S_RECENT,
    S_PALETTE_FOOT,
    S_ZOOM_IN,
    S_ZOOM_OUT,
    S_SELECT_ALL,
    S_NET_HIGH,
    S_NET_LOW,
    S_NET_NC,
    S_NET_INVALID,
    S_NET_NC_BODY,
    S_NET_INVALID_BODY,
    S_PIN_ON,
    S_COUNT
};

struct StrEntry
{
    int key;
    const char *en;
    const char *enShort;
    const char *ru;
    const char *ruShort;
};

// Full table. Russian strings use only Cyrillic letters plus Latin-1
// punctuation (already in the base font) so the Cyrillic supplement page
// covers every codepoint. No em dashes, no fancy quotes.
inline const StrEntry *stringTable(int *countOut)
{
    static const StrEntry kTable[] = {
        { S_BASE, "Base", "Base", "База", "База" },
        { S_CHIPS, "Chips", "Chips", "Чипы", "Чипы" },
        { S_IN, "In", "In", "Вход", "Вход" },
        { S_OUT, "Out", "Out", "Выходы", "Выходы" },
        { S_MISC, "Misc", "Misc", "Разное", "Разное" },
        { S_NEW, "New\nCtrl-N", "New", "Новый\nCtrl-N", "Новый" },
        { S_LOAD, "Load\nCtrl-L", "Load", "Открыть\nCtrl-L", "Открыть" },
        { S_MERGE, "Merge\nCtrl-M", "Merge", "Слияние\nCtrl-M", "Слияние" },
        { S_BOX, "Box\nCtrl-B", "Box", "Блок\nCtrl-B", "Блок" },
        { S_SAVE, "Save\nCtrl-S", "Save", "Сохранить\nCtrl-S", "Сохранить" },
        { S_UNDO, "Undo\nCtrl-Z", "Undo", "Отменить\nCtrl-Z", "Отменить" },
        { S_REDO, "Redo\nCtrl-Y", "Redo", "Вернуть\nCtrl-Y", "Вернуть" },
        { S_HOME, "Home", "Home", "Домой", "Домой" },
        { S_ZOOM, "Zoom\next", "Zoom", "Вся\nсхема", "Вся схема" },
        { S_SNAP_ON, "Snap\n(on)", "Snap", "Привязка\n(вкл)", "Привязка" },
        { S_SNAP_OFF, "Snap\n(off)", "Snap", "Привязка\n(выкл)", "Привязка" },
        { S_VIEW_LIVE, "Wires\n(live)", "Wires", "Провода\n(цветные)", "Провода" },
        { S_VIEW_GREY, "Wires\n(grey)", "Wires", "Провода\n(серые)", "Провода" },
        { S_PNG, "PNG it\nCtrl-G", "PNG", "PNG\nCtrl-G", "PNG" },
        { S_QUIT, "Quit", "Quit", "Выход", "Выход" },
        { S_SETTINGS, "Settings", "Settings", "Настройки", "Настройки" },
        { S_LANGUAGE, "Language", "Language", "Язык", "Язык" },
        { S_ENGLISH, "English", "English", "Английский", "Английский" },
        { S_RUSSIAN, "Russian", "Russian", "Русский", "Русский" },
        { S_THEME, "Theme", "Theme", "Тема", "Тема" },
        { S_THEME_DARK, "Dark", "Dark", "Тёмная", "Тёмная" },
        { S_THEME_CONTRAST, "Contrast", "Contrast", "Контрастная", "Контрастная" },
        { S_TOOLTIPS, "Tooltips", "Tooltips", "Подсказки", "Подсказки" },
        { S_TT_OFF, "Off", "Off", "Выкл", "Выкл" },
        { S_TT_SHORT, "Short", "Short", "Короткие", "Короткие" },
        { S_TT_NORMAL, "Normal", "Normal", "Обычные", "Обычные" },
        { S_TT_LONG, "Long", "Long", "Длинные", "Длинные" },
        { S_SOUND, "Sound", "Sound", "Звук", "Звук" },
        { S_ON, "On", "On", "Вкл", "Вкл" },
        { S_OFF, "Off", "Off", "Выкл", "Выкл" },
        { S_CLOSE, "Close", "Close", "Закрыть", "Закрыть" },
        { S_SOUND_RESTART_NOTE, "Sound applies on restart", "Sound applies on restart",
          "Звук применится после перезапуска", "Звук применится после перезапуска" },
        { S_SAVED_NOTE, "Saved to atanua.xml", "Saved to atanua.xml",
          "Сохранено в atanua.xml", "Сохранено в atanua.xml" },
        { S_UISCALE, "UI scale", "UI scale", "Масштаб", "Масштаб" },
        { S_SCALE_SMALL, "Small", "Small", "Мелкий", "Мелкий" },
        { S_SCALE_NORMAL, "Normal", "Normal", "Обычный", "Обычный" },
        { S_SCALE_LARGE, "Large", "Large", "Крупный", "Крупный" },
        { S_SHORTCUTS, "Shortcuts", "Shortcuts", "Горячие клавиши", "Горячие клавиши" },
        { S_ACTION, "Action", "Action", "Действие", "Действие" },
        { S_SHORTCUT, "Shortcut", "Shortcut", "Сочетание", "Сочетание" },
        { S_ROTATE, "Rotate", "Rotate", "Повернуть", "Повернуть" },
        { S_OPTIMIZE, "Optimize box", "Optimize", "Оптимизировать", "Оптимизировать" },
        { S_DELETE, "Delete", "Delete", "Удалить", "Удалить" },
        { S_CANCEL, "Cancel", "Cancel", "Отмена", "Отмена" },
        { S_NUDGE, "Nudge", "Nudge", "Сдвиг", "Сдвиг" },
        { S_ARROWS, "Arrows", "Arrows", "Стрелки", "Стрелки" },
        { S_ISSUES, "If you find any kind of bug or error, feel free to open an issue ticket on my github",
            "If you find any kind of bug or error, feel free to open an issue ticket on my github"
            , "Если вы нашли баг или испытываете трудности в работе с какими-либо функциями или хотите добавить что-либо, пожалуйста сообщите разработчику на GitHub",
            "Если вы нашли баг или испытываете трудности в работе с какими-либо функциями или хотите добавить что-либо, пожалуйста сообщите разработчику на GitHub"},
        { S_EMAIL, "or contact the developer directly by email",
            "or contact the developer directly by email",
            "или свяжитесь с разработчиком напрямую по почте",
            "или свяжитесь с разработчиком напрямую по почте"},
        { S_FILEASSOC, "Open .atanua files with this app",
            "Associate .atanua",
            "Открывать файлы .atanua в этом приложении",
            "Ассоциация .atanua"},
        { S_OPENTITLE, "Open Atanua design file", "Open Atanua design file",
            "Открыть файл проекта Atanua", "Открыть файл проекта Atanua" },
        { S_SAVETITLE, "Save Atanua design file", "Save Atanua design file",
            "Сохранить файл проекта Atanua", "Сохранить файл проекта Atanua" },
        { S_CONFIRM_EXIT, "Are you sure you want to exit?\nAny unsaved changes will be lost.",
            "Are you sure you want to exit?\nAny unsaved changes will be lost.",
            "Точно выйти?\nНесохранённые изменения будут потеряны.",
            "Точно выйти?\nНесохранённые изменения будут потеряны." },
        { S_CONFIRM_RESET, "Are you sure you want to reset?\nAny unsaved changes will be lost.",
            "Are you sure you want to reset?\nAny unsaved changes will be lost.",
            "Точно сбросить схему?\nНесохранённые изменения будут потеряны.",
            "Точно сбросить схему?\nНесохранённые изменения будут потеряны." },
        { S_CONFIRM_OPEN, "Open %s?\nAny unsaved changes will be lost.",
            "Open %s?\nAny unsaved changes will be lost.",
            "Открыть %s?\nНесохранённые изменения будут потеряны.",
            "Открыть %s?\nНесохранённые изменения будут потеряны." },
        { S_ERR_BOXLIMIT, "Maximum number of active boxes exceeded.\nContinue loading anyway?\n\nIf you really need more boxes, adjust the limit in atanua.xml",
            "Maximum number of active boxes exceeded.\nContinue loading anyway?\n\nIf you really need more boxes, adjust the limit in atanua.xml",
            "Превышено максимальное число активных боксов.\nПродолжить загрузку?\n\nЕсли нужно больше, измените лимит в atanua.xml",
            "Превышено максимальное число активных боксов.\nПродолжить загрузку?\n\nЕсли нужно больше, измените лимит в atanua.xml" },
        { S_ERR_BOXNOPINS, "Trying to box an .atanua file with no external pins!\nBuild it anyway?",
            "Trying to box an .atanua file with no external pins!\nBuild it anyway?",
            "В .atanua файле нет внешних пинов!\nВсё равно собрать бокс?",
            "В .atanua файле нет внешних пинов!\nВсё равно собрать бокс?" },
        { S_ERR_BADWIRE, "Invalid wire definition found.\nTry to continue loading?",
            "Invalid wire definition found.\nTry to continue loading?",
            "Найдено неверное описание провода.\nПродолжить загрузку?",
            "Найдено неверное описание провода.\nПродолжить загрузку?" },
        { S_USERNAME, "User name", "User name", "Имя", "Имя" },
        { S_CREDIT, "Based on Atanua by Jari Komppa", "Based on Atanua by Jari Komppa",
            "Основано на Atanua, автор: Jari Komppa", "Основано на Atanua, автор: Jari Komppa" },
        { S_CANVAS, "Background", "Background", "Фон", "Фон" },
        { S_PAPER, "Paper", "Paper", "Бумага", "Бумага" },
        { S_LIVE, "Live", "Live", "Цветные", "Цветные" },
        { S_GREY, "Grey", "Grey", "Серые", "Серые" },
        { S_THEME_LIGHT, "Light", "Light", "Светлая", "Светлая" },
        { S_MENU_FILE, "File", "File", "Файл", "Файл" },
        { S_MENU_EDIT, "Edit", "Edit", "Правка", "Правка" },
        { S_MENU_VIEW, "View", "View", "Вид", "Вид" },
        { S_MENU_HELP, "Help", "Help", "Справка", "Справка" },
        { S_LIBRARY, "Library", "Library", "Библиотека", "Библиотека" },
        { S_SEARCH, "Search components", "Search", "Поиск компонентов", "Поиск" },
        { S_NO_MATCHES, "Nothing matches", "Nothing matches", "Ничего не найдено", "Ничего не найдено" },
        { S_DRAG_HINT, "Drag onto the canvas", "Drag onto the canvas", "Перетащите на холст", "Перетащите на холст" },
        { S_UNTITLED, "Untitled", "Untitled", "Без имени", "Без имени" },
        { S_OPEN_ITEM, "Open...", "Open", "Открыть...", "Открыть" },
        { S_MERGE_ITEM, "Merge into design...", "Merge", "Добавить в схему...", "Добавить" },
        { S_BOX_ITEM, "Import as box...", "Box", "Импорт как блок...", "Блок" },
        { S_SAVE_ITEM, "Save as...", "Save", "Сохранить как...", "Сохранить" },
        { S_PNG_ITEM, "Export screenshot (PNG)", "PNG", "Снимок экрана (PNG)", "Снимок" },
        { S_FIT_ITEM, "Zoom to fit", "Fit", "Показать всю схему", "Вся схема" },
        { S_HOME_ITEM, "Reset view", "Home", "Сбросить вид", "Сброс вида" },
        { S_SNAP_ITEM, "Snap to grid", "Snap", "Привязка к сетке", "Привязка" },
        { S_LIVE_ITEM, "Colored wires", "Live wires", "Цветные провода", "Цветные" },
        { S_PERF_ITEM, "Performance overlay", "Perf", "Счётчик производительности", "Счётчик" },
        { S_ABOUT, "About Atanua Prime", "About", "О программе", "О программе" },
        { S_APPEARANCE, "Appearance", "Appearance", "Оформление", "Оформление" },
        { S_BEHAVIOR, "Behavior", "Behavior", "Поведение", "Поведение" },
        { S_SYSTEM, "System", "System", "Система", "Система" },
        { S_ST_CHIPS, "chips", "chips", "чипов", "чипов" },
        { S_ST_WIRES, "wires", "wires", "проводов", "проводов" },
        { S_ST_NETS, "nets", "nets", "цепей", "цепей" },
        { S_ST_SIM, "Simulating", "Sim", "Симуляция", "Сим." },
        { S_SELECTION, "selected", "selected", "выбрано", "выбрано" },
        { S_EMPTY_TITLE, "Start a circuit", "Start a circuit", "Начните схему", "Начните схему" },
        { S_EMPTY_BODY, "Drag a component from the library, or open a design with Ctrl+L", "Drag a component from the library", "Перетащите компонент из библиотеки или откройте проект через Ctrl+L", "Перетащите компонент из библиотеки" },
        { S_REPORT_ISSUE, "Report an issue", "Report an issue", "Сообщить о проблеме", "Сообщить о проблеме" },
        { S_PALETTE, "Command palette", "Commands", "Палитра команд", "Команды" },
        { S_PALETTE_HINT, "Type a command or component", "Search", "Введите команду или компонент", "Поиск" },
        { S_KIND_COMMAND, "Command", "Command", "Команда", "Команда" },
        { S_RECENT, "Recent", "Recent", "Недавние", "Недавние" },
        { S_PALETTE_FOOT, "Enter to run, arrows to move, Esc to close", "Enter / Esc", "Enter - выполнить, стрелки - выбор, Esc - закрыть", "Ввод / Esc" },
        { S_ZOOM_IN, "Zoom in", "Zoom in", "Приблизить", "Приблизить" },
        { S_ZOOM_OUT, "Zoom out", "Zoom out", "Отдалить", "Отдалить" },
        { S_SELECT_ALL, "Select all", "Select all", "Выделить всё", "Выделить всё" },
        { S_NET_HIGH, "High", "High", "Высокий", "Высокий" },
        { S_NET_LOW, "Low", "Low", "Низкий", "Низкий" },
        { S_NET_NC, "Not connected", "Floating", "Не подключено", "Не подкл." },
        { S_NET_INVALID, "Conflict", "Conflict", "Конфликт", "Конфликт" },
        { S_NET_NC_BODY, "Nothing drives this net yet.", "Nothing drives this net.", "Эту цепь пока ничто не питает.", "Цепь ничто не питает." },
        { S_NET_INVALID_BODY, "Two or more outputs drive this net, or a chip is wired wrong.", "Outputs collide.", "Цепь питают два и более выхода, или микросхема подключена неверно.", "Выходы конфликтуют." },
        { S_PIN_ON, "on", "on", "на", "на" }
    };
    if (countOut)
        *countOut = (int)(sizeof(kTable) / sizeof(kTable[0]));
    return kTable;
}

// Lookup with fallback: unknown language -> English, unknown key -> "".
inline const char *text(int key, int lang, int compact)
{
    int n = 0;
    const StrEntry *t = stringTable(&n);
    for (int i = 0; i < n; i++)
    {
        if (t[i].key != key)
            continue;
        if (lang == LANG_RU)
            return compact ? t[i].ruShort : t[i].ru;
        return compact ? t[i].enShort : t[i].en;
    }
    return "";
}

inline int clampLang(int v)
{
    if (v < 0 || v >= LANG_COUNT)
        return LANG_EN;
    return v;
}

inline int parseLang(const char *s)
{
    if (!s || !s[0])
        return LANG_EN;
    if (s[0] == '1')
        return LANG_RU;
    if ((s[0] == 'r' || s[0] == 'R') && (s[1] == 'u' || s[1] == 'U'))
        return LANG_RU;
    return LANG_EN;
}

inline const char *langCode(int lang)
{
    return lang == LANG_RU ? "ru" : "en";
}

inline const char *langName(int lang, int uiLang)
{
    if (lang == LANG_RU)
        return text(S_RUSSIAN, uiLang, 0);
    return text(S_ENGLISH, uiLang, 0);
}

inline int clampTheme(int v)
{
    if (v < 0 || v >= THEME_COUNT)
        return THEME_DARK;
    return v;
}

inline int clampAudio(int v)
{
    return v ? 1 : 0;
}

inline int clampTooltipMs(int ms)
{
    if (ms < 0) return 0;
    if (ms > 10000) return 10000;
    return ms;
}

// Theme colors live in UiTheme::palette(); these keep the old call sites
// and the settings tests readable.
inline int themeMenuBg(int theme)
{
    return UiTheme::palette(clampTheme(theme)).chrome;
}

inline int themeAccent(int theme)
{
    return UiTheme::palette(clampTheme(theme)).accent;
}

// Canvas flavor that suits a chrome theme; picked when the theme changes
// (the canvas stays its own persisted setting afterwards).
inline int themeWantsDarkCanvas(int theme)
{
    return clampTheme(theme) == THEME_LIGHT ? 0 : 1;
}

// Persisted values. Tooltip delay and audio mirror AtanuaConfig fields;
// language and theme are new.
struct Values
{
    int language;
    int theme;
    int tooltipMs;
    int audio;
    float uiScale;
    int canvasDark;
    int liveWires;
};

inline void defaults(Values &v)
{
    v.language = LANG_EN;
    v.theme = THEME_DARK;
    v.tooltipMs = 1500;
    v.audio = 1;
    v.uiScale = 1.0f;
    v.canvasDark = 1;
    v.liveWires = 1;
}

inline void validate(Values &v)
{
    v.language = clampLang(v.language);
    v.theme = clampTheme(v.theme);
    v.tooltipMs = clampTooltipMs(v.tooltipMs);
    v.audio = clampAudio(v.audio);
    v.uiScale = clampUiScale(v.uiScale);
    v.canvasDark = clampAudio(v.canvasDark);
    v.liveWires = clampAudio(v.liveWires);
}

inline int parseDec(const char *s, int fallback)
{
    if (!s || !s[0])
        return fallback;
    int neg = 0;
    int i = 0;
    if (s[0] == '-')
    {
        neg = 1;
        i = 1;
    }
    if (!s[i])
        return fallback;
    int v = 0;
    for (; s[i]; i++)
    {
        if (s[i] < '0' || s[i] > '9')
            return fallback;
        v = v * 10 + (s[i] - '0');
        if (v > 1000000)
            return fallback;
    }
    return neg ? -v : v;
}

inline float parseFloat(const char *s, float fallback)
{
    if (!s || !s[0])
        return fallback;
    int neg = 0;
    int i = 0;
    if (s[0] == '-')
    {
        neg = 1;
        i = 1;
    }
    long whole = 0;
    int digits = 0;
    while (s[i] >= '0' && s[i] <= '9')
    {
        whole = whole * 10 + (s[i] - '0');
        i++;
        if (++digits > 6)
            return fallback;
    }
    float v = (float)whole;
    if (s[i] == '.' || s[i] == ',')
    {
        i++;
        float place = 0.1f;
        int fdigits = 0;
        while (s[i] >= '0' && s[i] <= '9' && fdigits < 3)
        {
            v += (s[i] - '0') * place;
            place *= 0.1f;
            i++;
            fdigits++;
        }
        if (s[i] != 0)
            return fallback;
    }
    else if (s[i] != 0)
        return fallback;
    if (digits == 0)
        return fallback;
    return neg ? -v : v;
}

// XML element mapping. Tags match atanua.xml element names so the settings
// ride the existing config file; each carries its value in a "value"
// attribute as a decimal string.
inline int fieldCount()
{
    return 7;
}

inline const char *fieldTag(int i)
{
    switch (i)
    {
    case 0: return "Language";
    case 1: return "ThemeVariant";
    case 2: return "TooltipDelay";
    case 3: return "AudioEnable";
    case 4: return "UiScale";
    case 5: return "CanvasDark";
    case 6: return "LiveWires";
    default: return "";
    }
}

inline bool formatFloat2(float v, char *out, int cap)
{
    if (!out || cap < 2)
        return false;
    int neg = 0;
    if (v < 0)
    {
        neg = 1;
        v = -v;
    }
    int whole = (int)v;
    int frac = (int)((v - whole) * 100.0f + 0.5f);
    if (frac >= 100)
    {
        whole++;
        frac -= 100;
    }
    char buf[16];
    int len = 0;
    buf[len++] = (char)('0' + (frac % 10));
    buf[len++] = (char)('0' + ((frac / 10) % 10));
    buf[len++] = '.';
    if (whole == 0)
        buf[len++] = '0';
    else
    {
        int rev = 0;
        char tmp[8];
        while (whole > 0 && rev < 7)
        {
            tmp[rev++] = (char)('0' + (whole % 10));
            whole /= 10;
        }
        while (rev > 0)
            buf[len++] = tmp[--rev];
    }
    if (neg)
        buf[len++] = '-';
    if (len + 1 > cap)
        return false;
    for (int i = 0; i < len; i++)
        out[i] = buf[len - 1 - i];
    out[len] = 0;
    return true;
}

inline bool getField(const Values &v, const char *tag, char *out, int cap)
{
    if (!tag || !out || cap <= 0)
        return false;
    int val = 0;
    if (strcmp(tag, "Language") == 0)
        val = v.language;
    else if (strcmp(tag, "ThemeVariant") == 0)
        val = v.theme;
    else if (strcmp(tag, "TooltipDelay") == 0)
        val = v.tooltipMs;
    else if (strcmp(tag, "AudioEnable") == 0)
        val = v.audio;
    else if (strcmp(tag, "UiScale") == 0)
        return formatFloat2(v.uiScale, out, cap);
    else if (strcmp(tag, "CanvasDark") == 0)
        val = v.canvasDark;
    else if (strcmp(tag, "LiveWires") == 0)
        val = v.liveWires;
    else
        return false;
    // Decimal itoa without stdio so tests and app share one path.
    char buf[16];
    int len = 0;
    unsigned int u = (unsigned int)(val < 0 ? -val : val);
    do
    {
        buf[len++] = (char)('0' + (u % 10));
        u /= 10;
    } while (u > 0 && len < 15);
    if (val < 0 && len < 15)
        buf[len++] = '-';
    if (len + 1 > cap)
        return false;
    for (int i = 0; i < len; i++)
        out[i] = buf[len - 1 - i];
    out[len] = 0;
    return true;
}

inline bool setField(Values &v, const char *tag, const char *str)
{
    if (!tag || !str)
        return false;
    if (strcmp(tag, "Language") == 0)
    {
        // Accept "0"/"1" as well as "en"/"ru".
        if ((str[0] == 'r' || str[0] == 'R') && (str[1] == 'u' || str[1] == 'U'))
            v.language = LANG_RU;
        else if ((str[0] == 'e' || str[0] == 'E') && (str[1] == 'n' || str[1] == 'N'))
            v.language = LANG_EN;
        else
            v.language = clampLang(parseDec(str, LANG_EN));
        return true;
    }
    if (strcmp(tag, "ThemeVariant") == 0)
    {
        v.theme = clampTheme(parseDec(str, THEME_DARK));
        return true;
    }
    if (strcmp(tag, "TooltipDelay") == 0)
    {
        v.tooltipMs = clampTooltipMs(parseDec(str, 1500));
        return true;
    }
    if (strcmp(tag, "AudioEnable") == 0)
    {
        v.audio = clampAudio(parseDec(str, 1));
        return true;
    }
    if (strcmp(tag, "UiScale") == 0)
    {
        v.uiScale = clampUiScale(parseFloat(str, 1.0f));
        return true;
    }
    if (strcmp(tag, "CanvasDark") == 0)
    {
        v.canvasDark = clampAudio(parseDec(str, 1));
        return true;
    }
    if (strcmp(tag, "LiveWires") == 0)
    {
        v.liveWires = clampAudio(parseDec(str, 1));
        return true;
    }
    return false;
}

} // namespace AppSettings

#endif
