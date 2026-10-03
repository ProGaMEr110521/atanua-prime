"""Router regression: diagonal wires become 90-degree anchor bends."""
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROUTER = os.path.join(REPO, "tools", "route_wires.py")
EXAMPLES = os.path.join(REPO, "tools", "examples")
if os.name == "nt":
    BUILD_EXE = os.path.join(REPO, "build", "Release", "atanua.exe")
else:
    BUILD_EXE = os.path.join(REPO, "build", "atanua")

SCALE = 1048576

NAIVE = ("<Atanua>"
         "<Chip Name=\"button ('1')\" xpos=\"226492416\" ypos=\"92274688\" rot=\"0\"/>"
         "<Chip Name=\"logic '1'\" xpos=\"226492416\" ypos=\"134217728\" rot=\"0\"/>"
         "<Chip Name=\"logic AND\" xpos=\"293601280\" ypos=\"117440512\" rot=\"0\"/>"
         "<Chip Name=\"LED (red)\" xpos=\"360710144\" ypos=\"125829120\" rot=\"0\"/>"
         "<Wire chip1=\"0\" pad1=\"0\" chip2=\"2\" pad2=\"0\"/>"
         "<Wire chip1=\"1\" pad1=\"0\" chip2=\"2\" pad2=\"1\"/>"
         "<Wire chip1=\"2\" pad1=\"2\" chip2=\"3\" pad2=\"0\"/></Atanua>")


def load(path):
    root = ET.parse(path).getroot()
    chips = [(c.get("Name"), int(c.get("xpos")) / SCALE,
              int(c.get("ypos")) / SCALE) for c in root.findall("Chip")]
    wires = [(int(w.get("chip1")), int(w.get("chip2")),
              int(w.get("pad1")), int(w.get("pad2")))
             for w in root.findall("Wire")]
    return chips, wires


def anchors_of(chips):
    return {i for i, c in enumerate(chips) if c[0] == "Connection Pin"}


def test_router_routes_diagonals():
    tmp = tempfile.mkdtemp(prefix="atanua_route_")
    try:
        src = os.path.join(tmp, "naive.atanua")
        dst = os.path.join(tmp, "routed.atanua")
        with open(src, "w", encoding="utf-8") as f:
            f.write(NAIVE)
        p = subprocess.run([sys.executable, ROUTER, src, dst],
                           capture_output=True, text=True, timeout=60)
        assert p.returncode == 0, "router failed:\n%s\n%s" % (p.stdout, p.stderr)
        chips, wires = load(dst)
        assert len(chips) == 10, "expected 6 fresh anchors, got %d chips" % len(chips)
        assert len(wires) == 9, "expected 3 wires x 3 segments, got %d" % len(wires)
        anch = anchors_of(chips)
        assert len(anch) == 6, "expected 6 anchors, got %d" % len(anch)
        # No direct diagonal between two non-anchor chips may remain.
        for (c1, c2, _p1, _p2) in wires:
            if c1 in anch or c2 in anch:
                continue
            dx = abs(chips[c1][1] - chips[c2][1])
            dy = abs(chips[c1][2] - chips[c2][2])
            assert dx <= 1.0 or dy <= 1.0, \
                "diagonal direct wire remains: %d -> %d" % (c1, c2)
        # Every anchor-to-anchor trunk must share one X or one Y exactly.
        trunks = [(c1, c2) for (c1, c2, _p1, _p2) in wires
                  if c1 in anch and c2 in anch]
        assert trunks, "no anchor trunk emitted"
        for (c1, c2) in trunks:
            same_x = chips[c1][1] + 0.5 == chips[c2][1] + 0.5
            same_y = chips[c1][2] + 0.5 == chips[c2][2] + 0.5
            assert same_x or same_y, \
                "trunk not axis-aligned: %d -> %d" % (c1, c2)
    finally:
        import shutil
        shutil.rmtree(tmp, ignore_errors=True)


def test_router_leaves_goldens_alone():
    for name in ["and_led_L.atanua", "and_led_Z.atanua",
                 "fanout_shared_anchor.atanua"]:
        src = os.path.join(EXAMPLES, name)
        assert os.path.isfile(src), "missing golden %s" % name
        before = load(src)
        tmp = tempfile.mkdtemp(prefix="atanua_route_gold_")
        try:
            dst = os.path.join(tmp, name)
            p = subprocess.run([sys.executable, ROUTER, src, dst],
                               capture_output=True, text=True, timeout=60)
            assert p.returncode == 0, "router failed on %s" % name
            after = load(dst)
            assert len(after[0]) == len(before[0]), \
                "%s gained anchors; hand-routed goldens must stay stable" % name
        finally:
            import shutil
            shutil.rmtree(tmp, ignore_errors=True)


def test_goldens_validate_and_simulate():
    if not os.path.isfile(BUILD_EXE):
        return
    import json
    for name in ["and_led_L.atanua", "and_led_Z.atanua",
                 "fanout_shared_anchor.atanua"]:
        src = os.path.join(EXAMPLES, name)
        r = subprocess.run([BUILD_EXE, "--validate", src],
                           capture_output=True, text=True, timeout=60)
        assert r.returncode == 0, "%s invalid:\n%s%s" % (name, r.stdout, r.stderr)
    led = os.path.join(EXAMPLES, "and_led_L.atanua")

    def state(args):
        r = subprocess.run([BUILD_EXE, "--simulate", led, "--ticks", "20"] + args,
                           capture_output=True, text=True, timeout=120)
        assert r.returncode == 0, "simulate failed:\n%s%s" % (r.stdout, r.stderr)
        for entry in json.loads(r.stdout)["leds"]:
            if entry["chip"] == 3:
                return entry["state"]
        raise AssertionError("LED chip 3 missing from report")

    assert state(["--set", "0:0=1"]) == "high", "driven LED must light"
    assert state(["--set", "0:0=0"]) == "low", "released LED must go dark"


if __name__ == "__main__":
    test_router_routes_diagonals()
    test_router_leaves_goldens_alone()
    test_goldens_validate_and_simulate()
    print("route-wires checks passed")
