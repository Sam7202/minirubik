# asm: the RV32I assembly search

`solve.s` runs the IDA* of `../search.c` with the same tables, coordinates
and move order (`../tables.h`), so its moves and node counts equal the C
search's on every state, and `../rv32/check_ripes` checks it on the host.
Design, measurements and the LED mapping are in sections 5, 8 and 9 of the
[HackMD note](https://hackmd.io/9dUgyW8_SDy4kCeemswbow).

What differs from the C:

- One loop over a fixed array of levels (`slots`); while the children of a
  node are tried, the child's coordinates, the face's table rows and the
  pruning limit stay in registers.
- A child is dropped at the first PDB over `limit = bound - g - 1`, 4-corner
  PDB first. `PRUNE=max` forms the maximum first, as `node_h()` does.
- No multiply or divide: `-march=rv32i` makes one an assembly error.
- Each answer is replayed on `cube.h`'s direct model and its length compared
  with the expected one (T5, T6); the exit code counts the failures.

## Usage

```sh
export PATH="$HOME/Library/xPacks/riscv-none-elf-gcc/current/bin:$PATH"
make run                                  # the 10 cases of cases.s on RV32_ISS
make verify                               # run, then check every answer on the host
make run CASE=21345671111111 EXPECT=11    # one state; EXPECT may be omitted
make verify PROC=RV32_5S                  # the 5-stage pipeline (T7)
make run PRUNE=max                        # bound test as in search.c
make run MOD3=branchless                  # mod 3 without a branch
make size                                 # section sizes
make solve.bin RENDER=1                   # GUI build that draws on the LED matrix
make check-render                         # every LED frame against a 3-D model
```

## LED matrix (`RENDER`)

- `RENDER=0` (default, used for every measurement): no renderer; the image
  is byte-identical to one built without it.
- `RENDER=1`: draws the cube as an unfolded net before the replay and after
  every move it replays, so the frames follow the solver's own output. GUI
  only, since Ripes' CLI has no I/O devices: add one 35 × 25 LED Matrix in
  the I/O tab and load `solve.bin` as a flat binary at address 0.
- `RENDER=2`: the same frames as text; `check_render.py` compares them with
  a 3-D model of the cube that uses none of the solver's tables.

GNU as cannot see the symbols Ripes defines for its I/O devices, so the
Makefile defines `LED_MATRIX_0_BASE`, `_WIDTH` and `_HEIGHT` with the values
Ripes exports for a lone LED Matrix (`0xf0000000`, 35, 25). Pass other values
to `make` if your I/O setup differs. How the mapping is derived: section 8 of
the note.

## Build notes

- Linked with `--no-relax`, so every `la` stays two instructions and the
  counts do not depend on where symbols land (relaxation once changed a
  count by 18,693).
- Ripes' print-string call also prints the trailing NUL, so strings are
  printed with `write` (a7 = 64) and an explicit length.
- Ripes' built-in assembler cannot assemble this file: it has no `.if`,
  `.macro` or `.include`, and evaluates `.equ` expressions in its own order
  (`3*4+1` gives 15). The program is built with GNU as and loaded with
  `-t bin`.

## Files

| File | What |
| :--- | :--- |
| `solve.s` | input check, start coordinates, IDA*, output, self-check, renderer |
| `cases.s` | test cases: 14 characters, expected length, a pad byte; a 0 byte ends the list |
| `Makefile` | GNU as + `../rv32/link.ld` + `objcopy`, loaded with `-t bin` |
| `check_render.py` | the renderer check run by `make check-render` |
