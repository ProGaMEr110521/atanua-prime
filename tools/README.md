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
