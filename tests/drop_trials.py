"""Scripted drop/argv trials against the real Atanua exe (grok-style).

Proves the plan.md drop/argv work without a human mouse:
  D1: launch exe with a .atanua path (argv) -> canvas loads the fixture.
  D2: launch exe with a .txt path (argv) -> ignored, canvas stays empty.
  D3: launch exe clean, synthesize a real WM_DROPFILES onto the Atanua
      window with a .atanua path -> canvas loads the fixture.
  D4: same drop with a .txt path -> ignored, canvas stays empty.
  D5: seeded HKCU association check (assocState logic lives in the
      app; here we only verify the shipped ProgID/icon/desktop files).

Technique mirrors grok's wire_trials.py but never steals focus (the
user may be working at this PC while trials run): the app window is
raised TOPMOST without activating, so the real cursor can click the
Save button while the user's window keeps keyboard focus; drops are
real WM_DROPFILES via PostMessage; the native Save dialog is driven
through its own HWNDs (WM_SETTEXT + BM_CLICK); canvas state is read
from saved .atanua XML via the real Save path; screenshots use
PrintWindow on the app HWND so occlusion never matters. The run parks
the mouse cursor for ~1 minute and covers the screen with the app
window; leave the mouse alone while it runs. DPI must be 96 (100% scaling).

Usage: python tests/drop_trials.py [--keep-open]
Screenshots + results land in tests/drop_trials/.
"""
import ctypes
import os
import re
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32
shell32 = ctypes.windll.shell32

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(REPO, "build", "Release", "atanua.exe")
EXE_DIR = os.path.dirname(EXE)
TRIAL_DIR = os.path.join(REPO, "tests", "drop_trials")
CONFIG = os.path.join(EXE_DIR, "atanua.xml")
CONFIG_BAK = os.path.join(TRIAL_DIR, "atanua.xml.bak")

WM_DROPFILES = 0x0233

DROP_FIXTURE = """<?xml version="1.0" ?>
<Atanua Version="harness" ChipCount="2" WireCount="1" key="0" scale="16">
    <Chip Name="button ('a')" xpos="131072" ypos="327680" rot="0" key="0" />
    <Chip Name="LED (red)" xpos="1310720" ypos="327680" rot="0" key="0" />
    <Wire chip1="0" pad1="0" chip2="1" pad2="0" key="0" />
</Atanua>
"""


def find_window():
    out = []
    CMP = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
    buf = ctypes.create_unicode_buffer(512)

    def cb(h, _):
        user32.GetWindowTextW(h, buf, 512)
        if "Atanua/Win32" in (buf.value or "") and user32.IsWindowVisible(h):
            out.append(h)
        return True

    user32.EnumWindows(CMP(cb), 0)
    return out[0] if out else None


def ensure_settled(hwnd):
    # Focus-free readiness: never foregrounds, activates, or clicks.
    # The window may sit behind the user's work; all driving below is
    # via window messages, and screenshots via PrintWindow.
    return user32.IsWindowVisible(hwnd)


def client_size(hwnd):
    from ctypes.wintypes import RECT
    rc = RECT()
    user32.GetClientRect(hwnd, ctypes.byref(rc))
    return rc.right - rc.left, rc.bottom - rc.top


def force_client_size(hwnd, want_w, want_h):
    from ctypes.wintypes import RECT
    for _ in range(4):
        cw, ch = client_size(hwnd)
        if (cw, ch) == (want_w, want_h):
            return True
        wr = RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(wr))
        ww, wh = wr.right - wr.left, wr.bottom - wr.top
        user32.SetWindowPos(hwnd, 0, 0, 0, ww + (want_w - cw), wh + (want_h - ch),
                            0x0002 | 0x0010)  # SWP_NOMOVE | SWP_NOACTIVATE
        time.sleep(0.5)
    return client_size(hwnd) == (want_w, want_h)


def printwindow(hwnd, path):
    """Screenshot the window itself (works occluded) via PrintWindow."""
    from ctypes.wintypes import RECT
    gdi32 = ctypes.windll.gdi32
    user32.GetDC.restype = ctypes.c_void_p
    user32.GetDC.argtypes = [ctypes.c_void_p]
    user32.ReleaseDC.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    gdi32.CreateCompatibleDC.restype = ctypes.c_void_p
    gdi32.CreateCompatibleDC.argtypes = [ctypes.c_void_p]
    gdi32.CreateCompatibleBitmap.restype = ctypes.c_void_p
    gdi32.CreateCompatibleBitmap.argtypes = [ctypes.c_void_p,
                                             ctypes.c_int, ctypes.c_int]
    gdi32.SelectObject.restype = ctypes.c_void_p
    gdi32.SelectObject.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    gdi32.GetDIBits.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                ctypes.c_uint, ctypes.c_uint,
                                ctypes.c_void_p, ctypes.c_void_p,
                                ctypes.c_uint]
    gdi32.DeleteObject.argtypes = [ctypes.c_void_p]
    gdi32.DeleteDC.argtypes = [ctypes.c_void_p]
    user32.PrintWindow.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                   ctypes.c_uint]
    wr = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(wr))
    w, h = wr.right - wr.left, wr.bottom - wr.top
    assert w > 0 and h > 0, "zero window rect"
    hdc_screen = user32.GetDC(0)
    try:
        hdc_mem = gdi32.CreateCompatibleDC(hdc_screen)
        assert hdc_mem, "CreateCompatibleDC failed"
        try:
            hbmp = gdi32.CreateCompatibleBitmap(hdc_screen, w, h)
            assert hbmp, "CreateCompatibleBitmap failed"
            try:
                gdi32.SelectObject(hdc_mem, hbmp)
                assert user32.PrintWindow(hwnd, hdc_mem, 2), \
                    "PrintWindow failed"
                import struct as _st
                bmi = _st.pack("<IiiHHIIiiII", 40, w, -h, 1, 32, 0,
                               0, 0, 0, 0, 0)
                buf = ctypes.create_string_buffer(w * h * 4)
                assert gdi32.GetDIBits(hdc_mem, hbmp, 0, h, buf, bmi,
                                       0) == h, "GetDIBits failed"
                from PIL import Image
                Image.frombuffer("RGBA", (w, h), buf, "raw", "BGRA",
                                 0, 1).convert("RGB").save(path)
            finally:
                gdi32.DeleteObject(hbmp)
        finally:
            gdi32.DeleteDC(hdc_mem)
    finally:
        user32.ReleaseDC(0, hdc_screen)


def launch(args, tag, wait_hwnd=True, cwd=None):
    """Start the exe with argv; wait for its window; size it; screenshot."""
    proc = subprocess.Popen([EXE] + args, cwd=cwd or EXE_DIR)
    hwnd = None
    if wait_hwnd:
        for _ in range(40):
            time.sleep(0.25)
            if proc.poll() is not None:
                raise RuntimeError("app exited early")
            hwnd = find_window()
            if hwnd:
                break
        assert hwnd, "no Atanua window"
        assert ensure_settled(hwnd), "app window lost"
        assert force_client_size(hwnd, 1280, 800), \
            "client not 1280x800: %s" % (client_size(hwnd),)
        # Visible but never focused: TOPMOST without activate, so real
        # clicks land on the app while the user's window keeps focus.
        assert retopmost(hwnd), "could not raise app window"
        time.sleep(1.0)  # let the canvas settle after argv load
        printwindow(hwnd, os.path.join(TRIAL_DIR, "dbg_%s.png" % tag))
    return proc, hwnd


WM_SETTEXT = 0x000C
BM_CLICK = 0x00F5
# Save-button center in client coords for the pinned EN 1280x800
# single-row topbar (swept 2026-09-22: x=510 still hits Box, x=525
# opens Save). Any topbar regrouping breaks this loudly (no dialog)
# instead of silently passing.
SAVE_CX, SAVE_CY = 525, 19


class _MOUSEINPUT(ctypes.Structure):
    _fields_ = [("dx", ctypes.c_long), ("dy", ctypes.c_long),
                ("mouseData", ctypes.c_ulong), ("dwFlags", ctypes.c_ulong),
                ("time", ctypes.c_ulong),
                ("dwExtraInfo", ctypes.c_void_p)]


class _INPUT(ctypes.Structure):
    _fields_ = [("type", ctypes.c_ulong), ("mi", _MOUSEINPUT)]


def real_click(hwnd, cx, cy):
    # Real cursor input: posted mouse messages lose the ImGui hover race
    # against the backend's global-cursor fallback, so park the real
    # cursor on the button and SendInput the click. Moves the cursor but
    # never touches focus: the trials TOPMOST the app window (no
    # activate), so clicks land while the user's window keeps focus.
    # The cursor is re-verified right before the click: if anything
    # (e.g. the user) moved it, park again instead of clicking blind.
    from ctypes.wintypes import POINT
    pt = POINT(cx, cy)
    assert user32.ClientToScreen(hwnd, ctypes.byref(pt)), \
        "ClientToScreen failed"
    user32.GetCursorPos.argtypes = [ctypes.POINTER(POINT)]
    # Park first and let the move event pump through (ImGui hover must
    # settle before the press, or a same-quantum down+up is ignored).
    for _ in range(10):
        assert user32.SetCursorPos(pt.x, pt.y), "SetCursorPos failed"
        time.sleep(0.3)
        cur = POINT(0, 0)
        user32.GetCursorPos(ctypes.byref(cur))
        if (cur.x, cur.y) == (pt.x, pt.y):
            break
        time.sleep(0.2)
    else:
        raise RuntimeError("cursor keeps moving away; user at mouse?")
    arr = (_INPUT * 2)()
    arr[0].type = 0
    arr[0].mi.dwFlags = 0x0002  # LEFTDOWN
    arr[1].type = 0
    arr[1].mi.dwFlags = 0x0004  # LEFTUP
    assert user32.SendInput(2, arr, ctypes.sizeof(_INPUT)) == 2, \
        "SendInput failed"
    time.sleep(0.4)


def retopmost(hwnd):
    user32.SetWindowPos.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                    ctypes.c_int, ctypes.c_int,
                                    ctypes.c_int, ctypes.c_int,
                                    ctypes.c_uint]
    return user32.SetWindowPos(hwnd, -1, 0, 0, 0, 0,
                               0x0001 | 0x0002 | 0x0010)


def find_dialog():
    out = []
    CMP = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
    buf = ctypes.create_unicode_buffer(512)

    def cb(h, _):
        user32.GetWindowTextW(h, buf, 512)
        if "Save Atanua" in (buf.value or "") and user32.IsWindowVisible(h):
            out.append(h)
        return True

    user32.EnumWindows(CMP(cb), 0)
    return out[0] if out else None


def child_windows(hwnd):
    out = []
    CMP = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

    def cb(h, _):
        out.append(h)
        return True

    user32.EnumChildWindows.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                        ctypes.c_void_p]
    user32.EnumChildWindows(hwnd, CMP(cb), 0)
    return out


def class_of(hwnd):
    buf = ctypes.create_unicode_buffer(64)
    user32.GetClassNameW(hwnd, buf, 64)
    return buf.value


def find_filename_edits(dlg):
    # Explorer-style save dialog has TWO ComboBoxEx32 edits (address bar
    # on top, file-name box at the bottom) plus a plain Edit id 1001 in
    # the bottom strip. Set the path into every plausible file-name box
    # and let the read-back below prove which one is authoritative.
    from ctypes.wintypes import RECT
    user32.GetWindowRect.argtypes = [ctypes.c_void_p,
                                     ctypes.POINTER(RECT)]
    boxes = []

    def rect_top(h):
        wr = RECT()
        user32.GetWindowRect(h, ctypes.byref(wr))
        return wr.top

    combos = [c for c in child_windows(dlg) if class_of(c) == "ComboBoxEx32"]
    combos.sort(key=rect_top)  # address bar first, file-name box last
    for c in combos:
        for g in child_windows(c):
            if class_of(g) == "Edit":
                boxes.append(g)
                break
    user32.GetDlgCtrlID.argtypes = [ctypes.c_void_p]
    user32.GetDlgCtrlID.restype = ctypes.c_int
    for c in child_windows(dlg):
        if class_of(c) == "Edit" and user32.GetDlgCtrlID(c) == 1001:
            boxes.append(c)
    seen, unique = set(), []
    for h in boxes:
        if h not in seen:
            seen.add(h)
            unique.append(h)
    return unique


def save_canvas(proc, hwnd, out_path, tag):
    """Save via posted Save-button click + dialog automation; (c, w)."""
    if os.path.exists(out_path):
        os.remove(out_path)
    user32.SendMessageW.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                    ctypes.c_void_p, ctypes.c_wchar_p]
    user32.GetDlgItem.restype = ctypes.c_void_p
    user32.GetDlgItem.argtypes = [ctypes.c_void_p, ctypes.c_int]
    real_click(hwnd, SAVE_CX, SAVE_CY)
    dlg = None
    for _ in range(3):
        for _ in range(10):
            time.sleep(0.25)
            dlg = find_dialog()
            if dlg:
                break
        if dlg:
            break
        print("  save(%s): retrying Save click" % tag)
        retopmost(hwnd)
        real_click(hwnd, SAVE_CX, SAVE_CY)
    if not dlg:
        print("  save(%s): no Save dialog appeared" % tag)
        return None
    printwindow(dlg, os.path.join(TRIAL_DIR, "dbg_%s_save_dlg.png" % tag))
    edits = find_filename_edits(dlg)
    if not edits:
        print("  save(%s): dialog %s has no filename box" % (tag, dlg))
        return None
    for edit in edits:
        assert user32.SendMessageW(edit, WM_SETTEXT, 0, out_path), \
            "could not set save path"
    time.sleep(0.3)
    # Read back what the boxes actually hold (wrong-box diagnosis).
    user32.SendMessageW.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                    ctypes.c_void_p, ctypes.c_void_p]
    matched = False
    for edit in edits:
        back = ctypes.create_unicode_buffer(1024)
        user32.SendMessageW(edit, 0x000D, 1024,
                            ctypes.cast(back, ctypes.c_void_p))
        hit = back.value[:80] == out_path[:80]
        matched = matched or hit
        print("  save(%s): box %s holds %r%s" % (tag, edit, back.value[:80],
                                                 "" if hit else " (STALE)"))
    if not matched:
        print("  save(%s): path took in no box" % tag)
        return None
    user32.SendMessageW.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                    ctypes.c_void_p, ctypes.c_wchar_p]
    ok = user32.GetDlgItem(dlg, 1)  # IDOK = the Save button
    if not ok:
        print("  save(%s): dialog %s has no Save button" % (tag, dlg))
        return None
    # BM_CLICK always returns zero; success is the file appearing below.
    user32.SendMessageW(ok, BM_CLICK, 0, None)
    for _ in range(20):
        time.sleep(0.25)
        if os.path.exists(out_path):
            break
    if not os.path.exists(out_path):
        print("  save(%s): file never appeared" % tag)
        return None
    tree = ET.parse(out_path)
    root = tree.getroot()
    return len(root.findall("Chip")), len(root.findall("Wire"))


def drop_files(hwnd, paths):
    """Synthesize a real WM_DROPFILES with a DROPFILES struct (like Explorer)."""
    blob = b"".join(p.encode("utf-16-le") + b"\x00\x00" for p in paths) + b"\x00\x00"
    GMEM = 0x0002 | 0x0020  # MOVEABLE | ZEROINIT
    kernel32.GlobalAlloc.restype = ctypes.c_void_p
    kernel32.GlobalAlloc.argtypes = [ctypes.c_uint, ctypes.c_size_t]
    kernel32.GlobalLock.restype = ctypes.c_void_p
    kernel32.GlobalLock.argtypes = [ctypes.c_void_p]
    kernel32.GlobalUnlock.argtypes = [ctypes.c_void_p]
    h = kernel32.GlobalAlloc(GMEM, 20 + len(blob))
    assert h, "drop alloc failed"
    p = kernel32.GlobalLock(h)
    assert p, "drop lock failed"
    import struct as _st
    # DROPFILES is 5 fields / 20 bytes: pFiles, pt.x, pt.y, fNC, fWide.
    ctypes.memmove(p, _st.pack("<IiiII", 20, 0, 0, 0, 1), 20)
    ctypes.memmove(p + 20, blob, len(blob))
    kernel32.GlobalUnlock(h)
    user32.PostMessageW.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                    ctypes.c_void_p, ctypes.c_void_p]
    assert user32.PostMessageW(hwnd, WM_DROPFILES, h, 0), \
        "PostMessage drop failed"
    # The app owns (and frees) the handle after DragFinish; give it time.
    time.sleep(2.0)
    return True


def stop_proc(proc):
    for _ in range(3):
        if proc.poll() is not None:
            return True
        proc.terminate()
        try:
            proc.wait(timeout=4)
        except Exception:
            pass
    try:
        proc.kill()
    except Exception:
        pass
    return proc.poll() is not None


def main():
    keep_open = "--keep-open" in sys.argv
    os.makedirs(TRIAL_DIR, exist_ok=True)
    user32.GetDC.restype = ctypes.c_void_p
    user32.GetDC.argtypes = [ctypes.c_void_p]
    user32.ReleaseDC.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    ctypes.windll.gdi32.GetDeviceCaps.restype = ctypes.c_int
    ctypes.windll.gdi32.GetDeviceCaps.argtypes = [ctypes.c_void_p,
                                                  ctypes.c_int]
    hdc = user32.GetDC(0)
    dpi = ctypes.windll.gdi32.GetDeviceCaps(hdc, 88)
    user32.ReleaseDC(0, hdc)
    print("system dpi:", dpi)
    assert dpi == 96, "non-100%% scaling (%d) unsupported" % dpi
    subprocess.run(["taskkill", "/F", "/IM", "atanua.exe"], capture_output=True)
    time.sleep(1.5)
    assert not find_window(), "stale Atanua window survived taskkill"
    shutil.copyfile(CONFIG, CONFIG_BAK)
    with open(CONFIG) as f:
        cfg = f.read()
    cfg = cfg.replace('Enable="1"', 'Enable="0"')
    # Pin EN for the run: the Save-button client coords below are only
    # valid for one topbar geometry, and EN labels are the stable dev
    # strings. RU coverage lives in the settings unit tests (and the RU
    # screenshots already captured). User config is restored in finally.
    cfg = re.sub(r'<Language value="[^"]*"/>', '<Language value="0"/>', cfg)
    with open(CONFIG, "w") as f:
        f.write(cfg)

    fix_atanua = os.path.join(TRIAL_DIR, "drop_src.atanua")
    with open(fix_atanua, "w") as f:
        f.write(DROP_FIXTURE)
    fix_txt = os.path.join(TRIAL_DIR, "drop_src.txt")
    with open(fix_txt, "w") as f:
        f.write("not a circuit")

    results = []
    try:
        # D1: argv with .atanua -> loads 2 chips + 1 wire.
        proc, hwnd = launch([fix_atanua], "d1_argv_atanua")
        got = save_canvas(proc, hwnd, os.path.join(TRIAL_DIR, "result_d1.atanua"), "d1")
        ok = got == (2, 1)
        print("D1 argv .atanua: canvas=%s expect=(2, 1) -> %s" % (got, "PASS" if ok else "FAIL"))
        results.append(ok)
        assert stop_proc(proc), "app refused to die"

        # D2: argv with .txt -> ignored, canvas empty.
        proc, hwnd = launch([fix_txt], "d2_argv_txt")
        got = save_canvas(proc, hwnd, os.path.join(TRIAL_DIR, "result_d2.atanua"), "d2")
        ok = got == (0, 0)
        print("D2 argv .txt: canvas=%s expect=(0, 0) -> %s" % (got, "PASS" if ok else "FAIL"))
        results.append(ok)
        assert stop_proc(proc), "app refused to die"

        # D3: real WM_DROPFILES with .atanua onto clean canvas -> loads.
        proc, hwnd = launch([], "d3_drop_atanua")
        drop_files(hwnd, [fix_atanua])
        printwindow(hwnd, os.path.join(TRIAL_DIR, "dbg_d3_after_drop.png"))
        got = save_canvas(proc, hwnd, os.path.join(TRIAL_DIR, "result_d3.atanua"), "d3")
        ok = got == (2, 1)
        print("D3 drop .atanua: canvas=%s expect=(2, 1) -> %s" % (got, "PASS" if ok else "FAIL"))
        results.append(ok)
        assert stop_proc(proc), "app refused to die"

        # D4: real WM_DROPFILES with .txt -> ignored.
        proc, hwnd = launch([], "d4_drop_txt")
        drop_files(hwnd, [fix_txt])
        printwindow(hwnd, os.path.join(TRIAL_DIR, "dbg_d4_after_drop.png"))
        got = save_canvas(proc, hwnd, os.path.join(TRIAL_DIR, "result_d4.atanua"), "d4")
        ok = got == (0, 0)
        print("D4 drop .txt: canvas=%s expect=(0, 0) -> %s" % (got, "PASS" if ok else "FAIL"))
        results.append(ok)
        assert stop_proc(proc), "app refused to die"

        # D6: relative argv path from another cwd -> still loads. The app
        # chdirs to its exe dir at startup, so a relative argv[1] must be
        # resolved against the startup directory first.
        rel_arg = os.path.relpath(fix_atanua, REPO)
        assert not os.path.isabs(rel_arg), "expected a relative trial path"
        proc, hwnd = launch([rel_arg], "d6_argv_relative", cwd=REPO)
        got = save_canvas(proc, hwnd, os.path.join(TRIAL_DIR, "result_d6.atanua"), "d6")
        ok = got == (2, 1)
        print("D6 argv relative: canvas=%s expect=(2, 1) -> %s" % (got, "PASS" if ok else "FAIL"))
        results.append(ok)
        if not keep_open:
            assert stop_proc(proc), "app refused to die"
        else:
            print("keeping last app window open per --keep-open")

        # D5: shipped association/support artifacts (no registry writes here).
        checks = {
            "atanua.ico ships": os.path.isfile(os.path.join(REPO, "atanua.ico")),
            ".desktop ships": os.path.isfile(os.path.join(REPO, "atanua.desktop")),
        }
        desk = os.path.join(REPO, "atanua.desktop")
        if os.path.isfile(desk):
            with open(desk) as f:
                d = f.read()
            checks[".desktop MimeType"] = "MimeType=application/x-atanua;" in d
            checks[".desktop %f"] = "%f" in d
        for name, passed in checks.items():
            print("D5 %s -> %s" % (name, "PASS" if passed else "FAIL"))
            results.append(passed)
    finally:
        shutil.copyfile(CONFIG_BAK, CONFIG)
    print("ALL DROP TRIALS PASS" if all(results) else "SOME DROP TRIALS FAILED")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
