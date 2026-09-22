"""Modern UI + bug-fix regression tests. Drives shipped sources and binary."""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_MAIN = os.path.join(REPO, "src", "core", "main.cpp")
SRC_SIM = os.path.join(REPO, "src", "core", "simutils.cpp")
SRC_FILEIO = os.path.join(REPO, "src", "core", "fileio.cpp")
SRC_FILEUTILS = os.path.join(REPO, "src", "core", "fileutils.cpp")
SRC_TOOLKIT = os.path.join(REPO, "src", "basecode", "toolkit.cpp")
SRC_EXTRAPIN = os.path.join(REPO, "src", "chip", "extrapin.cpp")
SRC_FONT = os.path.join(REPO, "src", "basecode", "angelcodefont.cpp")
THEME_H = os.path.join(REPO, "src", "include", "ui_theme.h")
CMAKE_LISTS = os.path.join(REPO, "CMakeLists.txt")
WORKFLOW = os.path.join(REPO, ".github", "workflows", "build.yml")
BUILD_EXE = os.path.join(REPO, "build", "Release", "atanua.exe")
DATA_DIR = os.path.join(REPO, "data")
def _circuit(name):
    # Vendored fixtures are committed; legacy public path is gitignored.
    cand = os.path.join(REPO, "tests", "fixtures", name)
    if os.path.exists(cand):
        return cand
    return os.path.join(REPO, "atanua_build_public", "tests", name)
TEST_CIRCUITS = [
    _circuit("baselogic.atanua"),
    _circuit("heavy.atanua"),
    _circuit("74181.atanua"),
]


def read(p):
    with open(p, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def test_modern_theme_is_default():
    main = read(SRC_MAIN)
    theme = read(THEME_H)
    for token in ["UI_TOPBAR_H", "C_TEXTDIM", "C_ACCENTTEXT",
                  "themeAccent", "toImVec", "gStatusH"]:
        assert token in main, f"modern theme token missing: {token}"
    for gone in ["#define C_MENUBG", "#define C_WIDGETBG", "#define C_WIDGETHOT",
                 "#define C_HOTROW", "#define GEN_ID"]:
        assert gone not in main, f"dead chrome token leftover: {gone}"
    toolkit = read(os.path.join(REPO, "src", "include", "toolkit.h"))
    assert "GEN_ID" not in toolkit, "dead widget ID macro leftover"
    assert "0xff3f4f4f" not in main, "old 2008 menubg still active"
    assert "UI_THEME_MENUBG" in theme and "UiTheme" in theme
    assert "UiTheme::wirePickTolerance" in main, "canvas theme helpers not used"
    assert "Status bar" in main or "Chips:%d" in main
    for token in ["draw_topbar_imgui", "draw_sidebar_imgui", "ImGui::Selectable",
                  "AlwaysAutoResize", "GetGlyphRangesCyrillic", "###tb",
                  "settings_radio", "PushID"]:
        assert token in main, f"imgui chrome missing: {token}"
    for gone in ["topbarLayout", "TopbarLayout", "topbarHeight", "compactLabels",
                 "slidervalue", "tb.settingsX", "xofs += tabW", "xofs += btnW"]:
        assert gone not in main and gone not in theme, f"manual layout leftover: {gone}"


def test_toolkit_and_font_guards():
    toolkit = read(SRC_TOOLKIT)
    assert "if (!aString)" in toolkit, "mystrdup null guard missing"
    for dead in ["int imgui_button(", "int imgui_slider(", "int imgui_textfield(",
                 "regionhit"]:
        assert dead not in toolkit, f"dead widget leftover: {dead}"
    font = read(SRC_FONT)
    assert "if (!string" in font, "font null guard missing"
    assert "findcharblock" in font and "return NULL" in font


def test_fileutils_bounds():
    src = read(SRC_FILEUTILS)
    assert "bytesRemaining" in src, "MemoryFile bounds helper missing"
    assert "mDataIdx >= (int)mData.size()" in src, "OOB read guard missing"
    assert "if (!f)" in src, "FILE null guard missing"


def test_sim_crash_guards():
    sim = read(SRC_SIM)
    assert "if (!c)" in sim and "if (idx < 0)" in sim, "delete_chip guard missing"
    assert "gFryList.clear()" in sim, "fry list reset missing"
    assert "undoMaxEntries" in sim and "snapshotBytes" in sim, "memory-bounded history missing"
    assert "gWireStartDrag = NULL" in sim, "stale drag pin must clear on cancel"
    assert "try" in sim and "clear_stack(gRedoStack)" in sim
    assert "while (i < gChip.size())" in sim, "optimize_box safe loop missing"


def test_undo_covers_every_mutation():
    main = read(SRC_MAIN)
    i = main.index("void do_rotate()")
    assert "save_undo()" in main[i:i + 600], "rotate must record undo"
    assert "sMoveUndoSaved" in main, "drag moves must record undo"
    assert "shouldSaveNudge" in main, "nudge coalescing missing"
    sim = read(SRC_SIM)
    assert "save_undo();" in sim[sim.index("void do_resetdialog"):sim.index("void do_resetdialog") + 400]


def test_fileio_roundtrip_guards():
    src = read(SRC_FILEIO)
    assert src.count("build_nets();") >= 8, "failure paths must rebuild nets"
    assert "chipcount < 0 || chipcount > 50000" in src, "count sanity missing"
    assert "actualChips" in src and "actualWires" in src, "box cache count fix missing"


def test_main_interaction_guards():
    main = read(SRC_MAIN)
    for needle in [
        "if (wireid < 0 || wireid >= (int)gWire.size())",
        "if (!w || !w->mFirst",
        "GET_WIRE_ID(gUIState.hotitem) : -1",
        "if (kbdWire < 0 || kbdWire >= (int)gWire.size())",
        "SDL_GetWindowSize",
        "if (want)",
        "cursor_normal",
        "font assets missing",
        "gTopbarH",
        "draw_topbar_imgui",
        "draw_sidebar_imgui",
        "split_wire_middle_at",
        "drop_routing_anchor_at",
        "find_release_pin",
        "find_anchor_near",
        "sMoveUndoSaved",
        "shouldSaveNudge",
        "Undo:%d Redo:%d",
        "wirePickTolerance",
        "pinGrabPad",
        "sClickWire",
        "drops a bend point",
    ]:
        assert needle in main, f"main guard missing: {needle}"


def test_wire_bend_helpers():
    theme = read(THEME_H)
    for token in ["wirePickTolerance", "wireEndTolerance", "snapWorld", "anchorGrabPad", "anchorHotZone", "pinGrabPad", "shouldSaveNudge", "wireFinishSnap", "undoMaxEntries", "undoMaxBytes"]:
        assert token in theme, f"bend helper missing: {token}"


def test_ubuntu_ci_has_gtk():
    # nativefunctions.cpp includes <gtk/gtk.h> on Linux, so CI must
    # install it and CMake must wire its flags, or ubuntu stays red.
    wf = read(WORKFLOW)
    assert "libgtk-3-dev" in wf or "libgtk2.0-dev" in wf, "workflow missing GTK dev package"
    cmake = read(CMAKE_LISTS)
    assert "gtk" in cmake.lower(), "CMake missing GTK wiring for Linux"


def test_workflow_publishes_tagged_releases():
    # Tag pushes must package both platforms and publish a release;
    # plain pushes must stay build-only. data/ must ship (app won't run).
    wf = read(WORKFLOW)
    assert "refs/tags/v" in wf, "no tag gate for releases"
    assert "gh release create" in wf, "no release publish step"
    assert wf.count("actions/checkout@v4") >= 3, "release job needs its own checkout for notes"
    assert "upload-artifact" in wf, "builds not uploaded for release"
    assert "atanua.exe" in wf and "data" in wf, "windows package incomplete"
    assert "dumpbin" in wf, "windows package must resolve runtime DLLs, not hardcode vcpkg paths"
    assert "CHANGELOG.md" in wf, "release must publish changelog notes"
    assert "--notes" in wf, "release must prefer changelog notes over generated ones"
    assert "ATANUAVERSION" in wf, "release must verify version matches tag"
    assert os.path.isfile(os.path.join(REPO, "data", "vera14.fnt")), "data assets missing from checkout"
    assert os.path.isfile(os.path.join(REPO, "data", "vera31.fnt")), "data assets missing from checkout"


def test_readme_and_changelog():    # Front page is bilingual and documents the shipped features;
    # the changelog carries an Unreleased section plus tag sections
    # so every tagged release page shows real notes.
    readme = read(os.path.join(REPO, "README.md"))
    assert "# Atanua Prime" in readme, "readme title missing"
    assert "Русский" in readme, "readme must have a Russian section"
    assert "## Русский" in readme, "russian section header missing"
    for token in ["Click-to-bend", "Undo", "Auto-update", "Ctrl+Z",
                  "Изгибы", "undo", "Автообновление", "Ctrl+Z"]:
        assert token in readme, f"readme missing documented feature: {token}"
    changelog = read(os.path.join(REPO, "CHANGELOG.md"))
    assert "## [Unreleased]" in changelog, "changelog needs an Unreleased section"
    assert "## [v1.3.141223]" in changelog, "changelog missing published tag section"
    assert "## [v1.3.141222]" in changelog, "changelog missing published tag section"


def test_dropfile_units():
    # Compiles the SHIPPED drop/argv helper header and drives extension
    # checks, drop acceptance and basename handling without a window.
    _compile_and_run(os.path.join(REPO, "tests", "test_dropfile.cpp"),
                     [],
                     "ALL DROPFILE TESTS PASSED")


def test_dropfile_wired_into_app():
    # argv double-click and window drag-and-drop must flow through the
    # shared helper: extension-checked, dirty-guarded, SDL path freed.
    main = read(SRC_MAIN)
    assert '#include "dropfile.h"' in main, "drop helper header not included"
    assert "case SDL_DROPFILE:" in main, "drop event not handled"
    assert "SDL_free(event.drop.file)" in main, "SDL drop path never freed"
    assert 'SDL_EventState(SDL_DROPFILE, SDL_ENABLE)' in main, "drop events not enabled"
    assert "open_external_file" in main, "shared open helper missing"
    assert "DropFile::shouldAcceptDrop" in main, "drop path not extension-checked"
    assert "DropFile::baseName" in main, "dirty prompt lacks file name"
    assert "canvas_is_dirty" in main, "dirty-canvas guard missing"
    assert "open_external_file(sArgvPath.c_str())" in main, "argv[1] bypasses the helper"
    assert "do_loaddialog(0, args[1])" not in main, "argv[1] still opens unchecked"
    assert "do_loaddialog(0, sArgvPath" not in main, "argv still opens unchecked"
    sim = read(SRC_SIM)
    assert "atanua_fopen_rb(aFilename)" in sim, "loader must open via UTF-8 helper"
    assert "if (!fh)" in sim, "loader must not claim failed opens"
    assert "_getcwd" in main or "getcwd" in main, "argv relative-path resolve missing"
    assert "gotoappdirectory(argc, args)" in main, "startup chdir missing"
    dropfile = read(os.path.join(REPO, "src", "include", "dropfile.h"))
    assert "shouldAcceptDrop" in dropfile and "hasAtanuaExtension" in dropfile, \
        "helper API incomplete"


def test_fileassoc_units():
    # Compiles the SHIPPED association helper header and drives ProgID
    # layout, command/icon formatting and exe matching without registry.
    _compile_and_run(os.path.join(REPO, "tests", "test_fileassoc.cpp"),
                     [],
                     "ALL FILEASSOC TESTS PASSED")


def test_fileassoc_wired_into_app():
    # Association must live behind an explicit Settings toggle (HKCU, no
    # admin), never write at startup; icon + .desktop must ship; the
    # Support section must carry both issues and email links.
    main = read(SRC_MAIN)
    assert '#include "dropfile.h"' in main, "drop helper header not included"
    assert "S_FILEASSOC" in main, "no association toggle in settings"
    assert "assocState(0)" in main, "toggle never reads association state"
    assert "assocInstall(0)" in main, "toggle never installs association"
    assert "assocRemove()" in main, "toggle never removes association"
    assert "mailto:vladtem3943@gmail.com" in main, "email link missing"
    assert "S_EMAIL" in main, "email string key missing"
    assert "atanua-prime/issues" in main, "issues link missing"
    native = read(os.path.join(REPO, "src", "core", "nativefunctions.cpp"))
    assert "HKEY_CURRENT_USER" in native, "association must stay per-user (HKCU)"
    assert "assocInstall" in native and "assocRemove" in native, "assoc impl missing"
    for token in ["assocReadString", "assocWriteString", "assocDeleteKey",
                  "currentExePath", "assocState"]:
        assert token in native, f"assoc helper missing: {token}"
    assert "RegDeleteTreeA" in native, "remove must clean the ProgID tree"
    internal = read(os.path.join(REPO, "src", "include", "atanua_internal.h"))
    for token in ["assocReadString", "assocWriteString", "assocDeleteKey",
                  "assocState", "assocInstall", "assocRemove", "currentExePath"]:
        assert token in internal, f"missing assoc decl: {token}"
    assert os.path.isfile(os.path.join(REPO, "atanua.ico")), "atanua.ico not shipped"
    assert os.path.isfile(os.path.join(REPO, "atanua.desktop")), "linux .desktop not shipped"
    desktop = read(os.path.join(REPO, "atanua.desktop"))
    assert "MimeType=application/x-atanua;" in desktop, ".desktop lacks MimeType"
    assert "%f" in desktop, ".desktop cannot receive dropped files"
    wf = read(WORKFLOW)
    assert "atanua.ico" in wf, "windows package must ship the icon"
    assert "atanua.desktop" in wf, "linux package must ship the .desktop file"


def test_settings_layout_no_overlap():
    # Loose rows sharing a visible label (Sound on/off vs file-assoc
    # on/off) collided in ImGui's ID space and raised the conflicting-ID
    # error, and the long assoc label ran under its own radios.
    main = read(SRC_MAIN)
    assert "PushID(idScope)" in main, "radio rows lack an ID scope"
    assert main.count("settings_radio(AppSettings::S_SOUND,") == 2, \
        "sound radios lost their row scope"
    assert main.count("settings_radio(AppSettings::S_FILEASSOC,") == 2, \
        "assoc radios lost their row scope"
    assert "text(AppSettings::S_FILEASSOC, lang, 1)" in main, \
        "assoc row must use the compact label form"
    # The shortcuts support block shows each address once and wraps so
    # the auto-sized window stays on screen in both languages.
    assert 'TextUnformatted("vladtem3943@gmail.com")' not in main, \
        "plain-text email duplicate is back"
    assert "PushTextWrapPos" in main and "PopTextWrapPos" in main, \
        "support text is not wrapped"


def test_reset_saves_only_on_confirm():
    sim = read(SRC_SIM)
    i = sim.index("void do_resetdialog")
    block = sim[i:i + 400]
    assert block.index("okcancel") < block.index("save_undo"), "reset must save only after confirm"


_msvc_env_cache = None

def _msvc_env():
    # Capture a real Developer Prompt environment so cl runs without a
    # wrapper batch file (which would mangle non-ASCII paths).
    global _msvc_env_cache
    if _msvc_env_cache is not None:
        return _msvc_env_cache
    vsdev = r"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
    bat = os.path.join(tempfile.mkdtemp(prefix="atanua_env_"), "env.bat")
    with open(bat, "w", encoding="ascii") as f:
        f.write(f'call "{vsdev}" -arch=x64 -host_arch=x64 >NUL\nset\n')
    p = subprocess.run(["cmd", "/c", bat], capture_output=True)
    env = {}
    raw = p.stdout
    text = None
    for enc in ("utf-8", "cp866", "cp1251"):
        try:
            text = raw.decode(enc)
            break
        except Exception:
            continue
    assert text is not None, "could not decode VsDevCmd environment"
    for line in text.splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            env[k.strip()] = v
    assert "INCLUDE" in env and "LIB" in env, "VsDevCmd did not yield a compiler env"
    _msvc_env_cache = env
    return env


def _find_cl(path_env=None):
    # Derive cl.exe from the MSVC include roots in the captured Developer
    # Prompt environment (VsDevCmd output), falling back to PATH lookup.
    # Directory walking proved unreliable here; direct probes are stable.
    import shutil
    cands = []
    try:
        inc = _msvc_env().get("INCLUDE", "")
        for part in inc.split(";"):
            p = part.strip().rstrip("\\/")
            if p.lower().endswith("\\include"):
                base = p[: -len("\\include")]
                cands.append(os.path.join(base, "bin", "Hostx64", "x64", "cl.exe"))
    except Exception:
        pass
    for cand in cands:
        try:
            if os.path.getsize(cand) > 0:
                return cand
        except OSError:
            continue
    if path_env is None:
        path_env = os.environ.get("PATH", "")
    found = shutil.which("cl.exe", path=path_env)
    assert found, "no MSVC cl.exe found"
    return found


def _sdl_include():
    cands = [
        os.path.join(REPO, "build", "vcpkg_installed", "x64-windows", "include"),
    ]
    root = os.environ.get("VCPKG_INSTALLATION_ROOT")
    if root:
        cands.append(os.path.join(root, "installed", "x64-windows", "include"))
    cands.append("/usr/include")
    for c in cands:
        if os.path.isfile(os.path.join(c, "SDL2", "SDL_endian.h")):
            return c
    return None


def _compile_and_run(test_cpp, extra_sources, run_marker):
    # Builds the given test against the given shipped sources with the
    # project's own toolchain and runs it. Proves the shipped code.
    tmp = tempfile.mkdtemp(prefix="atanua_cxx_")
    try:
        exe = os.path.join(tmp, "t.exe" if os.name == "nt" else "t")
        if os.name == "nt":
            env = dict(os.environ)
            env.update(_msvc_env())
            sdl = _sdl_include()
            assert sdl, "no SDL headers for standalone compile"
            cmd = [_find_cl(env.get("PATH", "")), "/nologo", "/EHsc",
                   f"/I{os.path.join(REPO, 'src', 'include')}",
                   f"/I{os.path.join(REPO, 'src')}",
                   f"/I{sdl}", test_cpp] + extra_sources + [f"/Fe{exe}"]
            p = subprocess.run(cmd, capture_output=True, env=env, timeout=180)
            def dec(b):
                for enc in ("utf-8", "cp866", "cp1251"):
                    try:
                        return b.decode(enc)
                    except Exception:
                        continue
                return b.decode("utf-8", errors="replace")
            out, err = dec(p.stdout), dec(p.stderr)
            assert p.returncode == 0, f"compile failed:\n{out}\n{err}"
            r2 = subprocess.run([exe], capture_output=True, timeout=30)
            out2 = dec(r2.stdout)
            assert r2.returncode == 0, f"test binary failed:\n{out2}\n{dec(r2.stderr)}"
            assert run_marker in out2, f"missing pass marker:\n{out2}"
        else:
            inc = ["-I" + os.path.join(REPO, "src", "include"),
                   "-I" + os.path.join(REPO, "src")]
            sdl = _sdl_include()
            if sdl and sdl != "/usr/include":
                inc.append("-I" + sdl)
            subprocess.run(["c++", "-std=c++17"] + inc + [test_cpp] + extra_sources +
                           ["-o", exe], check=True, capture_output=True, timeout=180)
            r2 = subprocess.run([exe], capture_output=True, text=True, timeout=30)
            assert r2.returncode == 0, f"test binary failed:\n{r2.stdout}\n{r2.stderr}"
            assert run_marker in r2.stdout, f"missing pass marker:\n{r2.stdout}"
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_fileutils_roundtrip():
    # Compiles the SHIPPED fileutils.cpp and drives its snapshot path.
    _compile_and_run(os.path.join(REPO, "tests", "test_fileutils_roundtrip.cpp"),
                     [os.path.join(REPO, "src", "core", "fileutils.cpp")],
                     "ALL FILEUTILS TESTS PASSED")


def test_settings_units():
    # Compiles the SHIPPED settings/language header and drives language
    # mapping, EN/RU lookup, validation and config field round-trip.
    _compile_and_run(os.path.join(REPO, "tests", "test_settings.cpp"),
                     [],
                     "ALL SETTINGS TESTS PASSED")


def test_settings_wired_into_app():
    # Language + settings must flow through the real paths: config fields
    # with save(), localized topbar labels, a settings panel that
    # persists, UTF-8 font decoding and a shipped Cyrillic glyph page.
    main = read(SRC_MAIN)
    assert '#include "app_settings.h"' in main, "settings header not included"
    assert "gSettingsOpen" in main, "settings open flag missing"
    assert "topbar_btn(AppSettings::S_NEW" in main, "action labels not localized"
    assert "AppSettings::S_BASE" in main, "tabs not localized"
    assert "S_SETTINGS" in main and "draw_topbar_imgui" in main, "no settings entry point in topbar"
    assert "draw_settings_panel" in main, "settings panel missing"
    assert "gConfig.save()" in main, "panel never persists"
    assert "S_SOUND_RESTART_NOTE" in main, "restart note missing"
    assert "gSettingsOpen = 0" in main, "panel has no close path"
    font = read(os.path.join(REPO, "src", "basecode", "angelcodefont.cpp"))
    assert "acfont_nextcode" in font, "font has no UTF-8 decoder"
    cfg = read(os.path.join(REPO, "src", "core", "AtanuaConfig.cpp"))
    assert "mLanguage" in cfg and "mThemeVariant" in cfg, "config fields missing"
    assert "void AtanuaConfig::save()" in cfg, "config save() missing"
    assert '"Language"' in cfg and '"ThemeVariant"' in cfg, "new XML elements missing"
    assert "isKnownConfigElement" in cfg, "save() must preserve unknown elements"
    header = read(os.path.join(REPO, "src", "include", "atanua.h"))
    assert "void save();" in header and "mLanguage;" in header, "config decl missing"
    theme = read(THEME_H)
    assert "TopbarLayout" not in theme and "topbarLayout" not in theme, \
        "manual topbar math must stay deleted"
    assert "draw_topbar_need" in main, "no measured row wrap"
    # The shipped font must carry Cyrillic glyphs the RU strings need
    # (the original page was Latin-only). They live on the same single
    # texture page so the cached draw path stays on one texture.
    import struct as _struct
    with open(os.path.join(DATA_DIR, "vera14.fnt"), "rb") as f:
        blob = f.read()
    assert blob[:3] == b"BMF", "font magic broken"
    assert b"vera14_01.png" not in blob, "font must stay single-page for the text cache"
    p = 4
    assert blob[p] == 1
    isz = _struct.unpack("<i", blob[p + 1:p + 5])[0]
    p += 5 + (isz - 4)
    assert blob[p] == 2
    csz = _struct.unpack("<i", blob[p + 1:p + 5])[0]
    scalew = _struct.unpack("<H", blob[p + 5 + 4:p + 5 + 6])[0]
    assert scalew == 256, f"cyrillic page not folded in, scaleW={scalew}"
    p += 5 + (csz - 4)
    assert blob[p] == 3
    psz = _struct.unpack("<i", blob[p + 1:p + 5])[0]
    assert blob[p + 5:p + 5 + (psz - 4)] == b"vera14_00.png\x00", "font page renamed"
    p += 5 + (psz - 4)
    assert blob[p] == 4
    hsz = _struct.unpack("<i", blob[p + 1:p + 5])[0]
    p += 5
    n = (hsz - 4) // 18
    assert n == 284, f"expected 218 latin + 66 cyrillic glyphs, got {n}"
    pages = set()
    cyr = 0
    for j in range(n):
        e = _struct.unpack("<hhhhhhhhBB", blob[p + j * 18:p + j * 18 + 18])
        pages.add(e[8])
        if 0x400 <= e[0] <= 0x45F:
            cyr += 1
    assert pages == {0}, f"glyphs span pages: {pages}"
    assert cyr == 66, f"cyrillic coverage lost: {cyr}"


    # Dear ImGui chrome (stage 1): vendored lib + backends, TTF font with
    # Cyrillic, and the settings panel drawn through ImGui beside the
    # kept canvas.
    imgui_h = os.path.join(REPO, "third_party", "imgui", "imgui.h")
    assert os.path.isfile(imgui_h), "vendored imgui.h missing"
    assert "1.92" in read(imgui_h), "unexpected vendored imgui version"
    for name in ["imgui_impl_sdl2.cpp", "imgui_impl_opengl2.cpp"]:
        assert os.path.isfile(os.path.join(REPO, "third_party", "imgui", "backends", name)), \
            f"imgui backend missing: {name}"
    assert os.path.isfile(os.path.join(REPO, "third_party", "imgui", "LICENSE.txt")), \
        "imgui license missing"
    assert os.path.isfile(os.path.join(DATA_DIR, "fonts", "DejaVuSans.ttf")), \
        "settings TTF missing"
    assert os.path.isfile(os.path.join(DATA_DIR, "fonts", "LICENSE_DEJAVU")), \
        "font license missing"
    for token in ["ImGui::CreateContext", "ImGui_ImplSDL2_ProcessEvent",
                  "ImGui_ImplSDL2_NewFrame", "ImGui_ImplOpenGL2_RenderDrawData",
                  "GetGlyphRangesCyrillic", "settings_radio", "ImGui::Begin",
                  "AlwaysAutoResize", "##statusbar", "SetTooltip", "ProgressBar",
                  "InputTextWithHint", "name_matches", "imgui_wants_keys",
                  "FontScaleMain", "S_UISCALE", "uiScalePreset", "draw_topbar_need",
                  "FrameRounding", "topbar_sep", "S_SHORTCUTS", "draw_shortcuts_window",
                  "gShortcutsOpen", "draw_topbar_tabs", "topbar_group_w", "0.906f"]:
        assert token in main, f"imgui wiring missing: {token}"
    assert "settings_opt" not in main, "old fixed-pixel panel helper still present"
    cmake = read(CMAKE_LISTS)
    assert "add_library(imgui" in cmake, "imgui target missing"
    assert "imgui_impl_sdl2" in cmake and "imgui_impl_opengl2" in cmake, \
        "imgui backends not built"


def test_updatecheck_units():
    # Compiles the SHIPPED update logic header and drives version compare
    # plus newest-release selection on representative payloads.
    _compile_and_run(os.path.join(REPO, "tests", "test_updatecheck.cpp"),
                     [],
                     "ALL UPDATE TESTS PASSED")


def test_updatecheck_wired_into_app():
    # The check must actually run at startup and surface exactly once,
    # and a Yes must flow into download -> install -> restart.
    main = read(SRC_MAIN)
    assert "AppUpdate_StartCheck();" in main, "update check never started"
    assert "AppUpdate_Poll(" in main, "update result never polled"
    assert "Update available:" in main, "no user-visible update prompt"
    assert "Download and install now?" in main, "prompt must offer the full flow"
    assert "AppUpdate_BeginDownload();" in main, "Yes never starts the download"
    assert "AppUpdate_DownloadActive(" in main, "no progress overlay"
    assert "AppUpdate_CancelDownload();" in main, "no way to cancel the download"
    assert "AppUpdate_ConsumeReady(" in main, "ready/failed result never consumed"
    assert "sRestartFrames" in main, "no restart countdown after install"
    assert "ATANUAVERSION" in main, "prompt must show built-in version"
    internal = read(os.path.join(REPO, "src", "include", "atanua_internal.h"))
    for token in ["AppUpdate_StartCheck", "AppUpdate_Poll", "AppUpdate_BeginDownload",
                  "AppUpdate_DownloadActive", "AppUpdate_CancelDownload",
                  "AppUpdate_ConsumeReady"]:
        assert token in internal, f"missing updater decl: {token}"
    core = os.path.join(REPO, "src", "core", "appupdate.cpp")
    assert os.path.isfile(core), "appupdate.cpp missing"
    src = read(core)
    assert "api.github.com" in src, "updater must query the releases API"
    assert "SDL_CreateThread" in src, "fetch must stay off the UI thread"
    assert "SDL_DetachThread" in src, "worker thread must detach"
    assert "tasklist" in src and "xcopy" in src, "windows restarter script missing"
    assert "Restarting to finish the update." in src, "no restart notice text"
    assert "tar -xf" in src or "tar.exe" in src, "windows unpack step missing"
    assert "kill -0" in src and "cp -rf" in src, "linux restarter script missing"
    cmake = read(CMAKE_LISTS)
    assert "appupdate.cpp" in cmake, "appupdate.cpp not built"
    assert "wininet" in cmake.lower(), "Windows HTTP link missing"


def test_anchor_visible_and_magnetic():
    main = read(SRC_MAIN)
    assert "anchorGrabPad" in main, "magnetic anchor hit-test missing"
    assert "mRotatedW < 2.0f" in main, "tiny-chip-only padding missing"
    pin = read(SRC_EXTRAPIN)
    assert "anchor dot" in pin, "always-on anchor marker missing"
    assert "UI_THEME_ACCENTTEXT" in pin, "anchor hover accent missing"
    assert "ANCHOR_WIRE_GREEN" in pin, "inner wire-start square missing"
    assert "Outer ring" in pin, "outer move ring missing"


def test_circuits_parse_and_wire_indices_valid():
    found = 0
    for path in TEST_CIRCUITS:
        assert os.path.exists(path), f"missing circuit {path}"
        tree = ET.parse(path)
        root = tree.getroot()
        assert root.tag == "Atanua", f"bad root in {path}"
        chips = root.findall("Chip")
        wires = root.findall("Wire")
        assert chips, f"no chips in {path}"
        for w in wires:
            c1 = int(w.get("chip1", "-1"))
            c2 = int(w.get("chip2", "-1"))
            p1 = int(w.get("pad1", "-1"))
            p2 = int(w.get("pad2", "-1"))
            assert 0 <= c1 < len(chips), f"wire chip1 OOB in {path}"
            assert 0 <= c2 < len(chips), f"wire chip2 OOB in {path}"
            assert p1 >= 0 and p2 >= 0, f"negative pad in {path}"
        found += 1
    assert found == len(TEST_CIRCUITS)


def test_binary_and_assets_present():
    assert os.path.exists(BUILD_EXE), "atanua.exe missing; build first"
    assert os.path.getsize(BUILD_EXE) > 100000, "atanua.exe suspiciously small"
    for name in ["vera14.fnt", "vera31.fnt", "icon.png", "led.png"]:
        assert os.path.exists(os.path.join(DATA_DIR, name)), f"missing data/{name}"


def test_cpp_theme_harness():
    import shutil
    import tempfile
    tmp = tempfile.mkdtemp(prefix="atanua_theme_")
    try:
        with open(os.path.join(REPO, "src", "include", "ui_theme.h"), "r", encoding="utf-8") as f:
            hdr_text = f.read()
        with open(os.path.join(REPO, "tests", "test_ui_theme.cpp"), "r", encoding="utf-8") as f:
            src_text = f.read()
        hdr = os.path.join(tmp, "ui_theme.h")
        src = os.path.join(tmp, "test_ui_theme.cpp")
        with open(hdr, "w", encoding="utf-8") as f:
            f.write(hdr_text)
        with open(src, "w", encoding="utf-8") as f:
            f.write(src_text)
        vsdev = r"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
        exe = os.path.join(tmp, "theme_test.exe")
        bat = os.path.join(tmp, "build_theme.bat")
        with open(bat, "w", encoding="ascii") as f:
            f.write(f'call "{vsdev}" -arch=x64 -host_arch=x64 >NUL\n')
            f.write(f'cl /nologo /EHsc /I"{tmp}" "{src}" /Fe"{exe}"\n')
        p = subprocess.run(["cmd", "/c", bat], capture_output=True, timeout=180)
        def dec(b):
            if not b:
                return ""
            for enc in ("utf-8", "cp866", "cp1251", "latin1"):
                try:
                    return b.decode(enc)
                except Exception:
                    continue
            return b.decode("utf-8", errors="replace")
        out, err = dec(p.stdout), dec(p.stderr)
        assert p.returncode == 0, f"theme harness compile failed:\n{out}\n{err}"
        r2 = subprocess.run([exe], capture_output=True, timeout=30)
        out2 = dec(r2.stdout)
        assert r2.returncode == 0, f"theme harness failed:\n{out2}\n{dec(r2.stderr)}"
        assert "ALL UI THEME TESTS PASSED" in out2, f"missing pass marker: {out2}"
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def parse_dotted(s):
    return tuple(int(p) for p in s.strip().split("."))


def test_builtin_version_monotonic():
    # The built-in version must never fall behind any CHANGELOG tag, or
    # releases ship stale versions and the updater nags forever.
    import re
    internal = read(os.path.join(REPO, "src", "include", "atanua_internal.h"))
    m = re.search(r'#define ATANUAVERSION "([^"]+)"', internal)
    assert m, "ATANUAVERSION missing"
    code = parse_dotted(m.group(1))
    changelog = read(os.path.join(REPO, "CHANGELOG.md"))
    tags = re.findall(r"^## \[(v[\d.]+)\]", changelog, re.M)
    assert tags, "no tag sections in changelog"
    for tag in tags:
        ver = parse_dotted(tag.lstrip("vV"))
        assert code >= ver, f"built-in {m.group(1)} predates changelog tag {tag}"


if __name__ == "__main__":
    test_modern_theme_is_default()
    test_toolkit_and_font_guards()
    test_fileutils_bounds()
    test_sim_crash_guards()
    test_fileio_roundtrip_guards()
    test_main_interaction_guards()
    test_wire_bend_helpers()
    test_fileutils_roundtrip()
    test_updatecheck_units()
    test_updatecheck_wired_into_app()
    test_undo_covers_every_mutation()
    test_ubuntu_ci_has_gtk()
    test_workflow_publishes_tagged_releases()
    test_readme_and_changelog()
    test_builtin_version_monotonic()
    test_reset_saves_only_on_confirm()
    test_anchor_visible_and_magnetic()
    test_circuits_parse_and_wire_indices_valid()
    test_binary_and_assets_present()
    test_dropfile_units()
    test_dropfile_wired_into_app()
    test_fileassoc_units()
    test_fileassoc_wired_into_app()
    test_settings_layout_no_overlap()
    test_cpp_theme_harness()
    print("python structural checks passed")
