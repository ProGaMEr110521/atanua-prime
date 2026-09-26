# Asset tools

The sprites and fonts behind Atanua Prime's look are generated, not drawn by
hand. Change a script, run it, and commit the regenerated files in `data/`.
After regenerating, copy `data/` next to your built binary again (CMake does
this on every relink) to see the result.

## Sprites

Every sprite is drawn in world units straight from the pin coordinates in the
matching `src/chip/*.cpp`, and covers exactly the rectangle that chip's
`render()` draws it into, so leads land on pins.

| Script | Needs | Builds |
|---|---|---|
| `sprites/gen_sprites.py` | `rsvg-convert` (librsvg), Inter installed | ANSI and IEC gates (2, 3, 8 inputs), latches, flip-flops, DX, MUX, GND/VCC (white line art, tinted by the engine); DIP 14/16/20/24 packages, key button, rocker switch, crystal clock |
| `sprites/gen_displays.py` | Pillow | 7-seg, 16-seg and TIL309 displays (base face + one additive layer per segment on opaque black), LED, LED grid |
| `sprites/gen_parts.py` | Pillow, `data/fonts` | Logic probe, stepper motor, audio DAC, smoke-emitting diode and its smoke puff |

Rules the engine relies on:

- Line-art sprites are **white on transparent**; the renderer tints them (white
  on the dark canvas, black on paper).
- Segment layers are **opaque black with the segment in white** (red for the
  TIL309, which is not tinted): 16-seg blends with `(GL_ONE, GL_SRC_ALPHA)`,
  which needs alpha = 1 everywhere.
- Keep power-of-two sizes; the loader builds its own mip chain.

```bash
python3 tools/sprites/gen_sprites.py          # add --svg DIR to keep the SVG sources
python3 tools/sprites/gen_displays.py
cd tools/sprites && python3 gen_parts.py
```

## Fonts

| Script | Needs | Builds |
|---|---|---|
| `fonts/build_fonts.sh` | fonttools, opentype-feature-freezer, `fonts-inter`, `fonts-jetbrains-mono` | `data/fonts/Inter-*.otf` with the `ss04` disambiguation feature frozen in (Dear ImGui does not shape text), `JetBrainsMono-Regular.ttf`, all subset to Latin + Cyrillic |
| `fonts/build_bmfonts.py` | Pillow with raqm | `data/vera14.fnt` / `vera31.fnt` bitmap fonts (Inter Medium / JetBrains Mono Medium). The canvas normally renders text through the vector fonts above; these are the fallback |

Licenses for every shipped font are in `data/fonts/`.
