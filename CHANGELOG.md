# Changelog / Журнал изменений

This file drives the release pages: the section matching a pushed `v*` tag is published as that release's notes automatically (plain pushes still get generated notes when no section matches).

Файл управляет страницами релизов: секция совпавшего `v*`-тега автоматически публикуется как описание релиза.

## [Unreleased] / [В разработке]

### UI remake / Переделка интерфейса
- New application chrome in `src/core/ui_chrome.cpp`: header with File / Edit / View / Help menus, document name and icon actions; resizable component library with search across all categories, category tabs, family grouping, Recent parts and hover cards; status bar with simulation rate, counts, selection, Snap / Colored wires toggles and zoom; Settings with theme previews, a key-cap shortcuts sheet (F1) and About.
- Новая оболочка в `src/core/ui_chrome.cpp`: шапка с меню Файл / Правка / Вид / Справка, именем схемы и кнопками-иконками; библиотека компонентов с изменяемой шириной, поиском по всем категориям, вкладками, группами, «Недавними» и карточками; строка состояния с частотой, счётчиками, выделением, привязкой, цветными проводами и масштабом; Настройки с превью тем, шпаргалка клавиш (F1) и «О программе».
- Command palette (Ctrl+K): fuzzy search over every command and component in English and Russian; Enter runs a command or picks up a part to drop with a click.
- Палитра команд (Ctrl+K): нечёткий поиск по всем командам и компонентам на русском и английском; Enter выполняет команду или берёт компонент, который ставится кликом.
- Edit > Select all (Ctrl+A); View > Zoom in / Zoom out (PgUp / PgDn).
- Правка > Выделить всё (Ctrl+A); Вид > Приблизить / Отдалить (PgUp / PgDn).
- Design tokens in `src/include/ui_theme.h`: one palette per theme (Dark graphite with amber, new Light, Contrast) for chrome and canvas.
- Токены дизайна в `src/include/ui_theme.h`: одна палитра на тему (тёмная графитовая с янтарным акцентом, новая светлая, контрастная) для оболочки и холста.
- All chip, display and hardware art redrawn as generated vector sprites at 4x resolution (`tools/sprites/`), laid out from real pin coordinates; flat DIP packages, keycaps, switches, clock, LEDs, 7/16-seg and TIL309 displays, LED grid, logic probe (dark screen), stepper, audio DAC.
- Вся графика микросхем, индикаторов и «железа» перерисована генераторами в `tools/sprites/` в 4 раза детальнее, по реальным координатам пинов: корпуса DIP, клавиши, переключатели, генератор, светодиоды, 7/16-сегментные индикаторы и TIL309, LED-матрица, логический пробник (тёмный экран), шаговый двигатель, аудио-ЦАП.
- Canvas: infinite grid, outline selection, wires drawn at the art's stroke weight with round caps, hover cards for parts, pins and wires with live High / Low / Floating / Conflict tags, an empty-canvas start panel.
- Холст: бесконечная сетка, выделение контуром, провода толщиной как линии компонентов со скруглёнными концами, карточки для компонентов, пинов и проводов с живым сигналом, стартовая подсказка на пустом холсте.
- Type: Inter with its ss04 disambiguation frozen in (distinct I / l / 1) and JetBrains Mono for numbers and part names; canvas text is rasterized from these fonts at the on-screen size (`tools/fonts/`). Header and About use the app's "At" monogram as vectors.
- Шрифты: Inter с зашитым ss04 (различимые I / l / 1) и JetBrains Mono для чисел и названий; текст холста растеризуется этими шрифтами в экранном размере (`tools/fonts/`). В шапке и «О программе» векторная монограмма «At».

### Fixed in the UI remake / Исправлено при переделке интерфейса
- Canvas keys (Delete, arrows, Ctrl shortcuts) stopped working after clicking the toolbar or library until the canvas was clicked.
- Клавиши холста (Delete, стрелки, Ctrl-сочетания) переставали работать после клика по панели или библиотеке, пока не кликнуть по холсту.
- Zoomed-out textures shimmered and looked pixelated: the mipmap builder sampled one texel per block and ignored alpha.
- Уменьшенные текстуры мерцали и выглядели пиксельными: генератор мип-уровней брал один тексель на блок и не учитывал альфу.
- A quick click (press and release in one event batch) now registers on the canvas.
- Быстрый клик (нажатие и отпускание в одной пачке событий) теперь срабатывает на холсте.
- Clicks on menus or dialogs over the canvas no longer also click the circuit underneath.
- Клики по меню и окнам поверх холста больше не срабатывают на схеме под ними.
- Logic probe mouse mapping used a hard-coded 40 px header height.
- Логический пробник считал позицию мыши с зашитой высотой шапки 40 px.
- Linux file dialogs fell back to English titles.
- Диалоги выбора файлов на Linux показывали заголовки на английском.
- Windows detection keyed on MSVC only; the app now also builds with MinGW-w64.
- Windows определялся только по MSVC; теперь приложение собирается и через MinGW-w64.
- Linux install ships the 256 px icon instead of the 64 px one.
- Установка на Linux кладёт иконку 256 px вместо 64 px.

### Added / Добавлено
- Linux install layout: binary, data, `.desktop` entry, and hicolor icon via `cmake --install`. Config follows XDG at `~/.config/atanua/atanua.xml`, with the old working-directory file still read if present.
- Установка на Linux: бинарник, данные, `.desktop` и иконка hicolor через `cmake --install`. Конфиг по XDG в `~/.config/atanua/atanua.xml`, старый файл из рабочей папки по-прежнему читается, если он есть.
- Linux update check downloads release info with `curl`.
- Проверка обновлений на Linux скачивает сведения о релизе через `curl`.
- READMEs rewritten to match the current build, controls, and vendored libraries.
- README переписаны под текущую сборку, управление и vendored-библиотеки.
- Top bar grouped by function with uniform button widths, separators, rounded controls and hover shortcut hints, plus a shortcuts reference window.
- Верхняя панель сгруппирована по функциям: одинаковая ширина кнопок в группе, разделители, скруглённые элементы, подсказки с хоткеями и окно-справка по сочетаниям.
- All twelve action buttons share one measured width with tighter toolbar spacing; separators slimmed.
- Все двенадцать кнопок действий одной измеренной ширины, компактные отступы панели.
- Light canvas palette reworked: soft paper background with readable grid instead of harsh white.
- Светлая тема холста переработана: мягкий бумажный фон с читаемой сеткой вместо резкого белого.
- Open .atanua by double-click or drag-and-drop onto the window: shared extension-checked helper, dirty-canvas confirm before discarding work, per-user file association behind an explicit Settings toggle (HKCU, no admin), shipped `atanua.ico` and Linux `.desktop` with MimeType.
- Открытие .atanua двойным кликом или перетаскиванием в окно: общая проверка расширения, подтверждение при несохранённых изменениях, ассоциация файлов по явному тогглу в настройках (HKCU, без админа), `atanua.ico` и Linux `.desktop` с MimeType в поставке.
- External opens made reliable: non-ASCII paths, relative argv, no false filename in the title on failed loads; covered by scripted argv/drop trials.
- Надёжное открытие извне: не-ASCII пути, относительный argv и честный заголовок при неудачной загрузке; покрыто скриптовыми argv/drop-пробами.
- Russian consistency pass: distinct Out/Quit labels, unified Save wording, clearer zoom label, localized file dialogs and confirmations.
- Проверка русского языка: разные подписи Out/Quit, единое слово для сохранения, понятная подпись масштаба, переведённые диалоги и подтверждения.
- View split into persisted canvas background and wire mode: Settings gains Background (Dark/Paper) and user-name rows, the topbar button now toggles wires only, canvas corner shows title plus name without the promo link (credit moved to Support).
- Разделение View: фон холста и режим проводов стали независимыми настройками с сохранением, в настройках появились строки фона и имени, кнопка включает только провода, в углу холста остались название и имя без рекламной ссылки (упоминание переехало в поддержку).
- Application and document icons from user artwork: multi-size app and .atanua icons embedded in the exe, file association points at the document icon, Explorer refreshes on toggle.
- Иконки приложения и документов по вашему арту: многоразмерные иконки вшиты в exe, ассоциация указывает на иконку документа, проводник обновляется при переключении.
- Support section in the shortcuts window: issues link plus developer email (`mailto:`), wrapped to keep the window on screen.
- Секция поддержки в окне горячих клавиш: ссылка на issues и почта разработчика (`mailto:`), текст переносится, чтобы окно не вылезало за экран.

### Fixed / Исправлено
- Linux data files resolve from the executable path (`/proc/self/exe`), so tarball, install prefix, and `/usr/share/atanua` launches work from any directory.
- Файлы данных на Linux находятся от пути к исполняемому файлу (`/proc/self/exe`), поэтому архив, установочный префикс и `/usr/share/atanua` запускаются из любой папки.
- Linux file dialogs no longer nest a GTK main loop; screenshots go to `~/Pictures`.
- Диалоги выбора файлов на Linux больше не вкладывают цикл GTK; скриншоты сохраняются в `~/Pictures`.

## [v1.3.141231] - 2026-09-24

### Fixed / Исправлено
- Linux release builds static-link TinyXML2 even when the distro only ships a shared `libtinyxml2.so` (CMake FetchContent of TinyXML2 10.0.0). `libtinyxml2.a` is still preferred when present. Ubuntu CI keeps failing if `ldd` shows `libtinyxml2.so` or unresolved libraries.
- Linux-релизы линкуют TinyXML2 статически даже если в дистрибутиве есть только общий `libtinyxml2.so` (CMake FetchContent TinyXML2 10.0.0). Если есть `libtinyxml2.a`, он по-прежнему предпочтителен. Ubuntu CI по-прежнему валит сборку, если `ldd` показывает `libtinyxml2.so` или неразрешённые библиотеки.
- Linux in-app update: `ldd` preflight refuses packages with missing libraries; apply copies the current binary to `atanua.bak`, `execve`s the same absolute path, rolls back if the child dies within ~1.5s, and keeps the running session on failure instead of quitting into a dead launcher.
- Обновление на Linux: предпроверка `ldd` отклоняет пакеты с недостающими библиотеками; установка копирует текущий бинарник в `atanua.bak`, делает `execve` того же абсолютного пути, откатывается если новый процесс сразу умирает (~1.5 с), и при ошибке оставляет текущую сессию вместо выхода в мёртвый лаунчер.

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
