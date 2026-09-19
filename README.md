# Atanua Prime — Real-Time Logic Simulator

**[English](#english) | [Русский](#russian)**

---

<a id="english"></a>
## English

Atanua Prime is a real-time logic simulator for education and digital-circuit tinkering — a modernized fork of [jarikomppa/atanua](https://github.com/jarikomppa/atanua), the open-source release of a former commercial product (2008–2014).

Drop logic chips onto the canvas, wire their pins together, and watch the circuit simulate live at ~1 kHz.

### ⬇️ Download

Grab the latest build from [**Releases**](https://github.com/ProGaMEr110521/atanua-prime/releases) — Windows (`atanua-windows-x64.zip`, self-contained) and Linux (`atanua-linux-x86_64.tar.gz`) packages are published automatically for every version tag. The app itself also notifies you when a newer release is out (see Auto-update below).

### ✨ What's new in Prime

- **Modern dark UI** — restyled chrome, taller top bar with grouped actions, readable chip palette, live status bar (chips / wires / nets / zoom / undo depth), responsive toolbar that wraps into two rows on narrow windows.
- **Click-to-bend wiring** — left-click the middle of any wire to drop a bend point there; a marker previews exactly where it lands.
- **Route-through anchors** — while drawing a wire, click empty canvas to drop anchors mid-route and keep wiring; releasing near a pin snaps the connection onto it instead of stranding a stray pin.
- **Findable, magnetic anchors** — bend points render as permanent two-tone dots, grab magnetically, and split hover into an inner green square (start another wire) and an outer ring (move the anchor).
- **Confident pins** — every pin has a padded connection zone that wins over nearby wires, so starting a wire from a busy pin grabs the pin, not the wire.
- **Undo/redo that keeps its promises** — full snapshot history (up to 100 steps), moves and rotations recorded, live Undo/Redo depth in the status bar, safe no-ops on empty stacks.
- **Settings with Russian** — the top-bar Settings panel switches the whole chrome between English and Русский live, plus theme variant (Dark/Contrast), tooltip delay, and sound; everything persists in `atanua.xml`.
- **Auto-update** — on launch the app quietly checks `atanua-prime` releases and shows a one-time dialog with the new version and download link when you're behind; silent otherwise (never installs anything by itself).
- **CI you can trust** — every push builds on Windows and Ubuntu; version tags additionally publish tested release archives; scripted UI trials drive the real app and verify wiring/undo end to end.

### 🎮 Controls

| Action | How |
|---|---|
| Place chip | Drag it from the left palette onto the canvas |
| Move chip / anchor | Drag the chip body (outer ring on anchors) |
| Start / finish a wire | Drag pin → pin, or click pin, click empty spots, click target pin |
| Bend a wire | Left-click its middle; drag the created dot anywhere |
| Branch from a pin/anchor | Drag from its inner square |
| Rotate selected chip | `Ctrl+R` |
| Nudge selection | Arrow keys |
| Delete selection | `Delete` |
| Undo / Redo | `Ctrl+Z` / `Ctrl+Y` (also `Ctrl+Shift+Z`), or the toolbar buttons |
| New / Open / Merge / Box / Save | `Ctrl+N` / `Ctrl+L` / `Ctrl+M` / `Ctrl+B` / `Ctrl+S` |
| Zoom to fit / Home / Snap / Live wires / Screenshot | `Ctrl+E` / `Ctrl+H` / `Ctrl+P` / `Ctrl+W` / `Ctrl+G` |
| Optimize box | `Ctrl+O` |
| Cancel current action | `Esc` |

### 🔧 Build from source

Windows (Visual Studio 2022+, CMake, vcpkg handles SDL2/TinyXML2):

```bat
cmake -S . -B build
cmake --build build --config Release
```

Linux:

```bash
sudo apt install cmake ninja-build libsdl2-dev libtinyxml2-dev libgl1-mesa-dev libglu1-mesa-dev libgtk-3-dev pkg-config
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run the checks: `python tests/test_modern_ui_and_fixes.py` (compiles and runs the C++ unit harnesses with your own toolchain).

### 📜 Changelog

See [CHANGELOG.md](CHANGELOG.md) — every version tag publishes its section to the release page automatically.

### 🙏 Credits

Original Atanua by Jari Komppa (Sol / [jarikomppa](https://github.com/jarikomppa)), released as open source after its commercial life. Prime keeps the simulation core and rebuilds the experience around it.

---

<a id="russian"></a>
## Русский

Atanua Prime — симулятор цифровой логики в реальном времени для учёбы и экспериментов — модернизированный форк [jarikomppa/atanua](https://github.com/jarikomppa/atanua), открытой версии бывшего коммерческого продукта (2008–2014).

Размещайте логические микросхемы на холсте, соединяйте их выводы проводами и наблюдайте, как схема симулируется вживую с частотой около 1 кГц.

### ⬇️ Скачать

Свежая сборка — на странице [**Releases**](https://github.com/ProGaMEr110521/atanua-prime/releases): Windows (`atanua-windows-x64.zip`, всё включено) и Linux (`atanua-linux-x86_64.tar.gz`). Пакеты публикуются автоматически для каждого версионного тега. Приложение само сообщит, когда выйдет новая версия (см. «Автообновление»).

### ✨ Что нового в Prime

- **Современный тёмный интерфейс** — обновлённая тема, высокий тулбар со сгруппированными действиями, читаемая палитра микросхем, живая строка состояния (микросхемы / провода / цепи / масштаб / глубина undo), тулбар, который на узких окнах переносится в два ряда.
- **Изгибы в один клик** — клик левой кнопкой по середине провода ставит точку изгиба; маркер заранее показывает, куда она встанет.
- **Маршрутизация через якоря** — ведя провод, кликайте по пустому холсту, чтобы ставить якоря, не прерывая трассировку; отпускание рядом с выводом само примагничивает соединение к нему, а не бросает рядом лишний пин.
- **Заметные, магнитные якоря** — точки изгиба всегда видны (двухцветные точки), легко хватаются, а при наведении делятся на внутренний зелёный квадрат (начать новый провод) и внешнее кольцо (переместить якорь).
- **Уверенные пины** — у каждого вывода есть расширенная зона соединения, которая побеждает соседние провода: начиная провод от занятого пина, вы схватите пин, а не провод.
- **Честные undo/redo** — полная история снапшотов (до 100 шагов), перемещения и повороты записываются, глубина Undo/Redo видна в строке состояния, пустые стеки безопасны.
- **Настройки с русским языком** — панель Settings в верхней строке переключает весь интерфейс между English и Русский на лету, плюс вариант темы (Тёмная/Контрастная), задержка подсказок и звук; всё сохраняется в `atanua.xml`.
- **Автообновление** — при запуске приложение тихо проверяет релизы `atanua-prime` и один раз показывает диалог с новой версией и ссылкой, если вы отстали; иначе молчит (само ничего не устанавливает).
- **CI, которому можно верить** — каждый пуш собирается под Windows и Ubuntu; версионные теги дополнительно публикуют проверенные архивы; скриптовые UI-тесты гоняют настоящее приложение и проверяют соединения и undo end-to-end.

### 🎮 Управление

| Действие | Как |
|---|---|
| Поставить микросхему | Перетащить из левой палитры на холст |
| Переместить микросхему / якорь | Тащить за корпус (у якоря — за внешнее кольцо) |
| Начать / завершить провод | Тащить пин → пин, либо клик по пину, клики по пустым местам, клик по целевому пину |
| Изогнуть провод | Клик левой по его середине; перетащить созданную точку |
| Ответвиться от пина / якоря | Тащить из его внутреннего квадрата |
| Повернуть выбранное | `Ctrl+R` |
| Сдвинуть выбранное | Стрелки |
| Удалить выбранное | `Delete` |
| Отменить / вернуть | `Ctrl+Z` / `Ctrl+Y` (также `Ctrl+Shift+Z`) или кнопки тулбара |
| Новый / Открыть / Влить / Бокс / Сохранить | `Ctrl+N` / `Ctrl+L` / `Ctrl+M` / `Ctrl+B` / `Ctrl+S` |
| Вписать в экран / Домой / Привязка / Живые провода / Скриншот | `Ctrl+E` / `Ctrl+H` / `Ctrl+P` / `Ctrl+W` / `Ctrl+G` |
| Оптимизировать бокс | `Ctrl+O` |
| Отменить текущее действие | `Esc` |

### 🔧 Сборка из исходников

Windows (Visual Studio 2022+, CMake, vcpkg сам подтянет SDL2/TinyXML2):

```bat
cmake -S . -B build
cmake --build build --config Release
```

Linux:

```bash
sudo apt install cmake ninja-build libsdl2-dev libtinyxml2-dev libgl1-mesa-dev libglu1-mesa-dev libgtk-3-dev pkg-config
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Проверки: `python tests/test_modern_ui_and_fixes.py` (компилирует и запускает C++-тесты вашим же тулчейном).

### 📜 Changelog

См. [CHANGELOG.md](CHANGELOG.md) — секция каждой версии автоматически публикуется на странице релиза.

### 🙏 Благодарности

Оригинальная Atanua — Jari Komppa (Sol / [jarikomppa](https://github.com/jarikomppa)), открытая после коммерческой жизни проекта. Prime сохраняет симуляционное ядро и перестраивает опыт вокруг него.
