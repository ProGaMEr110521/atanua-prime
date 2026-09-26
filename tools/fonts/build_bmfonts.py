#!/usr/bin/env python3
"""Rebuild the canvas bitmap fonts (AngelCode BMFont, binary v2).

The canvas draws text through src/basecode/angelcodefont.cpp, which reads
data/vera14.fnt and data/vera31.fnt. The file names stay for compatibility;
the glyphs now come from the chrome's type family:

  vera14  Inter Medium (with Cyrillic)  watermark and user name
  vera31  JetBrains Mono Medium         part numbers, labels, readouts

Glyph sets match the original Vera files (218 Latin/Latin-1 + 66 Cyrillic
in vera14), each font lives on one texture page, glyphs are white with the
coverage in alpha (the renderer tints them), and kerning pairs are taken
from the font's GPOS table through HarfBuzz (Pillow + raqm).

    tools/fonts/build_bmfonts.py
"""
import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DATA = os.path.join(ROOT, "data")
INTER = os.environ.get("INTER_DIR", "/usr/share/fonts/opentype/inter")
JBM = os.environ.get("JBM_DIR", "/usr/share/fonts/truetype/jetbrains-mono")

LATIN = list(range(32, 127)) + list(range(160, 256))
# The original sets also carry a few CP1252 punctuation/currency glyphs.
LATIN_EXTRA = [0x152, 0x153, 0x160, 0x161, 0x178, 0x17D, 0x17E, 0x192, 0x2C6, 0x2DC,
               0x2013, 0x2014, 0x2018, 0x2019, 0x201A, 0x201C, 0x201D, 0x201E,
               0x2020, 0x2021, 0x2022, 0x2026, 0x2030, 0x2039, 0x203A, 0x20AC, 0x2122]
CYRILLIC = [0x401] + list(range(0x410, 0x450)) + [0x451]
PAD = 3


def original_ids(name):
    """Glyph ids of a shipped .fnt, so the replacement covers the same set."""
    path = os.path.join(DATA, name + ".fnt")
    b = open(path, "rb").read()
    p = 4
    while p < len(b):
        t = b[p]
        sz = struct.unpack("<i", b[p + 1:p + 5])[0]
        body = b[p + 5:p + 5 + sz - 4]
        if t == 4:
            return [struct.unpack("<h", body[i * 18:i * 18 + 2])[0] for i in range(len(body) // 18)]
        p += 5 + sz - 4
    return None


def build(name, font_path, px, page_w, page_h, face_name, ids, kern=True):
    font = ImageFont.truetype(font_path, px, layout_engine=ImageFont.Layout.RAQM)
    ascent, descent = font.getmetrics()
    line_h = ascent + descent
    glyphs = []
    for cp in ids:
        ch = chr(cp)
        if 127 <= cp < 160:
            # C1 control codes ride along in the original sets; keep the
            # ids (the loader indexes them) but draw nothing.
            glyphs.append((cp, ch, 0, 0, 0, 0, 0))
            continue
        l, t, r, b = font.getbbox(ch, anchor="ls")
        adv = font.getlength(ch)
        glyphs.append((cp, ch, l, t, r, b, adv))

    page = Image.new("L", (page_w, page_h), 0)
    draw = ImageDraw.Draw(page)
    x = y = PAD
    row_h = 0
    records = []
    for cp, ch, l, t, r, b, adv in glyphs:
        w, h = max(0, r - l), max(0, b - t)
        if x + w + PAD > page_w:
            x = PAD
            y += row_h + PAD
            row_h = 0
        if y + h + PAD > page_h:
            sys.exit(f"{name}: page {page_w}x{page_h} too small at {px}px")
        if w and h:
            draw.text((x - l, y - t), ch, font=font, fill=255, anchor="ls")
        records.append((cp, x, y, w, h, l, ascent + t, int(round(adv))))
        x += w + PAD
        row_h = max(row_h, h)

    rgba = Image.merge("RGBA", [Image.new("L", page.size, 255)] * 3 + [page])
    png = f"{name}_00.png"
    rgba.save(os.path.join(DATA, png))

    pairs = []
    if kern:
        letters = [g for g in glyphs if g[6] and (g[1].isalnum() or g[1] in ".,-'\"/")]
        for a in letters:
            for bb in letters:
                k = font.getlength(a[1] + bb[1]) - a[6] - bb[6]
                if abs(k) >= 1.0:
                    pairs.append((a[0], bb[0], int(round(k))))

    out = bytearray(b"BMF\x02")
    info = struct.pack("<hBBHBBBBBBBB", px, 0b11000000, 0, 100, 1, 0, 0, 0, 0, 1, 1, 0)
    info += face_name.encode("ascii") + b"\x00"
    out += struct.pack("<Bi", 1, len(info) + 4) + info
    common = struct.pack("<HHHHHB", line_h, ascent, page_w, page_h, 1, 0)
    out += struct.pack("<Bi", 2, len(common) + 4) + common
    pages = png.encode("ascii") + b"\x00"
    out += struct.pack("<Bi", 3, len(pages) + 4) + pages
    chars = b"".join(struct.pack("<hhhhhhhhBB", cp, x, y, w, h, xo, yo, xa, 0, 15)
                     for cp, x, y, w, h, xo, yo, xa in records)
    out += struct.pack("<Bi", 4, len(chars) + 4) + chars
    kb = b"".join(struct.pack("<hhh", a, b, k) for a, b, k in pairs)
    out += struct.pack("<Bi", 5, len(kb) + 4) + kb
    with open(os.path.join(DATA, name + ".fnt"), "wb") as f:
        f.write(out)
    print(f"{name}: {face_name} {px}px, {len(records)} glyphs, {len(pairs)} kern pairs, "
          f"line {line_h}, page {page_w}x{page_h}")


def main():
    ids14 = original_ids("vera14") or sorted(set(LATIN + LATIN_EXTRA + CYRILLIC))
    ids31 = original_ids("vera31") or sorted(set(LATIN + LATIN_EXTRA))
    build("vera14", os.path.join(INTER, "Inter-Medium.otf"), 26, 256, 1024, "Inter Medium", ids14)
    build("vera31", os.path.join(JBM, "JetBrainsMono-Medium.ttf"), 56, 1024, 512,
          "JetBrains Mono Medium", ids31, kern=False)


if __name__ == "__main__":
    main()
