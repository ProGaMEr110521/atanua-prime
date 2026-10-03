#!/usr/bin/env python3
"""Route diagonal Atanua wires through Connection Pin bends.

Turns each long diagonal chip-to-chip wire into a Manhattan-style
Z-bend with a perfectly axis-aligned trunk plus two short stubs:

    A -> a1 -> a2 -> B

where a1->a2 shares one X (vertical trunk) or one Y (horizontal trunk).
Stubs stay short, so the scheme reads as 90-degree wiring instead of a
star of diagonals. No simulation or loader changes: bends are ordinary
Connection Pin chips plus ordinary Wire segments, the same construction
the app itself creates for click-to-bend and routing anchors.

Usage:
    python tools/route_wires.py in.atanua out.atanua

Rules:
- Chips listed first, wires after (matches the app saver).
- Wires already touching a Connection Pin are left alone.
- A direct wire is routed only when both chip-origin deltas exceed 1.0
  canvas unit; short near-orthogonal wires stay direct.
- Non-anchor chips snap to an 8-unit grid; anchors snap to 0.5.
- Parallel trunks get lane offsets (0, +2, -2, +4, -4, ...) so they
  never overlap. Every routed net gets fresh anchors: anchors are never
  shared across nets (sharing would short two nets together).

Canvas units: X = xpos / 1048576. Anchor pad sits at its chip center,
so an anchor chip at (cx - 0.5, cy - 0.5) puts its pad at (cx, cy).
"""
import sys
import xml.etree.ElementTree as ET

SCALE = 1048576
GRID = 8.0
DIAG_TOL = 1.0
LANE_OFFSETS = [0, 2, -2, 4, -4, 6, -6, 8, -8, 10, -10, 12, -12]


def snap(v, step):
    return round(v / step) * step


def parse_circuit(path):
    tree = ET.parse(path)
    root = tree.getroot()
    if root.tag != "Atanua":
        raise SystemExit("ERROR: missing <Atanua> root element")
    chips = []
    for el in root.findall("Chip"):
        name = el.get("Name")
        if name is None:
            raise SystemExit("ERROR: Chip without Name")
        try:
            x = int(el.get("xpos", "0")) / SCALE
            y = int(el.get("ypos", "0")) / SCALE
        except ValueError:
            raise SystemExit("ERROR: Chip without integer xpos/ypos")
        chips.append({"name": name, "x": x, "y": y,
                      "rot": el.get("rot", "0")})
    wires = []
    for el in root.findall("Wire"):
        try:
            c1 = int(el.get("chip1"))
            c2 = int(el.get("chip2"))
            p1 = int(el.get("pad1"))
            p2 = int(el.get("pad2"))
        except (TypeError, ValueError):
            raise SystemExit("ERROR: Wire without integer chip/pad")
        wires.append((c1, c2, p1, p2))
    return chips, wires


def is_anchor(chips, idx):
    return chips[idx]["name"] == "Connection Pin"


def main():
    if len(sys.argv) != 3:
        print("usage: route_wires.py in.atanua out.atanua")
        return 2
    src, dst = sys.argv[1], sys.argv[2]
    chips, wires = parse_circuit(src)

    # Snap placed chips to the grid; leave existing anchors on the 0.5 grid.
    for ch in chips:
        if ch["name"] == "Connection Pin":
            ch["x"] = snap(ch["x"], 0.5)
            ch["y"] = snap(ch["y"], 0.5)
        else:
            ch["x"] = snap(ch["x"], GRID)
            ch["y"] = snap(ch["y"], GRID)

    occupied = set()  # anchor-pad coordinates already taken by another net
    for i, ch in enumerate(chips):
        if ch["name"] == "Connection Pin":
            pad = (round((ch["x"] + 0.5) * 2) / 2,
                   round((ch["y"] + 0.5) * 2) / 2)
            occupied.add(pad)

    def body_at(px, py):
        # Conservative body test without mined sizes: typical packages fit
        # in 7 x 4 from their origin, so keep pads clearly outside that.
        for ch in chips:
            if ch["name"] == "Connection Pin":
                continue
            if ch["x"] - 1 <= px <= ch["x"] + 7 and \
               ch["y"] - 1 <= py <= ch["y"] + 4:
                return True
        return False

    def new_anchor(px, py):
        key = (round(px * 2) / 2, round(py * 2) / 2)
        # Anchor chip origin puts its pad (center) exactly on (px, py).
        chips.append({"name": "Connection Pin", "x": key[0] - 0.5,
                      "y": key[1] - 0.5, "rot": "0"})
        occupied.add(key)
        return len(chips) - 1

    lane_no = 0
    out_wires = []
    routed = 0
    for (c1, c2, p1, p2) in wires:
        if c1 == c2 or is_anchor(chips, c1) or is_anchor(chips, c2):
            out_wires.append((c1, c2, p1, p2))
            continue
        ax, ay = chips[c1]["x"], chips[c1]["y"]
        bx, by = chips[c2]["x"], chips[c2]["y"]
        dx, dy = bx - ax, by - ay
        if abs(dx) <= DIAG_TOL or abs(dy) <= DIAG_TOL:
            out_wires.append((c1, c2, p1, p2))
            continue
        # Try lane offsets in rotating order so parallel trunks never
        # overlap and never reuse another net's anchor (which would short
        # two nets together). First free lane wins.
        placed = None
        for k in range(len(LANE_OFFSETS)):
            off = LANE_OFFSETS[(lane_no + k) % len(LANE_OFFSETS)]
            if abs(dx) >= abs(dy):
                lane_x = snap((ax + bx) / 2 + off, 0.5)
                q1 = (lane_x, snap(ay + 1, 0.5))
                q2 = (lane_x, snap(by + 1, 0.5))
            else:
                lane_y = snap((ay + by) / 2 + off, 0.5)
                q1 = (snap(ax + 1, 0.5), lane_y)
                q2 = (snap(bx + 1, 0.5), lane_y)
            if q1 not in occupied and q2 not in occupied \
                    and not body_at(*q1) and not body_at(*q2):
                placed = (q1, q2)
                lane_no += 1
                break
        if placed is None:
            # Dense area: fall back to the first lane choice with fresh
            # anchors (never reuse across nets).
            off = LANE_OFFSETS[lane_no % len(LANE_OFFSETS)]
            lane_no += 1
            if abs(dx) >= abs(dy):
                lane_x = snap((ax + bx) / 2 + off, 0.5)
                placed = ((lane_x, snap(ay + 1, 0.5)),
                          (lane_x, snap(by + 1, 0.5)))
            else:
                lane_y = snap((ay + by) / 2 + off, 0.5)
                placed = ((snap(ax + 1, 0.5), lane_y),
                          (snap(bx + 1, 0.5), lane_y))
        (q1, q2) = placed
        a1 = new_anchor(*q1)
        a2 = new_anchor(*q2)
        out_wires.append((c1, a1, p1, 0))
        out_wires.append((a1, a2, 0, 0))
        out_wires.append((a2, c2, 0, p2))
        routed += 1

    root = ET.Element("Atanua")
    for ch in chips:
        el = ET.SubElement(root, "Chip")
        el.set("Name", ch["name"])
        el.set("xpos", str(int(round(ch["x"] * SCALE))))
        el.set("ypos", str(int(round(ch["y"] * SCALE))))
        el.set("rot", str(ch["rot"]))
    for (c1, c2, p1, p2) in out_wires:
        el = ET.SubElement(root, "Wire")
        el.set("chip1", str(c1))
        el.set("chip2", str(c2))
        el.set("pad1", str(p1))
        el.set("pad2", str(p2))
    tree = ET.ElementTree(root)
    ET.indent(tree, space="  ")
    tree.write(dst, encoding="unicode", xml_declaration=False)
    print("routed %d wire(s): %d chips, %d wires -> %s"
          % (routed, len(chips), len(out_wires), dst))
    return 0


if __name__ == "__main__":
    sys.exit(main())
