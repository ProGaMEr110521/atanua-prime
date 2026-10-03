# AI circuit tools

Machine interface for generating Atanua circuits programmatically.
Two artifacts, one generator; tests keep them honest.

## tools/chips.json — the chip catalog

Mined from `src/chip/*.cpp` + `src/core/basechipfactory.cpp` by
`tools/mine_pins.py` (re-run it after any chip change and commit both
outputs; `test_chip_catalog_in_sync` enforces this).

- 269 chips: `name` (exact string for `<Chip Name="">`), `category`
  (palette tab: Base/Chips/In/Out/Misc; `null` means loadable but not in
  the palette, e.g. `Connection Pin`), `class`, `file`, `dynamic`.
- `pins[]` in pad-index order: `index`, `member`, `label` (verbatim UI
  tooltip), `readonly` (ground truth from code), `role` (`in`/`out`/
  `bidir`, mined from read/write behavior with label fallback).
- Conventions worth knowing: `*.atanua (Box)` has dynamic pins from its
  subfile; `... inverted` variants are active-low (labels show the base
  form); LED-grid labels follow `Pin N: Row/Col` in the app; pad indices
  are positions in the file's `<Chip>` order and in each chip's pin list.
- Coordinates are integer world units: divide `xpos`/`ypos` by 1048576
  for canvas units, and snap generated chips to round multiples like the
  fixtures do.

## atanua --validate — the file checker

`atanua.exe --validate <file>` checks a circuit without opening any
window (exit before GL/audio init, so it also runs headless):

- exit `0` + `OK: N chips, M wires`; exit `1` + `ERROR ...` per problem
  (unknown chip, pad/chip index out of range, missing attributes,
  malformed XML, missing box subfile); exit `2` for misuse.
- Rules mirror the loader (`do_loadxml`): names match
  case-insensitively, wires may only reference earlier chips, Box
  subfiles resolve next to the circuit. Anything the loader would
  silently drop is an error here instead.
- Pin counts come from `src/include/chipdb.h`, emitted by the same
  miner run as the JSON catalog.

## Clean wiring — 90-degree schemes without touching the model

A `Wire` always draws straight, so every bend is one or two `Connection
Pin` chips joining shorter segments. Generators have two ways to stay
clean; both are file-level, no simulation changes.

- By hand: follow `.grok/skills/atanua-clean-schemes/SKILL.md` (grid,
  L/Z patterns, fan-out sharing, validate loop). Copy the nearest file
  under `tools/examples/` (`and_led_L`, `and_led_Z`,
  `fanout_shared_anchor`) instead of inventing geometry.
- Automatic: `python tools/route_wires.py in.atanua out.atanua` snaps
  chips to an 8-unit grid and replaces each long diagonal with an
  `A -> a1 -> a2 -> B` Z-bend whose middle trunk shares one X or one Y
  exactly. Wires already touching a `Connection Pin` are left alone.
  Every routed net gets fresh anchors, and lanes are offset so parallel
  trunks never overlap and anchors never land inside a chip body.

Regression: `python tests/test_route_wires.py` (diagonals routed,
trunks axis-aligned, goldens stable plus validate/simulate behavior).

## atanua --simulate — headless batch runs

`atanua.exe --simulate <file> [--ticks N] [--set C:P=V ...]` loads a
circuit with the real loader, runs the real propagation loop on virtual
time (same `mPhysicsKHz` substeps, same dirty marking), and prints one
JSON object: file, ticks, chip/wire counts, `wireLegacy` (loaded wire
style: 0 modern, 1 legacy), `leds` (chip, name, state),
`nets` (index, state, pin count), `pins` (chip, pad, net, state).
States are `high`/`low`/`nc`/`invalid` (`unconnected` for loose pads).

- `--set` forces an output pad for the whole run (ideal driver applied
  after every chip-update phase); repeat the flag per stimulus.
- Exit codes match `--validate` (0/1/2). Ticks default to 100.
- Needs no window, GL context, or audio device (textures hand out
  dummies headlessly); `SDL_VIDEODRIVER=dummy` works.
- Limits, stated plainly: stimulus is constant per run (no timed
  sequences), Box-heavy files above the config limit are refused
  instead of prompting, and clocks follow virtual milliseconds.
