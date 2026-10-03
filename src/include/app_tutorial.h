/*
Atanua first-start tutorial - pure step machine plus spotlight geometry.
No I/O and no UI toolkit here, so unit tests can drive the shipped logic:
steps, advance/skip, visibility from the persisted seen-flag, and the
screen rect each step spotlights (computed from layout metrics the app
passes in). Rendering lives in main.cpp.
*/
#ifndef APP_TUTORIAL_H
#define APP_TUTORIAL_H

namespace AppTutorial {

enum Step
{
    TUT_WELCOME = 0,
    TUT_TOOLS = 1,
    TUT_COMPONENTS = 2,
    TUT_SETTINGS = 3,
    TUT_HELP = 4,
    TUT_DONE = 5
};

inline int stepCount()
{
    return 5; /* welcome, tools, components, settings, help */
}

struct State
{
    int step;
    int seen;
};

inline void init(State &s, int seenFlag)
{
    s.step = TUT_WELCOME;
    s.seen = seenFlag ? 1 : 0;
}

/* Returns 1 when the advance finished the tour. */
inline int advance(State &s)
{
    if (s.step < TUT_HELP)
    {
        s.step++;
        return 0;
    }
    s.step = TUT_DONE;
    s.seen = 1;
    return 1;
}

inline void back(State &s)
{
    if (s.step > TUT_WELCOME && s.step < TUT_DONE)
        s.step--;
}

inline void skip(State &s)
{
    s.step = TUT_DONE;
    s.seen = 1;
}

inline int visible(const State &s)
{
    return (!s.seen && s.step != TUT_DONE) ? 1 : 0;
}

struct Text
{
    const char *titleEn;
    const char *titleRu;
    const char *bodyEn;
    const char *bodyRu;
};

/* Russian strings use Cyrillic plus Latin-1 punctuation only, matching
 * the bitmap font coverage the settings table already obeys. */
inline Text stepText(int step)
{
    switch (step)
    {
    case TUT_WELCOME:
    {
        Text t = {
            "Welcome to Atanua Prime",
            "Добро пожаловать в Atanua Prime",
            "This short tour shows where everything lives: "
            "tools on top, components on the left, settings and help "
            "on the right. Use Next, Back, or Skip any time.",
            "Короткий обзор: инструменты сверху, компоненты слева, "
            "настройки и помощь справа. Листайте кнопками Далее "
            "и Назад или нажмите Пропустить."
        };
        return t;
    }
    case TUT_TOOLS:
    {
        Text t = {
            "Tools",
            "Инструменты",
            "The top bar holds file actions, undo and redo, view "
            "toggles, and zoom. Hover any button for its shortcut.",
            "Верхняя панель: файлы, отмена и возврат, вид, масштаб. "
            "Наведите курсор на кнопку, чтобы увидеть подсказку."
        };
        return t;
    }
    case TUT_COMPONENTS:
    {
        Text t = {
            "Components",
            "Компоненты",
            "The left palette lists every chip. Type in the filter to "
            "narrow it, then drag a chip onto the canvas.",
            "Слева палитра всех микросхем. Введите текст в фильтр, "
            "чтобы сузить список, затем перетащите чип на холст."
        };
        return t;
    }
    case TUT_SETTINGS:
    {
        Text t = {
            "Settings",
            "Настройки",
            "The gear opens settings: language, theme, wire style, "
            "canvas background, and update checks.",
            "Шестеренка открывает настройки: язык, тема, стиль "
            "проводов, фон холста и проверка обновлений."
        };
        return t;
    }
    default:
    {
        Text t = {
            "Help",
            "Помощь",
            "The question mark opens the shortcuts window and support "
            "links. This was the last stop: press Finish to start "
            "building.",
            "Знак вопроса открывает окно сочетаний клавиш и ссылки "
            "поддержки. Это последний шаг: нажмите Готово, чтобы "
            "начать собирать схему."
        };
        return t;
    }
    }
}

struct Rect
{
    float x, y, w, h;
    int hasFocus;
};

/* Spotlight rect per step from live layout metrics. Welcome centers a
 * dialog with no focus; settings and help share the top-right cluster
 * with different widths so each highlight reads distinctly. */
inline Rect focusRect(int step, float scrW, float scrH,
    float toolkitW, float topbarH)
{
    Rect r = { 0, 0, 0, 0, 0 };
    if (step == TUT_TOOLS)
    {
        r.x = 0; r.y = 0; r.w = scrW; r.h = topbarH; r.hasFocus = 1;
    }
    else if (step == TUT_COMPONENTS)
    {
        r.x = 0; r.y = topbarH; r.w = toolkitW;
        r.h = scrH - topbarH; r.hasFocus = 1;
    }
    else if (step == TUT_SETTINGS)
    {
        r.w = scrW < 240.0f ? scrW : 240.0f;
        r.x = scrW - r.w; r.y = 0; r.h = topbarH; r.hasFocus = 1;
    }
    else if (step == TUT_HELP)
    {
        r.w = scrW < 80.0f ? scrW : 80.0f;
        r.x = scrW - r.w; r.y = 0; r.h = topbarH; r.hasFocus = 1;
    }
    return r;
}

} // namespace AppTutorial

#endif
