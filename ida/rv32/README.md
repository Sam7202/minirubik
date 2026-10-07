# rv32: the C search on Ripes

```
host                          Ripes (target)                     host
../tables.c ────────────────▶ search_rv32.bin ── ripes_out.txt ─▶ check_ripes
(built by build.c,            ../search.c, unmodified            replay, BFS distance,
 linked read-only)                                               host search
```

- `search_rv32.c` includes `../search.c` and `../tables.c` directly, so Ripes
  runs the same search as the host. The tables are const data in `.rodata`;
  nothing is built or copied on the target.
- Linked without libgcc: a multiply, divide or remainder that gcc cannot
  turn into shifts becomes a call to `__mulsi3` and the like, and fails to
  link.
- `check_ripes.c` replays each answer on `cube.h`'s model, checks its length
  against a full BFS, and requires the moves and node counts to equal those
  of the host build of `../search.c`. The target must print
  `STATE -> moves  [len N, expanded E, generated G]`.

This directory is the gcc reference of Stage 4: the loop form of
`../search.c` by default, the recursive form of tag `stage3-c` with
`DFS=recursive`. `../asm/` uses its `link.ld` and `check_ripes`.
Measurements are in sections 4 and 5 of the
[HackMD note](https://hackmd.io/9dUgyW8_SDy4kCeemswbow).

## Usage

```sh
export PATH="$HOME/Library/xPacks/riscv-none-elf-gcc/current/bin:$PATH"
make verify                         # host tables, Ripes search, host check
make run                            # Ripes only: --iret, cycles, CPI
make run CASES='"21345671111111"'   # one state
make run PDB=2                      # two PDBs (../tables2.c)
make run DFS=recursive              # the recursive dfs() of stage3-c
make run ARCH=rv32im ISAEXTS=M      # with the M extension
make run PROC=RV32_5S               # pipeline model (much slower)
make size                           # section sizes
make -C baseline run                # the original solver.c, 4 to 5 minutes
```

The default cases are `cases[]` in `search_rv32.c`: 6 states, including the
costliest one for three PDBs, `51342763312223`. Counts include start-up,
parsing and printing, about 2,000 instructions for the solved state.

## Files

| File | What |
| :--- | :--- |
| `search_rv32.c` | the target program: freestanding runtime, `../search.c`, `../tables.c`, cases |
| `check_ripes.c` | host checker of the Ripes output |
| `link.ld` | flat layout for `-t bin`; `_start` is the first byte |
| `baseline/` | the original solver.c on Ripes |

## Running C on Ripes

- `-t c` compiles with the compiler and arguments in Ripes' settings (here
  `-O0 -g`, with newlib), with no way to pass `-O2`, a linker script or a
  harness. The measured images are therefore built here and loaded with
  `-t bin`.
- There is no command line, so the input is compiled in (`cases[]`).
- newlib's `malloc` grows the heap with `brk` and cannot get large blocks on
  this build; the search uses no heap.
- `-t bin` loads a raw image at address 0 and starts there, hence
  `objcopy -O binary` and `_start` in `.text.init`. This build also accepts
  `-t elf`.
- newlib is left out on purpose, although its `printf` works on Ripes: it
  divides, and without it `--iret` is almost all search.
