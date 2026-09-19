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
    for token in ["0xff20242c", "UI_TOPBAR_H", "UI_TAB_W", "UI_BTN_W", "UI_ROW_H",
                  "C_MENULINE", "C_TEXT", "C_HOTROW", "C_ACCENTTEXT"]:
        assert token in main, f"modern theme token missing: {token}"
    assert "0xff3f4f4f" not in main, "old 2008 menubg still active"
    assert "UI_THEME_MENUBG" in theme and "UiTheme" in theme
    assert "UiTheme::clampSliderMax" in main or "clampSliderMax" in main
    assert "Status bar" in main or "Chips:%d" in main
    for token in ["topbarLayout", "TopbarLayout", "topbarHeight", "compactLabels"]:
        assert token in theme, f"responsive topbar helper missing: {token}"


def test_toolkit_and_font_guards():
    toolkit = read(SRC_TOOLKIT)
    assert "if (!aString)" in toolkit, "mystrdup null guard missing"
    assert "if (!buffer || maxlen <= 0)" in toolkit, "textfield guard missing"
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
        "UiTheme::chipListIndex",
        "if (!w || !w->mFirst",
        "GET_WIRE_ID(gUIState.hotitem) : -1",
        "if (kbdWire < 0 || kbdWire >= (int)gWire.size())",
        "SDL_GetWindowSize",
        "if (want)",
        "cursor_normal",
        "font assets missing",
        "topbarLayout",
        "gTopbarH",
        "compactLabels",
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
    assert os.path.isfile(os.path.join(REPO, "data", "vera14.fnt")), "data assets missing from checkout"
    assert os.path.isfile(os.path.join(REPO, "data", "vera31.fnt")), "data assets missing from checkout"


def test_readme_and_changelog():
    # Front page is bilingual and documents the shipped features;
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


def test_updatecheck_units():
    # Compiles the SHIPPED update logic header and drives version compare
    # plus newest-release selection on representative payloads.
    _compile_and_run(os.path.join(REPO, "tests", "test_updatecheck.cpp"),
                     [],
                     "ALL UPDATE TESTS PASSED")


def test_updatecheck_wired_into_app():
    # The check must actually run at startup and surface exactly once.
    main = read(SRC_MAIN)
    assert "AppUpdate_StartCheck();" in main, "update check never started"
    assert "AppUpdate_Poll(" in main, "update result never polled"
    assert "Update available:" in main, "no user-visible update prompt"
    assert "ATANUAVERSION" in main, "prompt must show built-in version"
    internal = read(os.path.join(REPO, "src", "include", "atanua_internal.h"))
    assert "AppUpdate_StartCheck" in internal and "AppUpdate_Poll" in internal
    core = os.path.join(REPO, "src", "core", "appupdate.cpp")
    assert os.path.isfile(core), "appupdate.cpp missing"
    src = read(core)
    assert "api.github.com" in src, "updater must query the releases API"
    assert "SDL_CreateThread" in src, "fetch must stay off the UI thread"
    assert "SDL_DetachThread" in src, "worker thread must detach"
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
    test_reset_saves_only_on_confirm()
    test_anchor_visible_and_magnetic()
    test_circuits_parse_and_wire_indices_valid()
    test_binary_and_assets_present()
    test_cpp_theme_harness()
    print("python structural checks passed")
