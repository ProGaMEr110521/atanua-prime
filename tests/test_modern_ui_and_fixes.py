"""Modern UI + bug-fix regression tests. Drives shipped sources and binary."""
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_MAIN = os.path.join(REPO, "src", "core", "main.cpp")
SRC_SIM = os.path.join(REPO, "src", "core", "simutils.cpp")
SRC_FILEIO = os.path.join(REPO, "src", "core", "fileio.cpp")
SRC_FILEUTILS = os.path.join(REPO, "src", "core", "fileutils.cpp")
SRC_TOOLKIT = os.path.join(REPO, "src", "basecode", "toolkit.cpp")
SRC_FONT = os.path.join(REPO, "src", "basecode", "angelcodefont.cpp")
THEME_H = os.path.join(REPO, "src", "include", "ui_theme.h")
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
    assert "UndoDepthForCurrentDesign" in sim, "adaptive undo missing"
    assert "try" in sim and "clear_stack(gRedoStack)" in sim
    assert "while (i < gChip.size())" in sim, "optimize_box safe loop missing"


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
    ]:
        assert needle in main, f"main guard missing: {needle}"


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
    # Compiles and runs the real shipped header. Proves helpers work.
    # Copies to an ASCII-only temp dir: repo path contains Cyrillic which
    # MSVC's batch setup mangles, so building in place fails spuriously.
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
    test_circuits_parse_and_wire_indices_valid()
    test_binary_and_assets_present()
    test_cpp_theme_harness()
    print("python structural checks passed")
