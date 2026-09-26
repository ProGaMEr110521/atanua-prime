#!/usr/bin/env python3
"""Generate Atanua's schematic chip sprites as SVG and render them to PNG.

Every sprite is drawn in world units (1 unit = one pin pitch pair, the same
units the chips use in src/chip/*.cpp), straight from the pin coordinates,
so leads end exactly on the pins. The renderer tints sprites (white on the
dark canvas, black on paper), so everything here is drawn in white on a
transparent background.

    tools/sprites/gen_sprites.py            # writes data/<name>.png
    tools/sprites/gen_sprites.py --svg DIR  # also keeps the SVG sources

Needs rsvg-convert (librsvg) and the Inter font installed for fontconfig.
"""
import math
import os
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DATA = os.path.join(ROOT, "data")

STROKE = 0.09        # line weight, world units
BUBBLE = 0.14        # inversion bubble radius
FONT = "Inter"
INK = "#ffffff"


class Sprite:
    def __init__(self, w, h, ppu, px=None):
        self.w, self.h, self.ppu = w, h, ppu
        # px: explicit (width, height) in pixels for textures the engine
        # stretches onto a non-power-of-two chip size.
        self.px = px or (int(w * ppu), int(h * ppu))
        self.items = []

    def add(self, s):
        self.items.append(s)

    def line(self, x0, y0, x1, y1):
        self.add(f'<line x1="{x0:.4f}" y1="{y0:.4f}" x2="{x1:.4f}" y2="{y1:.4f}"/>')

    def poly(self, pts, closed=False):
        d = " ".join(f"{x:.4f},{y:.4f}" for x, y in pts)
        tag = "polygon" if closed else "polyline"
        self.add(f'<{tag} points="{d}"/>')

    def path(self, d):
        self.add(f'<path d="{d}"/>')

    def circle(self, cx, cy, r):
        self.add(f'<circle cx="{cx:.4f}" cy="{cy:.4f}" r="{r:.4f}"/>')

    def rect(self, x, y, w, h, r=0.0):
        self.add(f'<rect x="{x:.4f}" y="{y:.4f}" width="{w:.4f}" height="{h:.4f}" rx="{r:.4f}"/>')

    def text(self, x, y, s, size=0.62, anchor="middle", weight=600, overline=False):
        # y is the vertical center of the cap height
        base = y + size * 0.36
        self.add(f'<text x="{x:.4f}" y="{base:.4f}" font-size="{size:.4f}" '
                 f'text-anchor="{anchor}" font-weight="{weight}" stroke="none" fill="{INK}">{s}</text>')
        if overline:
            # Approximate advance for short labels; Inter caps ~0.66em.
            wid = 0.64 * size * len(s)
            if anchor == "middle":
                x0 = x - wid / 2
            elif anchor == "end":
                x0 = x - wid
            else:
                x0 = x
            oy = y - size * 0.52
            self.add(f'<line x1="{x0 + 0.02:.4f}" y1="{oy:.4f}" x2="{x0 + wid - 0.02:.4f}" '
                     f'y2="{oy:.4f}" stroke-width="{STROKE * 0.75:.4f}"/>')

    def svg(self):
        body = "\n  ".join(self.items)
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.px[0]}" '
                f'height="{self.px[1]}" viewBox="0 0 {self.w} {self.h}" preserveAspectRatio="none">\n'
                f'<g fill="none" stroke="{INK}" stroke-width="{STROKE}" stroke-linejoin="round" '
                f'stroke-linecap="butt" font-family="{FONT}">\n  {body}\n</g>\n</svg>\n')


def pin_y(py):
    """Pin set() y -> lead center y (pins are 0.5 boxes)."""
    return py + 0.25


# ---------------------------------------------------------------- gates (US)

def us_body(s, kind, top, bot, x0, depth):
    """Draw an ANSI gate body. Returns (back_x(y) function, tip x)."""
    mid = (top + bot) / 2
    half = (bot - top) / 2
    if kind in ("and",):
        flat = x0 + depth - half
        s.path(f"M{x0},{top} H{flat} A{half},{half} 0 0 1 {flat},{bot} H{x0} Z")
        return (lambda y: x0), flat + half
    if kind in ("or", "xor"):
        bulge = 0.35 * (bot - top) / 1.6
        tip = x0 + depth + 0.1
        # back curve, quadratic with control at mid
        s.path(f"M{x0},{top} Q{x0 + bulge * 2},{mid} {x0},{bot} "
               f"Q{x0 + depth * 0.62},{bot} {tip},{mid} "
               f"Q{x0 + depth * 0.62},{top} {x0},{top} Z")

        def back(y, xx=x0):
            t = (y - top) / (bot - top)
            return xx + 2 * t * (1 - t) * bulge * 2 - 0.0
        if kind == "xor":
            gap = 0.2
            s.path(f"M{x0 - gap},{top} Q{x0 - gap + bulge * 2},{mid} {x0 - gap},{bot}")
            return (lambda y: back(y, x0 - gap)), tip
        return back, tip
    if kind == "not":
        tip = x0 + depth
        s.poly([(x0, top), (tip, mid), (x0, bot)], closed=True)
        return (lambda y: x0), tip
    raise ValueError(kind)


def gate_us(kind, inputs, h, out_y, inv):
    s = Sprite(4, h, 128)
    ys = [pin_y(p) for p in inputs]
    if h <= 2:
        top, bot = 0.2, 1.8
        if kind == "not":
            top, bot = 0.4, 1.6
        x0, depth = 1.0, 1.75
    else:
        top, bot = out_y - 1.1, out_y + 1.1
        x0, depth = 1.2, 1.85
    base = kind.replace("n", "", 1) if kind in ("nand", "nor") else kind
    back, tip = us_body(s, base, top, bot, x0, depth)
    if h <= 2:
        for y in ys:
            s.line(0, y, back(y), y)
    else:
        # Fan wide pin spacing into the body back.
        n = len(ys)
        inner = [top + 0.2 + (bot - top - 0.4) * i / (n - 1) for i in range(n)]
        for y, yi in zip(ys, inner):
            s.poly([(0, y), (0.4, y), (0.8, yi), (back(yi), yi)])
    x = tip
    if inv:
        s.circle(tip + BUBBLE + STROKE / 2, out_y, BUBBLE)
        x = tip + 2 * BUBBLE + STROKE
    s.line(x, out_y, 4.0, out_y)
    return s


# --------------------------------------------------------------- gates (IEC)

def gate_iec(symbol, inputs, h, out_y, inv):
    s = Sprite(4, h, 128)
    ys = [pin_y(p) for p in inputs]
    if h <= 2:
        top, bot = 0.12, 1.88
    else:
        top, bot = 0.02, h - 0.1
    x0, x1 = 1.1, 2.9
    s.rect(x0, top, x1 - x0, bot - top, 0.06)
    for y in ys:
        s.line(0, y, x0, y)
    s.text((x0 + x1) / 2, out_y if h <= 2 else (top + 0.7), symbol, size=0.78, weight=500)
    x = x1
    if inv:
        s.circle(x1 + BUBBLE + STROKE / 2, out_y, BUBBLE)
        x = x1 + 2 * BUBBLE + STROKE
    s.line(x, out_y, 4.0, out_y)
    return s


GATES = {
    # name: (us kind, iec symbol, inverted)
    "and": ("and", "&amp;", False),
    "nand": ("nand", "&amp;", True),
    "or": ("or", "≥1", False),
    "nor": ("nor", "≥1", True),
    "xor": ("xor", "=1", False),
    "not": ("not", "1", True),
}


def all_gates():
    out = {}
    for name, (kind, sym, inv) in GATES.items():
        ins = [0.75] if name == "not" else [0.25, 1.25]
        out[f"{name}_us"] = gate_us(kind, ins, 2, 1.0, inv)
        out[f"{name}_fi"] = gate_iec(sym, ins, 2, 1.0, inv)
        if name in ("and", "nand", "or", "nor"):
            ins3 = [0.25, 0.75, 1.25]
            out[f"{name}3_us"] = gate_us(kind, ins3, 2, 1.0, inv)
            out[f"{name}3_fi"] = gate_iec(sym, ins3, 2, 1.0, inv)
            ins8 = [-0.05 + 0.5 * i for i in range(8)]
            out[f"{name}8_us"] = gate_us(kind, ins8, 4, 2.0, inv)
            out[f"{name}8_fi"] = gate_iec(sym, ins8, 4, 2.0, inv)
    return out


# ------------------------------------------------------------ latches, boxes

def labeled_box(w, h, ppu, box, left, right, top=(), bottom=(), title=None,
                clock=None, draw_w=None, draw_h=None):
    """left/right: [(pin_y_center, label, overline)], top/bottom:
    [(x_center, label, overline)]; box = (x0, y0, x1, y1)."""
    s = Sprite(draw_w or w, draw_h or h, ppu)
    x0, y0, x1, y1 = box
    s.rect(x0, y0, x1 - x0, y1 - y0, 0.06)
    size = 0.5 if (y1 - y0) < 2.0 else 0.62
    for y, lab, ov in left:
        s.line(0, y, x0, y)
        if lab:
            s.text(x0 + 0.16, y, lab, size, anchor="start", overline=ov)
    for y, lab, ov in right:
        s.line(x1, y, w, y)
        if lab:
            s.text(x1 - 0.16, y, lab, size, anchor="end", overline=ov)
    for x, lab, ov in top:
        s.line(x, 0, x, y0)
        if lab:
            s.text(x, y0 + 0.42, lab, size, overline=ov)
    for x, lab, ov in bottom:
        s.line(x, y1, x, h)
        if lab:
            s.text(x, y1 - 0.42, lab, size, overline=ov)
    if clock is not None:
        c = 0.26
        s.poly([(x0, clock - c), (x0 + c * 1.3, clock), (x0, clock + c)])
    if title:
        s.text((x0 + x1) / 2, y0 + 0.45, title, 0.56, weight=600)
    return s


def latches():
    out = {}
    L = lambda y, t, o=False: (pin_y(y), t, o)
    box = (1.1, 0.1, 2.9, 1.9)
    out["d"] = labeled_box(4, 2, 128, box, [L(0.25, "D"), L(1.25, "E")], [L(0.25, "Q"), L(1.25, "Q", True)])
    out["jk"] = labeled_box(4, 2, 128, box, [L(0.25, "J"), L(1.25, "K")], [L(0.25, "Q"), L(1.25, "Q", True)])
    out["sr"] = labeled_box(4, 2, 128, box, [L(0.25, "S"), L(1.25, "R")], [L(0.25, "Q"), L(1.25, "Q", True)])
    out["sr_neg"] = labeled_box(4, 2, 128, box, [L(0.25, "S", True), L(1.25, "R", True)],
                                [L(0.25, "Q"), L(1.25, "Q", True)])
    out["t"] = labeled_box(4, 2, 128, box, [L(0.25, "T"), L(1.25, "E")], [L(0.25, "Q"), L(1.25, "Q", True)])
    # ser: 4x3 chip drawn as 4x4
    out["ser"] = labeled_box(4, 3, 128, (1.1, 0.1, 2.9, 2.9),
                             [L(0.25, "S"), L(1.25, "E"), L(2.25, "R")],
                             [L(0.25, "Q"), L(1.25, "Q", True)], draw_h=4)
    # flip-flops: 5x5 chips drawn as 8x8
    fbox = (1.0, 0.75, 4.0, 4.25)
    outs = [L(1.25, "Q"), L(3.25, "Q", True)]
    sr_async = dict(top=[(2.5, "S", False)], bottom=[(2.5, "R", False)])
    out["d_flipflop"] = labeled_box(5, 5, 64, fbox, [L(1.25, "D"), L(3.25, "")], outs,
                                    clock=pin_y(3.25), draw_w=8, draw_h=8, **sr_async)
    out["t_flipflop"] = labeled_box(5, 5, 64, fbox, [L(1.25, "T"), L(3.25, "")], outs,
                                    clock=pin_y(3.25), draw_w=8, draw_h=8, **sr_async)
    out["jk_flipflop"] = labeled_box(5, 5, 64, fbox, [L(1.25, "J"), L(2.25, ""), L(3.25, "K")], outs,
                                     clock=pin_y(2.25), draw_w=8, draw_h=8, **sr_async)
    out["sr_flipflop"] = labeled_box(5, 5, 64, fbox, [L(1.25, "S"), L(2.25, ""), L(3.25, "R")], outs,
                                     clock=pin_y(2.25), draw_w=8, draw_h=8)
    # dx: 5x8 drawn as 8x8
    out["dx"] = labeled_box(5, 8, 64, (1.0, 0.05, 4.0, 7.95),
                            [L(1.25, "0"), L(2.25, "1"), L(3.25, "2"),
                             L(5.25, "EN"), L(6.25, "EN", True), L(7.25, "EN", True)],
                            [L(i + 0.25, str(i)) for i in range(8)], title=None, draw_w=8, draw_h=8)
    s = out["dx"]
    s.text(2.1, 0.5, "DX", 0.56)
    out["mux"] = labeled_box(5, 8, 64, (1.0, 0.05, 4.0, 7.95),
                             [L(1.25, "G0"), L(2.25, "G1"), L(3.25, "0"), L(4.25, "1"),
                              L(5.25, "2"), L(6.25, "3"), L(7.25, "EN", True)],
                             [L(1.25, "Q"), L(3.25, "Q", True)], title="MUX", draw_w=8, draw_h=8)
    return out


def power():
    out = {}
    g = Sprite(2, 2, 128)
    g.line(1.0, 0.0, 1.0, 0.95)
    for i, hw in enumerate((0.62, 0.40, 0.18)):
        y = 0.95 + i * 0.22
        g.line(1.0 - hw, y, 1.0 + hw, y)
    out["gnd"] = g
    v = Sprite(2, 2, 128)
    v.line(1.0, 2.0, 1.0, 1.05)
    v.poly([(0.55, 1.05), (1.0, 0.45), (1.45, 1.05)], closed=True)
    out["vcc"] = v
    return out


# ------------------------------------------------------ colored hardware parts
# Drawn untinted by the engine (0xffffffff), so these carry real colors.
# Labels are overlaid by the chips in black (keys, clock) or translucent
# white (IC part numbers), so surfaces under them stay light or dark.

def raw(s, el):
    s.add(el)


def rrect(x, y, w, h, r, fill, stroke=None, sw=0.0):
    st = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ' stroke="none"'
    return (f'<rect x="{x:.4f}" y="{y:.4f}" width="{w:.4f}" height="{h:.4f}" rx="{r:.4f}" '
            f'fill="{fill}"{st}/>')


def ic_package(w, h, pins, x0, pitch, top_c, bot_c, body, px):
    s = Sprite(w, h, 0, px=px)
    by0, by1 = body
    leg_w = 0.2
    for k in range(pins):
        cx = x0 + pitch * k + 0.25
        for c, edge in ((top_c, by0), (bot_c, by1)):
            ya, yb = sorted((c - 0.12, edge))
            raw(s, rrect(cx - leg_w / 2, ya, leg_w, yb - ya, 0.03, "#a3a9b2"))
            raw(s, rrect(cx - leg_w / 2 + 0.05, ya, 0.05, yb - ya, 0.0, "#c7ccd3"))
    raw(s, rrect(0.05, by0, w - 0.1, by1 - by0, 0.08, "#2c3037", "#4d535d", 0.035))
    # top light edge, keeps the body readable on the dark canvas
    raw(s, rrect(0.12, by0 + 0.06, w - 0.24, 0.035, 0.0, "#3f454f"))
    mid = (by0 + by1) / 2
    raw(s, f'<path d="M0.05,{mid - 0.2:.4f} A0.2,0.2 0 0 1 0.05,{mid + 0.2:.4f} Z" fill="#17191d" stroke="none"/>')
    raw(s, f'<circle cx="0.42" cy="{by1 - 0.2:.4f}" r="0.07" fill="#3f444d" stroke="none"/>')
    return s


def packages():
    out = {}
    out["chip_14pin"] = ic_package(4.0, 2.0, 7, 0.15, 0.54, 0.25, 1.75, (0.45, 1.55), (512, 256))
    out["chip_16pin"] = ic_package(4.54, 2.25, 8, 0.15, 0.54, 0.25, 2.0, (0.5, 1.75), (512, 256))
    out["chip_20pin"] = ic_package(5.7, 2.8, 10, 0.15, 0.54, 0.4, 2.35, (0.65, 2.1), (1024, 512))
    out["chip_24pin"] = ic_package(6.8, 3.4, 12, 0.13, 0.5475, 0.0, 3.35, (0.3, 3.05), (1024, 512))
    return out


def inputs_parts():
    out = {}
    # Push button: flat keycap, label goes on the top face at (0.75, 0.25).
    b = Sprite(2, 2, 128)
    raw(b, rrect(0.08, 0.08, 1.84, 1.84, 0.22, "#b9bdc5"))
    raw(b, rrect(0.08, 0.08, 1.84, 1.7, 0.22, "#d3d6dc"))
    raw(b, rrect(0.26, 0.16, 1.48, 1.36, 0.16, "#eef0f3"))
    out["button"] = b
    # Toggle: rocker in a dark bezel. The engine flips the texture
    # vertically for the "on" state, so the raised half swaps.
    t = Sprite(2, 2, 128)
    raw(t, rrect(0.12, 0.08, 1.76, 1.84, 0.2, "#2c2f35", "#454a53", 0.04))
    raw(t, rrect(0.32, 0.26, 1.36, 1.48, 0.1, "#c4c8cf"))
    raw(t, rrect(0.32, 0.26, 1.36, 0.8, 0.1, "#f1f2f4"))
    raw(t, rrect(0.32, 0.98, 1.36, 0.08, 0.0, "#dfe1e5"))
    out["switch"] = t
    # Clock: crystal can. Labels and the phase bar sit on the can face.
    c = Sprite(2, 2, 128)
    for lx in (0.62, 1.38):
        raw(c, rrect(lx - 0.04, 1.5, 0.08, 0.5, 0.02, "#a3a9b2"))
    raw(c, rrect(0.1, 1.36, 1.8, 0.18, 0.06, "#aeb3ba"))
    raw(c, rrect(0.2, 0.1, 1.6, 1.32, 0.2, "#dde0e5", "#c3c7ce", 0.03))
    raw(c, rrect(0.3, 0.18, 1.4, 0.05, 0.0, "#eef0f3"))
    out["clock"] = c
    return out


def main():
    keep = None
    if "--svg" in sys.argv:
        keep = sys.argv[sys.argv.index("--svg") + 1]
        os.makedirs(keep, exist_ok=True)
    sprites = {}
    sprites.update(all_gates())
    sprites.update(latches())
    sprites.update(power())
    sprites.update(packages())
    sprites.update(inputs_parts())
    with tempfile.TemporaryDirectory() as tmp:
        for name, sp in sorted(sprites.items()):
            svg = sp.svg()
            src = os.path.join(keep or tmp, name + ".svg")
            with open(src, "w", encoding="utf-8") as f:
                f.write(svg)
            dst = os.path.join(DATA, name + ".png")
            subprocess.run(["rsvg-convert", "-f", "png", "-o", dst, src], check=True)
            print(f"{name:14s} {sp.px[0]}x{sp.px[1]}")


if __name__ == "__main__":
    main()
