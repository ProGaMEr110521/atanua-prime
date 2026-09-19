# Changelog / Журнал изменений

This file drives the release pages: the section matching a pushed `v*` tag is published as that release's notes automatically (plain pushes still get generated notes when no section matches).

Файл управляет страницами релизов: секция совпавшего `v*`-тега автоматически публикуется как описание релиза.

## [Unreleased] / [В разработке]

### Added / Добавлено
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
