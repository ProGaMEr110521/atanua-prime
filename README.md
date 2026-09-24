# Atanua Prime — Real-Time Logic Simulator

**[English](#english) | [Русский](#russian)**

---

<a id="english"></a>
## English

Atanua Prime is a real-time logic simulator for education and digital-circuit experimentation. It is a modernized fork of [jarikomppa/atanua](https://github.com/jarikomppa/atanua), the open-source release of the former commercial product (2008–2014).

Place logic chips on the canvas, wire their pins together, and watch the circuit simulate live. The default simulation rate is 1 kHz (`PhysicsKHz` in `atanua.xml`).

### Download

Prebuilt packages are published on [Releases](https://github.com/ProGaMEr110521/atanua-prime/releases) when a `v*` version tag is pushed:

- Windows: `atanua-windows-x64.zip` (self-contained archive with runtime DLLs when needed)
- Linux: `atanua-linux-x86_64.tar.gz`

Plain branch pushes build in CI but do not publish release archives.

### What's new in Prime

- **Modern dark UI** — Dear ImGui chrome (top bar, chip palette, status bar, settings), responsive toolbar layout, searchable chip palette, and live status (chips, wires, nets, zoom, undo/redo depth).
- **Click-to-bend wiring** — left-click the middle of a wire to insert a bend point; drag the anchor to reposition it.
- **Route-through anchors** — while drawing a wire, click empty canvas to drop routing anchors; releasing near a pin snaps the connection to that pin.
- **Magnetic anchors and pins** — bend points render as visible dots with separate inner (start wire) and outer (move) hit zones; pins have padded grab areas that win over nearby wires.
- **Undo/redo** — snapshot history capped at 100 steps and 64 MiB total; moves, rotations, and edits record history; depth shown in the status bar.
- **Settings** — language (English / Russian), theme (Dark / Contrast), UI scale, tooltip delay, and sound; choices persist in `atanua.xml`.
- **Auto-update (Windows)** — on launch, the app checks GitHub releases and can download, install, and restart when a newer tagged build is available. Linux builds do not perform this background check.
- **CI** — every push and pull request builds on Windows and Ubuntu; tagged releases additionally package and publish both platform archives.

### Controls

| Action | How |
|---|---|
| Place chip | Drag from the left palette onto the canvas |
| Move chip / anchor | Drag the chip body (outer ring on anchors) |
| Start / finish a wire | Drag pin to pin, or click pin, click empty spots, click target pin |
| Bend a wire (Click-to-bend) | Left-click the middle of a wire; drag the created dot |
| Branch from a pin / anchor | Drag from the inner square |
| Rotate selected chip | `Ctrl+R` |
| Nudge selection | Arrow keys |
| Delete selection | `Delete` or `Ctrl+D` |
| Undo / Redo | `Ctrl+Z` / `Ctrl+Y` (also `Ctrl+Shift+Z`), or toolbar buttons |
| New / Open / Merge / Box / Save | `Ctrl+N` / `Ctrl+L` / `Ctrl+M` / `Ctrl+B` / `Ctrl+S` |
| Zoom to fit / Home / Snap / Live wires / Screenshot | `Ctrl+E` / `Ctrl+H` / `Ctrl+P` / `Ctrl+W` / `Ctrl+G` |
| Optimize box | `Ctrl+O` |
| Cancel current action | `Esc` |

Open **Shortcuts** from the top bar for the full list.

### Build from source

Requirements: CMake 3.20+, a C++17 compiler, SDL2, OpenGL, and TinyXML2.

**Windows** (Visual Studio 2022+, vcpkg for SDL2 and TinyXML2 via `vcpkg.json`):

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

**Linux**:

```bash
sudo apt install build-essential cmake ninja-build libsdl2-dev libtinyxml2-dev libgl1-mesa-dev libglu1-mesa-dev libgtk-3-dev pkg-config curl
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

If `data/` is present at configure time, CMake copies it next to the built executable. The app expects `data/` (fonts, chip textures) alongside the binary.

Run the binary:

- Windows: `build\Release\atanua.exe`
- Linux: `./build/atanua`

### Tests

```bash
python tests/test_modern_ui_and_fixes.py
```

The script runs structural checks against sources and, when a local build exists, exercises the binary and bundled test circuits under `tests/fixtures/`.

### Repository layout

| Path | Purpose |
|---|---|
| `src/` | Application and simulation code |
| `data/` | Runtime assets (textures, Vera bitmap fonts, DejaVu TTF for UI) |
| `third_party/imgui/` | Vendored Dear ImGui for settings and chrome |
| `tests/` | Regression tests and sample `.atanua` circuits |
| `CMakeLists.txt` | Primary build (recommended) |
| `makefile` | Legacy Linux makefile using vendored TinyXML and GLee (not maintained for Prime) |

### Changelog

See [CHANGELOG.md](CHANGELOG.md). Tagged release notes are taken from matching `## [vX]` sections when a version tag is published.

### Credits

Original Atanua by Jari Komppa (Sol / [jarikomppa](https://github.com/jarikomppa)). Prime keeps the simulation core and rebuilds the editor experience around it.

---

<a id="russian"></a>
## Русский

Atanua Prime — симулятор цифровой логики в реальном времени для учёбы и экспериментов. Это модернизированный форк [jarikomppa/atanua](https://github.com/jarikomppa/atanua), открытой версии бывшего коммерческого продукта (2008–2014).

Размещайте микросхемы на холсте, соединяйте выводы проводами и наблюдайте симуляцию вживую. Частота по умолчанию — 1 кГц (параметр `PhysicsKHz` в `atanua.xml`).

### Скачать

Готовые сборки публикуются на [Releases](https://github.com/ProGaMEr110521/atanua-prime/releases) при пуше тега `v*`:

- Windows: `atanua-windows-x64.zip` (архив со всем необходимым, включая DLL при необходимости)
- Linux: `atanua-linux-x86_64.tar.gz`

Обычные пуши веток собираются в CI, но архивы релизов не публикуют.

### Что нового в Prime

- **Современный тёмный интерфейс** — оболочка на Dear ImGui (верхняя панель, палитра, строка состояния, настройки), адаптивный тулбар, поиск по палитре и живая строка состояния (микросхемы, провода, цепи, масштаб, глубина undo/redo).
- **Изгибы в один клик** — клик по середине провода ставит точку изгиба; якорь можно перетащить.
- **Маршрутизация через якоря** — при ведении провода кликайте по пустому холсту для промежуточных якорей; отпускание рядом с выводом примагничивает соединение к нему.
- **Магнитные якоря и пины** — точки изгиба видны и имеют отдельные зоны для нового провода (внутри) и перемещения (снаружи); у выводов расширенная зона захвата.
- **Undo/redo** — до 100 шагов и 64 МиБ суммарно; перемещения, повороты и правки попадают в историю; глубина видна в строке состояния.
- **Настройки** — язык (English / Русский), тема (Тёмная / Контрастная), масштаб UI, задержка подсказок и звук; всё сохраняется в `atanua.xml`.
- **Автообновление (Windows)** — при запуске приложение проверяет релизы на GitHub и может скачать, установить и перезапуститься. Сборки для Linux фоновую проверку не выполняют.
- **CI** — каждый push и pull request собирается под Windows и Ubuntu; по тегам дополнительно публикуются архивы обеих платформ.

### Управление

| Действие | Как |
|---|---|
| Поставить микросхему | Перетащить из левой палитры |
| Переместить микросхему / якорь | Тащить за корпус (у якоря — за внешнее кольцо) |
| Начать / завершить провод | Тащить пин → пин, либо клики: пин, пустые места, целевой пин |
| Изогнуть провод | Клик по середине провода; перетащить точку |
| Ответвиться от пина / якоря | Тащить из внутреннего квадрата |
| Повернуть выбранное | `Ctrl+R` |
| Сдвинуть выбранное | Стрелки |
| Удалить выбранное | `Delete` или `Ctrl+D` |
| Отменить / вернуть (undo) | `Ctrl+Z` / `Ctrl+Y` (также `Ctrl+Shift+Z`) или кнопки тулбара |
| Новый / Открыть / Влить / Бокс / Сохранить | `Ctrl+N` / `Ctrl+L` / `Ctrl+M` / `Ctrl+B` / `Ctrl+S` |
| Вписать / Домой / Привязка / Живые провода / Скриншот | `Ctrl+E` / `Ctrl+H` / `Ctrl+P` / `Ctrl+W` / `Ctrl+G` |
| Оптимизировать бокс | `Ctrl+O` |
| Отменить текущее действие | `Esc` |

Полный список — в окне **Shortcuts** на верхней панели.

### Сборка из исходников

Нужны CMake 3.20+, компилятор C++17, SDL2, OpenGL и TinyXML2.

**Windows** (Visual Studio 2022+, vcpkg для SDL2 и TinyXML2 через `vcpkg.json`):

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

**Linux**:

```bash
sudo apt install build-essential cmake ninja-build libsdl2-dev libtinyxml2-dev libgl1-mesa-dev libglu1-mesa-dev libgtk-3-dev pkg-config curl
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Если каталог `data/` есть при конфигурации, CMake копирует его рядом с исполняемым файлом. Приложению нужен `data/` (шрифты, текстуры) рядом с бинарником.

Запуск:

- Windows: `build\Release\atanua.exe`
- Linux: `./build/atanua`

### Тесты

```bash
python tests/test_modern_ui_and_fixes.py
```

Скрипт проверяет исходники и, при наличии локальной сборки, запускает бинарник с тестовыми схемами из `tests/fixtures/`.

### Структура репозитория

| Путь | Назначение |
|---|---|
| `src/` | Код приложения и симуляции |
| `data/` | Ресурсы (текстуры, шрифты Vera, DejaVu TTF для UI) |
| `third_party/imgui/` | Встроенный Dear ImGui для настроек и оболочки |
| `tests/` | Регрессионные тесты и примеры `.atanua` |
| `CMakeLists.txt` | Основная сборка (рекомендуется) |
| `makefile` | Устаревший makefile с vendored TinyXML и GLee (для Prime не поддерживается) |

### Changelog

См. [CHANGELOG.md](CHANGELOG.md). Примечания к релизу берутся из секций `## [vX]` при публикации тега.

### Благодарности

Оригинальная Atanua — Jari Komppa (Sol / [jarikomppa](https://github.com/jarikomppa)). Prime сохраняет ядро симуляции и перестраивает редактор вокруг него.
