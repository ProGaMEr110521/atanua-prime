# Changelog / Журнал изменений

This file drives the release pages: the section matching a pushed `v*` tag is published as that release's notes automatically (plain pushes still get generated notes when no section matches).

Файл управляет страницами релизов: секция совпавшего `v*`-тега автоматически публикуется как описание релиза.

## [Unreleased] / [В разработке]

### Added / Добавлено
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
- Support section in the shortcuts window: issues link plus developer email (`mailto:`), wrapped to keep the window on screen.
- Секция поддержки в окне горячих клавиш: ссылка на issues и почта разработчика (`mailto:`), текст переносится, чтобы окно не вылезало за экран.

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
