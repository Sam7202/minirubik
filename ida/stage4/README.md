# stage4 — every distance-11 state on Ripes

Raw data for note sections 3.5, 4.5 and 5.4: all 2,644 distance-11 states
on `RV32_ISS`, one Ripes run per state, for the hand-written assembly
(`../asm`) and for the gcc reference (`../rv32`) with three and with two
PDBs. The gcc reference is `../search.c` with its default loop; the
recursive form of tag `stage3-c` (`DFS=recursive`) is kept for comparison.
Host: Apple M1, 8 cores; Ripes v2.2.6-106-g5b8a616.

| File | What |
| :--- | :--- |
| `d11.c` | prints the 2,644 distance-11 states, one per line (input of `batch.py`) |
| `batch.py` | builds one image per state, runs them on Ripes in parallel, writes a CSV |
| `asm.csv` | `../asm`, the hand-written RV32I search |
| `asm_max.csv` | `../asm` built with `PRUNE=max`: h is the maximum of the three PDBs, as in `search.c` |
| `rv32_pdb3.csv` | `../rv32`, gcc -O2, three PDBs |
| `rv32_pdb2.csv` | `../rv32`, gcc -O2, two PDBs |
| `rv32_pdb3_rec.csv` | `../rv32` built with `DFS=recursive`, three PDBs |
| `rv32_pdb2_rec.csv` | `../rv32` built with `DFS=recursive`, two PDBs |

CSV columns: `state`, `iret` (Ripes `--iret`), `len`, `expanded`,
`generated`, `exit` (the program's exit code), `check` (the assembly's own
check of the answer, `ok` or `FAIL`; `-` for the C harness, which does not
check itself).

## Results

| | asm | asm, `PRUNE=max` | gcc, three PDBs | gcc, two PDBs | gcc recursive, three PDBs | gcc recursive, two PDBs |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| States failed | 0 | 0 | 0 | 0 | 0 | 0 |
| Over 5 x 10^7 instructions | 0 | 0 | 0 | 0 | 0 | 0 |
| Mean `--iret` | 1,084,723.0 | 1,242,566.3 | 1,575,885.6 | 6,740,242.4 | 2,040,094.5 | 8,668,650.3 |
| Max `--iret` | 4,296,273 | 4,900,912 | 6,226,078 | 20,844,421 | 8,066,912 | 26,815,803 |
| State at the max | `51342763312223` | `51342763312223` | `51342763312223` | `54721631111111` | `51342763312223` | `54721631111111` |
| Min `--iret` | 519,419 | 596,253 | 755,173 | 4,595,185 | 977,198 | 5,910,179 |
| Wall clock, 8 runs at a time | 2 min 45 s | 2 min 44 s | 5 min 48 s | 9 min 2 s | 6 min 29 s | 10 min 51 s |

`PRUNE=max` evaluates the bound test as the C does (all three PDBs, then
their maximum), and both are a loop over a stack of levels, so against gcc
it differs only in how the code is written: it needs 79% of gcc's
instructions on average (61% of the recursive form's). The early exit of
the default build saves another 157,843.3 on average and 604,639 in the
worst query. The recursive form needs 29% more instructions than the loop
in the mean query with both heuristics; see note section 4.5.

A state fails if its length is not 11, its exit code is not 0, or (asm) its
own check does not print `ok`. Then, on the host, `../rv32/check_ripes`
checked every answer of each batch (replay, optimal length from a full BFS,
moves and node counts equal to the host search): 2,644 of 2,644 in all six.

## Commands

From `ida/`, with the xPack toolchain on PATH, after `make` and
`make PDB=2` (the two-PDB batches need `tables2.c`, which `batch.py` does
not build):

```sh
cc -O2 -std=c99 -Wall -Wextra -Wpedantic -DCORNER_PDB=1 -I. stage4/d11.c tables.c -o stage4/d11
stage4/d11 > stage4/d11.txt
stage4/batch.py asm --raw asm_raw.txt > stage4/asm.csv
stage4/batch.py asm --make PRUNE=max --raw asm_max_raw.txt > stage4/asm_max.csv
stage4/batch.py rv32 --raw pdb3_raw.txt > stage4/rv32_pdb3.csv
stage4/batch.py rv32 --pdb 2 --raw pdb2_raw.txt > stage4/rv32_pdb2.csv
stage4/batch.py rv32 --make DFS=recursive --raw pdb3_rec_raw.txt > stage4/rv32_pdb3_rec.csv
stage4/batch.py rv32 --pdb 2 --make DFS=recursive --raw pdb2_rec_raw.txt > stage4/rv32_pdb2_rec.csv
make -C rv32 check_ripes
rv32/check_ripes < asm_raw.txt
rv32/check_ripes < asm_max_raw.txt
rv32/check_ripes < pdb3_raw.txt
rv32/check_ripes < pdb3_rec_raw.txt
make -C rv32 check_ripes PDB=2
rv32/check_ripes < pdb2_raw.txt
rv32/check_ripes < pdb2_rec_raw.txt
```

The raw outputs (every run's full Ripes output) are not kept in git.

## How each run gets its input

- **asm**: the image is assembled once with the placeholder
  `XXXXXXXXXXXXXX` and an expected length of 11. Each run gets a copy with
  those 14 bytes replaced by the state, the same bytes the assembler would
  emit, so nothing is reassembled.
- **rv32**: the C harness is compiled once per state, with the command that
  `make -n` prints for `../rv32`. Patching the bytes does not work there:
  the input is a compile-time constant, and gcc checks it while compiling,
  so an image built with the placeholder reports it invalid whatever its
  bytes say. The same holds for every single-state measurement of the C:
  gcc can evaluate the input check at compile time, while the assembly
  checks its input at run time.
