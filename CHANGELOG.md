# Changelog / Журнал изменений

This file drives the release pages: the section matching a pushed `v*` tag is published as that release's notes automatically (plain pushes still get generated notes when no section matches).

Файл управляет страницами релизов: секция совпавшего `v*`-тега автоматически публикуется как описание релиза.

## [Unreleased] / [В разработке]

### Added / Добавлено
- Quick-find filter autofocuses on open so typing starts at once with no click.
- Поле быстрого поиска получает фокус при открытии, ввод начинается сразу без клика.
- What is new popup: after an update the app shows the changelog sections for every version since the previous build once (remembered as SeenVersion in atanua.xml); Help reopens the current build notes any time, Esc closes. Every version bump must add its `## [vX]` section below so the popup and release pages have notes to show.
- Окно Что нового: после обновления приложение один раз показывает разделы журнала для всех версий после предыдущей сборки (запоминается как SeenVersion в atanua.xml); помощь открывает заметки текущей сборки в любое время, закрытие по Esc. Каждый бамп версии обязан добавлять свою секцию `## [vX]` ниже, чтобы окну и страницам релизов было что показать.

## [v1.3.141236] - 2026-10-05

### Added / Добавлено
- Quick-find palette: double-press Shift opens a filterable list of every component across all tabs; Enter drops the highlighted chip for canvas placement, Esc or canvas click dismisses.
- Быстрый поиск: двойное нажатие Shift открывает фильтруемый список всех компонентов со всех вкладок; Enter ставит подсвеченный чип для размещения на холсте, Esc или клик по холсту закрывает.
- Quick-find fixes: rows place on press so picks drag straight out like the sidebar, dismiss is edge-triggered with the press swallowed, mouse capture holds only while dragging, and the hotkey ignores nav focus (text entry still vetoes).
- Исправления поиска: строки ставятся по нажатию и тянутся наружу как из палитры, закрытие только по новому клику без побочных действий, захват мыши только при перетаскивании, хоткей не зависит от фокуса навигации (ввод текста по-прежнему блокирует).
- Classic wire style is now the default; the Modern/Classic radios show hover tips explaining which anchor square connects and which moves.
- Классический стиль проводов теперь по умолчанию; переключатели Современный/Классический показывают подсказки о том, какой квадрат якорей соединяет, а какой двигает.

## [v1.3.141235] - 2026-10-03

### Added / Добавлено
- Wire style switch (Modern/Legacy) in settings: modern connects anchor wires from the inner square and moves anchors from the outer area, legacy swaps the two squares (inner moves, outer connects); modern keeps click-to-bend; the choice persists in atanua.xml and shows in the status bar; Modern is the default.
- Переключатель стиля проводов (Современный/Классический) в настройках: современный соединяет провода якорей из внутреннего квадрата и двигает якоря за внешнюю зону, классический меняет квадраты местами (внутренний двигает, внешний соединяет); современный оставляет изгибы по клику; выбор сохраняется в atanua.xml и виден в строке состояния; по умолчанию Современный.
- First-start briefing with spotlight focus: a short tour of tools, components, settings, and help on the very first launch, with Back/Next, one-shot Skip (Esc works too), and a persisted seen-flag so it never nags again.
- Приветственный обзор при первом запуске с подсветкой областей: короткий тур по инструментам, компонентам, настройкам и помощи, с кнопками Назад/Далее, пропуском в один клик (Esc тоже работает) и флагом просмотра, чтобы больше не показываться.
- Briefing fixes: the step panel renders above the dim, the help spotlight rings the real ? button instead of Quit, the ? window offers a tutorial re-run, the status bar shows the active wire style, and the headless report carries the loaded wire mode.
- Исправления обзора: панель шагов поверх затемнения, подсветка помощи на настоящей кнопке ?, повтор обучения из окна помощи, стиль проводов в строке состояния, режим проводов в headless-отчёте.

## [v1.3.141234] - 2026-10-03

### Added / Добавлено
- Settings gains a manual update check: if the silent startup check failed (e.g. offline launch), the Updates row runs it again on demand.
- В настройках появилась ручная проверка обновлений: если тихая проверка при запуске не удалась (например запуск без сети), строка обновлений запускает её повторно.

## [v1.3.141233] - 2026-10-03

### Added / Добавлено
- Windows builds link the C runtime statically (x64-windows-static triplet): the exe imports only OS DLLs, so PCs without the VC++ redistributable — e.g. university labs — run it from a plain folder copy.
- Сборки Windows статически линкуют CRT (триплет x64-windows-static): exe зависит только от системных DLL, поэтому на ПК без VC++ redistributable — например учебных — программа запускается из обычной папки.
- AI circuit tooling: mined chip catalog (`tools/chips.json`, 269 chips with pinouts) and headless `atanua --validate` with machine-readable diagnostics and exit codes.
- Инструменты для ИИ-генерации схем: каталог компонентов (`tools/chips.json`, 269 чипов с распиновкой) и headless-проверка `atanua --validate` с машиночитаемой диагностикой и кодами выхода.
- Headless `atanua --simulate`: batch runs of the real propagation loop with `--set` stimuli and a JSON report (LED/net/pin states), no window needed.
- Headless-запуск `atanua --simulate`: пакетный прогон настоящего симулятора со стимулами `--set` и JSON-отчётом (состояния LED/цепей/пинов), окно не нужно.
- Automation docs and clean-scheme routing: `tools/README.md`, the Manhattan Z-router (`tools/route_wires.py`) that bends generated wiring to 90 degrees through Connection Pin anchors, and runnable examples under `tools/examples/`.
- Документация для автоматизации и чистые схемы: `tools/README.md`, Манхэттен-трассировщик (`tools/route_wires.py`), который гнёт сгенерированные провода под 90 градусов через якоря Connection Pin, и запускаемые примеры в `tools/examples/`.

## [v1.3.141232] - 2026-09-25

### Added / Добавлено
- Application and document icons from user artwork: multi-size app and .atanua icons embedded in the exe, file association points at the document icon, Explorer refreshes on toggle.
- Иконки приложения и документов по вашему арту: многоразмерные иконки вшиты в exe, ассоциация указывает на иконку документа, проводник обновляется при переключении.

## [v1.3.141231] - 2026-09-24

### Added / Добавлено
- Linux install layout: binary, data, `.desktop` entry, and hicolor icon via `cmake --install`. Config follows XDG at `~/.config/atanua/atanua.xml`, with the old working-directory file still read if present.
- Установка на Linux: бинарник, данные, `.desktop` и иконка hicolor через `cmake --install`. Конфиг по XDG в `~/.config/atanua/atanua.xml`, старый файл из рабочей папки по-прежнему читается, если он есть.
- Linux update check downloads release info with `curl`.
- Проверка обновлений на Linux скачивает сведения о релизе через `curl`.
- READMEs rewritten to match the current build, controls, and vendored libraries.
- README переписаны под текущую сборку, управление и vendored-библиотеки.

### Fixed / Исправлено
- Linux release builds static-link TinyXML2 even when the distro only ships a shared `libtinyxml2.so` (CMake FetchContent of TinyXML2 10.0.0). `libtinyxml2.a` is still preferred when present. Ubuntu CI keeps failing if `ldd` shows `libtinyxml2.so` or unresolved libraries.
- Linux-релизы линкуют TinyXML2 статически даже если в дистрибутиве есть только общий `libtinyxml2.so` (CMake FetchContent TinyXML2 10.0.0). Если есть `libtinyxml2.a`, он по-прежнему предпочтителен. Ubuntu CI по-прежнему валит сборку, если `ldd` показывает `libtinyxml2.so` или неразрешённые библиотеки.
- Linux in-app update: `ldd` preflight refuses packages with missing libraries; apply copies the current binary to `atanua.bak`, `execve`s the same absolute path, rolls back if the child dies within ~1.5s, and keeps the running session on failure instead of quitting into a dead launcher.
- Обновление на Linux: предпроверка `ldd` отклоняет пакеты с недостающими библиотеками; установка копирует текущий бинарник в `atanua.bak`, делает `execve` того же абсолютного пути, откатывается если новый процесс сразу умирает (~1.5 с), и при ошибке оставляет текущую сессию вместо выхода в мёртвый лаунчер.
- Linux data files resolve from the executable path (`/proc/self/exe`), so tarball, install prefix, and `/usr/share/atanua` launches work from any directory.
- Файлы данных на Linux находятся от пути к исполняемому файлу (`/proc/self/exe`), поэтому архив, установочный префикс и `/usr/share/atanua` запускаются из любой папки.
- Linux file dialogs no longer nest a GTK main loop; screenshots go to `~/Pictures`.
- Диалоги выбора файлов на Linux больше не вкладывают цикл GTK; скриншоты сохраняются в `~/Pictures`.

## [v1.3.141230] - 2026-09-23

### Added / Добавлено
- Open .atanua by double-click or drag-and-drop onto the window: shared extension-checked helper, dirty-canvas confirm before discarding work, per-user file association behind an explicit Settings toggle (HKCU, no admin), shipped `atanua.ico` and Linux `.desktop` with MimeType.
- Открытие .atanua двойным кликом или перетаскиванием в окно: общая проверка расширения, подтверждение при несохранённых изменениях, ассоциация файлов по явному тогглу в настройках (HKCU, без админа), `atanua.ico` и Linux `.desktop` с MimeType в поставке.
- External opens made reliable: non-ASCII paths, relative argv, no false filename in the title on failed loads; covered by scripted argv/drop trials.
- Надёжное открытие извне: не-ASCII пути, относительный argv и честный заголовок при неудачной загрузке; покрыто скриптовыми argv/drop-пробами.
- Russian consistency pass: distinct Out/Quit labels, unified Save wording, clearer zoom label, localized file dialogs and confirmations.
- Проверка русского языка: разные подписи Out/Quit, единое слово для сохранения, понятная подпись масштаба, переведённые диалоги и подтверждения.
- View split into persisted canvas background and wire mode: Settings gains Background (Dark/Paper) and user-name rows, the topbar button now toggles wires only, canvas corner shows title plus name without the promo link (credit moved to Support).
- Разделение View: фон холста и режим проводов стали независимыми настройками с сохранением, в настройках появились строки фона и имени, кнопка включает только провода, в углу холста остались название и имя без рекламной ссылки (упоминание переехало в поддержку).
- Settings layout fixes: rows no longer collide in the settings ID space, the long association label uses its compact form, and the shortcuts support block wraps to stay on screen.
- Исправления панели настроек: строки больше не сталкиваются в пространстве ID, длинная подпись ассоциации использует короткую форму, блок поддержки в окне сочетаний переносится и остаётся на экране.
- Zed/clangd project support: `.clangd` fallback flags and an exported compile database.
- Поддержка проектов Zed/clangd: запасные флаги `.clangd` и экспортируемая база компиляции.
- Support section in the shortcuts window: issues link plus developer email (`mailto:`), wrapped to keep the window on screen.
- Секция поддержки в окне горячих клавиш: ссылка на issues и почта разработчика (`mailto:`), текст переносится, чтобы окно не вылезало за экран.

## [v1.3.141229] - 2026-09-21

### Added / Добавлено
- Version stamp, no user-facing changes.
- Техническая сборка, видимых изменений нет.

## [v1.3.141228] - 2026-09-21

### Added / Добавлено
- Support link: GitHub issues link on the shortcuts page.
- Ссылка поддержки: ссылка на GitHub issues на странице сочетаний.

## [v1.3.141227] - 2026-09-21

### Added / Добавлено
- Top bar grouped by function with uniform button widths, separators, rounded controls and hover shortcut hints, plus a shortcuts reference window.
- Верхняя панель сгруппирована по функциям: одинаковая ширина кнопок в группе, разделители, скруглённые элементы, подсказки с хоткеями и окно-справка по сочетаниям.
- All twelve action buttons share one measured width with tighter toolbar spacing; separators slimmed.
- Все двенадцать кнопок действий одной измеренной ширины, компактные отступы панели.
- Light canvas palette reworked: soft paper background with readable grid instead of harsh white.
- Светлая тема холста переработана: мягкий бумажный фон с читаемой сеткой вместо резкого белого.

## [v1.3.141226] - 2026-09-20

### Added / Добавлено
- Top bar and chip sidebar drawn with Dear ImGui: buttons and rows auto-size to their labels (nothing can overflow), toggle states highlighted, tooltips with shortcuts, responsive one/two-row top bar by measured need; DejaVu 17/15px chrome fonts.
- Верхняя панель и список микросхем на Dear ImGui: кнопки и строки сами подстраиваются под подписи (ничего не вылезает), включённые состояния подсвечены, подсказки с хоткеями, верхняя панель в одну или две строки по измеренной ширине; шрифты DejaVu 17/15px.
- Status bar, hover tooltips, update progress and perf overlay drawn with Dear ImGui; the homegrown button/slider/textfield widgets are deleted, bitmap Vera fonts remain only for zoomable canvas chip graphics.
- Статус-бар, всплывающие подсказки, прогресс обновления и оверлей производительности на Dear ImGui; самописные виджеты удалены, растровые шрифты Vera остались только для масштабируемых надписей на схеме.
- Searchable chip palette: filter box narrows every chip list as you type (keystrokes go to the filter, never to canvas shortcuts).
- Поиск по палитре микросхем: поле фильтра сужает каждый список при вводе (клавиши идут в фильтр, а не в хоткеи холста).
- UI scale setting (Compact/Normal/Large) with live switch and persistence.
- Масштаб интерфейса (Мелкий/Обычный/Крупный) с применением на лету и сохранением.
- Settings panel now drawn with Dear ImGui (vendored v1.92.9b) and DejaVu Sans TTF: the window auto-sizes to any label length, so translated strings can never overflow their controls; keyboard navigation enabled.
- Панель настроек теперь рисуется через Dear ImGui (в составе v1.92.9b) шрифтом DejaVu Sans TTF: окно само подстраивается под длину подписей, и переводы больше не вылезают из элементов; включена навигация с клавиатуры.
- Settings panel in the top bar: interface language (English/Русский applied live), theme variant (Dark/Contrast), tooltip delay, and sound on/off — every choice persists in atanua.xml across restarts.
- Панель настроек в верхней панели: язык интерфейса (English/Русский применяется сразу), вариант темы (Тёмная/Контрастная), задержка подсказок и звук вкл/выкл — каждый выбор сохраняется в atanua.xml между запусками.
- Full auto-update: Yes downloads the release archive with a progress bar and Cancel, then installs, restarts the app automatically, and reports failures plainly.
- Полное автообновление: Yes скачивает архив релиза с прогресс-баром и кнопкой Cancel, затем устанавливает, перезапускает приложение и понятно сообщает об ошибках.
- Automatic update prompt on launch when a newer tag exists on `atanua-prime` (quiet otherwise), with the version and download link.
- Автоматическое предложение обновиться при запуске, если на `atanua-prime` есть тег новее (иначе тихо), с версией и ссылкой.
- Bilingual README and changelog-driven release notes (`## [vX]` sections publish as release pages).
- Двуязычные README и примечания к релизам из changelog (секции `## [vX]` публикуются на страницах релизов).

### Fixed / Исправлено
- Undo/redo restoring empty designs: snapshot names are actually written and read now; first undo after boot no longer wipes the canvas; rotates and drags record history (up to 100 steps).
- Undo/redo, очищавшие всё: имена снапшотов теперь реально пишутся и читаются; первое undo после запуска больше не стирает холст; повороты и перемещения записываются в историю (до 100 шагов).
- Status bar overlapping the sidebar: the sidebar now ends above the measured status strip; dead color defines, widget ID macro and homegrown widgets removed.
- Статус-бар больше не перекрывает боковую панель: панель заканчивается над измеренной полосой статуса; удалены мёртвые цвета, макрос ID и самописные виджеты.

## [v1.3.141225] - 2026-09-19

### Added / Добавлено
- Full auto-update flow: Yes downloads the release archive with a progress bar and Cancel, then installs, restarts the app automatically, and reports failures plainly.
- Полный цикл автообновления: Yes скачивает архив релиза с прогресс-баром и кнопкой Cancel, затем устанавливает, перезапускает приложение и понятно сообщает об ошибках.
- Release gating: tagged releases publish only when the built-in version matches the tag.
- Проверка релизов: тегированные релизы публикуются только при совпадении встроенной версии с тегом.

## [v1.3.141224] - 2026-09-19

### Added / Добавлено
- Undo history fixes: snapshot names are written and read, the first undo after boot no longer wipes the canvas, rotates and drags record history.
- Исправления истории undo: имена снапшотов пишутся и читаются, первое undo после запуска больше не стирает холст, повороты и перемещения записываются в историю.
- Release update check: the app notices newer release tags.
- Проверка обновлений: приложение замечает новые теги релизов.
- Bilingual README and changelog-driven release notes (`## [vX]` sections publish as release pages).
- Двуязычные README и примечания к релизам из changelog (секции `## [vX]` публикуются на страницах релизов).

## [v1.3.141223] - 2026-09-18

### Added / Добавлено
- Tagged release packages for Windows (`atanua-windows-x64.zip`) and Linux (`atanua-linux-x86_64.tar.gz`) published automatically by CI.
- Автоматическая публикация пакетов Windows (`atanua-windows-x64.zip`) и Linux (`atanua-linux-x86_64.tar.gz`) средствами CI.

### Fixed / Исправлено
- Ubuntu CI build: install GTK3 and link it so Linux file dialogs compile.
- Сборка Ubuntu CI: установка GTK3 и линковка, чтобы компилировались диалоги выбора файлов.

## [v1.3.141222] - 2026-09-18

### Added / Добавлено
- Automatic GitHub releases on version tags (Windows + Linux archives with runtime DLLs, fonts, and assets).
- Автоматические релизы GitHub по версионным тегам (архивы Windows + Linux с DLL, шрифтами и ресурсами).

## [v1.3.141221] - 2026-09-18

### Added / Добавлено
- First automated CI builds for Windows (MSVC) and Ubuntu (GCC) on every push.
- Первые автоматические CI-сборки под Windows (MSVC) и Ubuntu (GCC) на каждый пуш.
