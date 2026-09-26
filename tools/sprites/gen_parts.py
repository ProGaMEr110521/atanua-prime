#!/usr/bin/env python3
"""Generate the remaining hardware sprites: logic probe, stepper motor,
audio DAC module, and the smoke-emitting diode (+ its smoke puff).

Each texture covers the rectangle its chip draws it into (see the render()
of logicprobe.cpp, stepper.cpp, audiochip.cpp, sedchip.cpp), so pin pads
and overlays land where the engine expects them.

    tools/sprites/gen_parts.py      # writes data/*.png
Needs Pillow.
"""
import os
import random

from PIL import Image, ImageFilter, ImageFont

from gen_displays import Canvas, DATA, save

FONTS = os.path.join(DATA, "fonts")
BOARD = (30, 33, 38, 255)
BOARD_EDGE = (52, 57, 66, 255)
PAD = (176, 182, 191, 255)
PAD_HI = (205, 210, 217, 255)


def font(name, px):
    # px is in output pixels; the canvas draws at 4x and downsamples.
    return ImageFont.truetype(os.path.join(FONTS, name), px * 4)


def text(c, u, v, s, f, fill, anchor="lm"):
    c.d.text((u * c.w * 4, v * c.h * 4), s, font=f, fill=fill, anchor=anchor)


def header(c, u0, u1, centers_v, pad_h):
    """Column of header pads along the left edge (texture units)."""
    for v in centers_v:
        c.rrect(u0, v - pad_h / 2, u1, v + pad_h / 2, pad_h * 0.2, PAD)
        c.rrect(u0, v - pad_h / 2, u0 + (u1 - u0) * 0.35, v + pad_h / 2, pad_h * 0.2, PAD_HI)


# ------------------------------------------------------------- logic probe
# Chip 18x9, texture drawn over the whole chip (1024x512 = ~57 px/unit).
# Traces are drawn by the engine in x 1.6..16.6, y 2.75..6.25; the hex
# readout sits at (1.6, 2.75). The screen is an OLED-style dark panel.

def gen_logicprobe():
    W, H = 18.0, 9.0
    c = Canvas(1024, 512)
    U = lambda x: x / W
    V = lambda y: y / H
    c.rrect(U(0.3), V(0.6), U(17.8), V(8.4), 0.012, BOARD, BOARD_EDGE, 0.0025)
    # pins: y = 2 + 0.56 k (set), centers +0.25
    header(c, U(0.0), U(0.62), [V(2.25 + 0.56 * k) for k in range(8)], V(0.34))
    # screen bezel and glass
    c.rrect(U(1.05), V(1.35), U(17.2), V(7.65), 0.008, (14, 16, 19, 255), (40, 44, 51, 255), 0.002)
    c.rrect(U(1.35), V(1.75), U(16.9), V(7.25), 0.005, (9, 18, 14, 255))
    # faint channel guides behind the traces
    ystep = (H - 5.5) / 8
    for k in range(8):
        y = 2.75 + ystep * k + ystep * 0.45
        c.rrect(U(1.6), V(y), U(16.6), V(y + 0.012), 0, (22, 44, 32, 255))
    f = font("JetBrainsMono-Regular.ttf", 17)
    text(c, U(1.45), V(8.05), "LOGIC PROBE", f, (120, 128, 140, 255))
    text(c, U(16.9), V(8.05), "8 CH  1 kHz", f, (84, 90, 100, 255), anchor="rm")
    for k in range(8):
        text(c, U(0.72), V(2.25 + 0.56 * k), str(7 - k), font("JetBrainsMono-Regular.ttf", 12),
             (110, 118, 130, 255))
    save(c.image(), "lcd")


# ----------------------------------------------------------------- stepper
# Chip 5x5, texture drawn at (0.25, 0) size 5x5. Coil pins at the bottom:
# centers x = 1.5, 2.5, 3.5, 4.5 (chip) -> 1.25 .. 4.25 (texture), y 4.75.
# The engine draws the rotor arrow around (2.65, 1.9) in texture space.

def gen_stepper():
    c = Canvas(512, 512)
    S = lambda x: x / 5.0
    # coil wires first, under the body
    wires = [(1.25, (214, 64, 58, 255)), (2.25, (58, 120, 214, 255)),
             (3.25, (70, 170, 96, 255)), (4.25, (40, 42, 46, 255))]
    for x, col in wires:
        c.rrect(S(x - 0.07), S(3.9), S(x + 0.07), S(4.72), 0.01, col)
        c.rrect(S(x - 0.1), S(4.55), S(x + 0.1), S(4.72), 0.01, PAD)
    # square frame with chamfered corners (octagon)
    x0, y0, x1, y1, ch = 0.45, 0.02, 4.85, 3.78, 0.42
    c.poly([(S(x0 + ch), S(y0)), (S(x1 - ch), S(y0)), (S(x1), S(y0 + ch)), (S(x1), S(y1 - ch)),
            (S(x1 - ch), S(y1)), (S(x0 + ch), S(y1)), (S(x0), S(y1 - ch)), (S(x0), S(y0 + ch))],
           (188, 193, 201, 255))
    c.poly([(S(x0 + ch + 0.06), S(y0 + 0.06)), (S(x1 - ch - 0.06), S(y0 + 0.06)),
            (S(x1 - 0.06), S(y0 + ch + 0.06)), (S(x1 - 0.06), S(y1 - ch - 0.06)),
            (S(x1 - ch - 0.06), S(y1 - 0.06)), (S(x0 + ch + 0.06), S(y1 - 0.06)),
            (S(x0 + 0.06), S(y1 - ch - 0.06)), (S(x0 + 0.06), S(y0 + ch + 0.06))],
           (206, 211, 218, 255))
    for (hx, hy) in ((0.95, 0.52), (4.35, 0.52), (0.95, 3.28), (4.35, 3.28)):
        c.ellipse(S(hx), S(hy), S(0.17), S(0.17), (58, 62, 70, 255), (150, 155, 163, 255), 0.006)
    cx, cy = 2.65, 1.9
    c.ellipse(S(cx), S(cy), S(1.28), S(1.28), (224, 227, 232, 255), (160, 165, 173, 255), 0.008)
    c.ellipse(S(cx), S(cy), S(0.92), S(0.92), (236, 238, 241, 255), (190, 194, 201, 255), 0.004)
    c.ellipse(S(cx), S(cy), S(0.2), S(0.2), (120, 125, 133, 255))
    save(c.image(), "stepper")


# --------------------------------------------------------------- audio DAC
# Chip 6.5x6, texture drawn at (0.25, 0) size 6x6. Pins A7..A0 on the left:
# centers y = 1.75 .. 5.25, x = 0.25 chip -> 0.0 texture.

def gen_audio():
    c = Canvas(512, 512)
    S = lambda x: x / 6.0
    c.rrect(S(0.15), S(0.15), S(5.9), S(5.85), 0.03, BOARD, BOARD_EDGE, 0.005)
    header(c, S(0.0), S(0.5), [S(1.75 + 0.5 * k) for k in range(8)], S(0.3))
    # traces from the header into the DAC
    for k in range(8):
        y = 1.75 + 0.5 * k
        ty = 3.25 + 0.3 * (k - 3.5) * 0.9
        c.poly([(S(0.5), S(y - 0.03)), (S(1.0), S(y - 0.03)), (S(1.45), S(ty - 0.03)),
                (S(1.7), S(ty - 0.03)), (S(1.7), S(ty + 0.03)), (S(1.45), S(ty + 0.03)),
                (S(1.0), S(y + 0.03)), (S(0.5), S(y + 0.03))], (58, 66, 76, 255))
    # DAC package
    c.rrect(S(1.7), S(2.0), S(3.0), S(4.5), 0.012, (44, 48, 55, 255), (74, 80, 91, 255), 0.004)
    for k in range(6):
        y = 2.25 + 0.4 * k
        c.rrect(S(1.58), S(y - 0.06), S(1.72), S(y + 0.06), 0.004, PAD)
        c.rrect(S(2.98), S(y - 0.06), S(3.12), S(y + 0.06), 0.004, PAD)
    text(c, S(2.35), S(3.25), "DAC", font("JetBrainsMono-Regular.ttf", 16), (150, 156, 166, 255), "mm")
    # buzzer
    bx, by, r = 4.55, 2.15, 1.2
    c.ellipse(S(bx), S(by), S(r), S(r), (22, 24, 28, 255), (70, 76, 86, 255), 0.006)
    for rr in (1.0, 0.7):
        c.ellipse(S(bx), S(by), S(rr), S(rr), None, (46, 50, 58, 255), 0.004)
    c.ellipse(S(bx), S(by), S(0.2), S(0.2), (72, 78, 88, 255))
    text(c, S(3.5), S(5.2), "AUDIO  8-BIT", font("JetBrainsMono-Regular.ttf", 16), (120, 128, 140, 255), "mm")
    save(c.image(), "audio")


# ------------------------------------------------------- smoke diode (joke)

def gen_sed():
    # Same silhouette as the LED (tinted by the engine), but charred.
    c = Canvas(128, 256)
    for lx in (0.34, 0.66):
        c.rrect(lx - 0.035, 0.6, lx + 0.035, 1.0, 0.01, (120, 120, 120, 255))
    c.rrect(0.14, 0.57, 0.86, 0.66, 0.02, (150, 150, 150, 255))
    c.rrect(0.2, 0.05, 0.8, 0.6, 0.3, (170, 170, 170, 240))
    c.rrect(0.27, 0.26, 0.73, 0.58, 0.2, (64, 64, 64, 255))  # soot inside
    c.poly([(0.42, 0.07), (0.5, 0.2), (0.46, 0.3), (0.55, 0.42)], (40, 40, 40, 255))  # crack
    img = c.image()
    save(img, "sed")
    # Smoke puff: soft noisy blob, grey on opaque black (blended additively).
    random.seed(7)
    n = Image.new("L", (64, 64), 0)
    px = n.load()
    for y in range(64):
        for x in range(64):
            d = ((x - 32) ** 2 + (y - 32) ** 2) ** 0.5 / 26.0
            px[x, y] = int(max(0.0, 1.0 - d) * random.randint(90, 255))
    n = n.filter(ImageFilter.GaussianBlur(2.2)).point(lambda v: min(255, int(v * 1.25)))
    save(Image.merge("RGBA", [n, n, n, Image.new("L", (64, 64), 255)]), "sedr")


def main():
    gen_logicprobe()
    gen_stepper()
    gen_audio()
    gen_sed()


if __name__ == "__main__":
    main()
