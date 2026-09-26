#!/usr/bin/env python3
"""Generate the LED and segment-display sprites.

The engine draws each display as a base texture (drawn as-is) plus one
texture per lit segment, blended additively and tinted with the display
color. So:

  *_base / 309     a dark display face with dim unlit segments
  segment layers   white (or red for the TIL309, which is not tinted)
                   segment plus a soft bloom, on OPAQUE black: 16-seg
                   blends with (ONE, SRC_ALPHA), which needs alpha = 1
  led              neutral light greys; the engine multiplies in the color
  ledgrid_*        tiled 2x2 cell panel, and a round lit dot

Segment geometry is shared between each base and its layers, and slots
match the original files' positions so pin names keep their meaning.

    tools/sprites/gen_displays.py      # writes data/*.png
Needs Pillow.
"""
import os

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DATA = os.path.join(ROOT, "data")
SS = 4  # supersampling factor


def hexbar(x0, y0, x1, y1, t, gap):
    """Segment polygon from (x0,y0) to (x1,y1), thickness t, pointed ends."""
    dx, dy = x1 - x0, y1 - y0
    ln = (dx * dx + dy * dy) ** 0.5
    ux, uy = dx / ln, dy / ln
    nx, ny = -uy, ux
    x0, y0 = x0 + ux * gap, y0 + uy * gap
    x1, y1 = x1 - ux * gap, y1 - uy * gap
    h = t / 2
    return [(x0, y0), (x0 + ux * h + nx * h, y0 + uy * h + ny * h),
            (x1 - ux * h + nx * h, y1 - uy * h + ny * h), (x1, y1),
            (x1 - ux * h - nx * h, y1 - uy * h - ny * h), (x0 + ux * h - nx * h, y0 + uy * h - ny * h)]


def diagbar(x0, y0, x1, y1, t, inset):
    """Diagonal stroke between frame points, pulled in from both ends."""
    dx, dy = x1 - x0, y1 - y0
    ln = (dx * dx + dy * dy) ** 0.5
    ux, uy = dx / ln, dy / ln
    nx, ny = -uy * t / 2, ux * t / 2
    a = (x0 + ux * inset, y0 + uy * inset)
    b = (x1 - ux * inset, y1 - uy * inset)
    return [(a[0] + nx, a[1] + ny), (b[0] + nx, b[1] + ny), (b[0] - nx, b[1] - ny), (a[0] - nx, a[1] - ny)]


def slant(poly, k, vmid=0.5):
    return [(u + (vmid - v) * k, v) for u, v in poly]


class Canvas:
    def __init__(self, w, h, bg=(0, 0, 0, 0)):
        self.w, self.h = w, h
        self.im = Image.new("RGBA", (w * SS, h * SS), bg)
        self.d = ImageDraw.Draw(self.im)

    def P(self, pts):
        return [(u * self.w * SS, v * self.h * SS) for u, v in pts]

    def poly(self, pts, fill):
        self.d.polygon(self.P(pts), fill=fill)

    def rrect(self, u0, v0, u1, v1, r, fill, outline=None, width=0.0):
        box = self.P([(u0, v0), (u1, v1)])
        self.d.rounded_rectangle([box[0], box[1]], radius=r * self.w * SS, fill=fill,
                                 outline=outline, width=int(width * self.w * SS) if outline else 0)

    def ellipse(self, cu, cv, ru, rv, fill, outline=None, width=0.0):
        box = self.P([(cu - ru, cv - rv), (cu + ru, cv + rv)])
        self.d.ellipse([box[0], box[1]], fill=fill, outline=outline,
                       width=int(width * self.w * SS) if outline else 0)

    def image(self):
        return self.im.resize((self.w, self.h), Image.LANCZOS)


def lit_layer(w, h, draw, color=(255, 255, 255), bloom=0.022, bloom_gain=0.55):
    """Segment in `color` with a soft bloom, on opaque black."""
    mask = Canvas(w, h)
    draw(mask, (255, 255, 255, 255))
    core = mask.image().split()[3]
    glow = core.filter(ImageFilter.GaussianBlur(bloom * w)).point(lambda v: int(v * bloom_gain))
    both = ImageChops.lighter(core, glow)
    chans = [both.point(lambda v, k=k: v * k // 255) for k in color]
    return Image.merge("RGBA", chans + [Image.new("L", (w, h), 255)])


def save(img, name):
    img.save(os.path.join(DATA, name + ".png"))
    print(f"{name:14s} {img.size[0]}x{img.size[1]}")


BODY = (21, 23, 27, 255)
BODY_EDGE = (46, 50, 58, 255)
UNLIT = (37, 41, 48, 255)


# ------------------------------------------------------------------ 7-seg

def seven_segments(t=0.075, gap=0.013):
    L, R, T, M, B = 0.305, 0.695, 0.105, 0.5, 0.895
    segs = {
        "a": hexbar(L, T, R, T, t, gap), "b": hexbar(R, T, R, M, t, gap),
        "c": hexbar(R, M, R, B, t, gap), "d": hexbar(L, B, R, B, t, gap),
        "e": hexbar(L, M, L, B, t, gap), "f": hexbar(L, T, L, M, t, gap),
        "g": hexbar(L, M, R, M, t, gap),
    }
    return {k: slant(v, 0.17) for k, v in segs.items()}


def gen_7seg():
    W = 512
    segs = seven_segments()
    dp = (0.775, 0.875, 0.038)
    base = Canvas(W, W)
    base.rrect(0.17, 0.008, 0.86, 0.992, 0.035, BODY, BODY_EDGE, 0.006)
    for poly in segs.values():
        base.poly(poly, UNLIT)
    base.ellipse(dp[0], dp[1], dp[2], dp[2], UNLIT)
    save(base.image(), "7seg_base")
    for k, poly in segs.items():
        save(lit_layer(W, W, lambda c, f, p=poly: c.poly(p, f)), "7seg_" + k)
    save(lit_layer(W, W, lambda c, f: c.ellipse(dp[0], dp[1], dp[2], dp[2], f)), "7seg_h")


# ----------------------------------------------------------------- 16-seg

def sixteen_segments(t=0.058, gap=0.011):
    L, R, T, M, B, C = 0.265, 0.735, 0.115, 0.5, 0.885, 0.5
    ins = t * 1.15
    segs = {
        "a1": hexbar(L, T, C, T, t, gap), "a2": hexbar(C, T, R, T, t, gap),
        "d1": hexbar(L, B, C, B, t, gap), "d2": hexbar(C, B, R, B, t, gap),
        "g1": hexbar(L, M, C, M, t, gap), "g2": hexbar(C, M, R, M, t, gap),
        "f": hexbar(L, T, L, M, t, gap), "b": hexbar(R, T, R, M, t, gap),
        "e": hexbar(L, M, L, B, t, gap), "c": hexbar(R, M, R, B, t, gap),
        "j": hexbar(C, T, C, M, t, gap), "m": hexbar(C, M, C, B, t, gap),
        "h": diagbar(L, T, C, M, t * 0.9, ins), "k": diagbar(R, T, C, M, t * 0.9, ins),
        "n": diagbar(L, B, C, M, t * 0.9, ins), "l": diagbar(R, B, C, M, t * 0.9, ins),
    }
    return {k: slant(v, 0.15) for k, v in segs.items()}


def gen_16seg():
    W = 512
    segs = sixteen_segments()
    dp = (0.79, 0.905, 0.03)
    base = Canvas(W, W)
    base.rrect(0.17, 0.008, 0.83, 0.992, 0.03, BODY, BODY_EDGE, 0.006)
    for poly in segs.values():
        base.poly(poly, UNLIT)
    base.ellipse(dp[0], dp[1], dp[2], dp[2], UNLIT)
    save(base.image(), "16seg_base")
    for k, poly in segs.items():
        save(lit_layer(W, W, lambda c, f, p=poly: c.poly(p, f), bloom=0.018), "16seg_" + k)
    save(lit_layer(W, W, lambda c, f: c.ellipse(dp[0], dp[1], dp[2], dp[2], f), bloom=0.018), "16seg_dp")


# ----------------------------------------------------------------- TIL309

RED = (255, 52, 40)


def til309_segments(t=0.05, gap=0.01):
    L, R, T, M, B = 0.3, 0.66, 0.25, 0.53, 0.81
    segs = {
        "a": hexbar(L, T, R, T, t, gap), "b": hexbar(R, T, R, M, t, gap),
        "c": hexbar(R, M, R, B, t, gap), "d": hexbar(L, B, R, B, t, gap),
        "e": hexbar(L, M, L, B, t, gap), "f": hexbar(L, T, L, M, t, gap),
        "g": hexbar(L, M, R, M, t, gap),
    }
    return {k: slant(v, 0.12, 0.53) for k, v in segs.items()}


def gen_309():
    segs = til309_segments()
    dp = (0.745, 0.85, 0.03)
    # base covers the whole 3x6 chip (256x512); segments use the top half
    base = Canvas(256, 512)
    base.rrect(0.13, 0.005, 0.87, 0.995, 0.06, BODY, BODY_EDGE, 0.012)
    base.rrect(0.2, 0.06, 0.8, 0.47, 0.05, (44, 12, 14, 255), (70, 24, 26, 255), 0.01)
    half = lambda pts: [(u, v / 2) for u, v in pts]
    for poly in segs.values():
        base.poly(half(poly), (74, 22, 24, 255))
    base.ellipse(dp[0], dp[1] / 2, dp[2], dp[2] / 2, (74, 22, 24, 255))
    save(base.image(), "309")
    for k, poly in segs.items():
        save(lit_layer(256, 256, lambda c, f, p=poly: c.poly(p, f), color=RED, bloom=0.03), "309_" + k)
    save(lit_layer(256, 256, lambda c, f: c.ellipse(dp[0], dp[1], dp[2], dp[2], f), color=RED, bloom=0.03),
         "309_h")


# -------------------------------------------------------------------- LEDs

def gen_led():
    # 1x2 world units, tinted by the LED color (multiplied), so neutral.
    c = Canvas(128, 256)
    for lx in (0.34, 0.66):
        c.rrect(lx - 0.035, 0.6, lx + 0.035, 1.0, 0.01, (150, 150, 150, 255))
    c.rrect(0.14, 0.57, 0.86, 0.66, 0.02, (205, 205, 205, 255))
    # dome: rounded top via a tall rounded rect clipped by the flange
    c.rrect(0.2, 0.05, 0.8, 0.6, 0.3, (232, 232, 232, 245))
    c.rrect(0.29, 0.13, 0.4, 0.5, 0.06, (255, 255, 255, 255))
    c.rrect(0.62, 0.2, 0.71, 0.55, 0.05, (210, 210, 210, 255))
    save(c.image(), "led")


def gen_ledgrid():
    # Base tile: 2x2 cells, repeated over the grid.
    c = Canvas(128, 128, (19, 21, 25, 255))
    for i in range(2):
        for j in range(2):
            cu, cv = 0.25 + 0.5 * i, 0.25 + 0.5 * j
            c.ellipse(cu, cv, 0.17, 0.17, (38, 42, 49, 255), (52, 57, 66, 255), 0.012)
    save(c.image(), "ledgrid_base")
    save(lit_layer(64, 64, lambda k, f: k.ellipse(0.5, 0.5, 0.33, 0.33, f), bloom=0.09, bloom_gain=0.6),
         "ledgrid_lit")


def main():
    gen_7seg()
    gen_16seg()
    gen_309()
    gen_led()
    gen_ledgrid()


if __name__ == "__main__":
    main()
