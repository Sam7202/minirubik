# stage2 — representation and algorithm

Raw data for note sections 3.2, 3.5 and 3.6. Host: Apple M1, 8 GB RAM,
macOS 15.0 (see ../ENVIRONMENT.md).

| File | What |
| :--- | :--- |
| `verify_pdb3.txt` | `make check`: every check on all 3,674,160 states with the three PDBs, timed |
| `verify_pdb2.txt` | `make check PDB=2`: the same with the permutation and orientation PDBs only |
| `pdb_merge.c` | two experiments on merging PDBs: the states `perm_h` and `ori_h` both put one move from solved, and IDA* with `perm_h + ori_h` |
| `pdb_merge.txt` | output of `pdb_merge`, timed |

## verify_pdb3.txt, verify_pdb2.txt

From `ida/`:

```sh
( time make check ) 2>&1 | tee stage2/verify_pdb3.txt
( time make check PDB=2 ) 2>&1 | tee stage2/verify_pdb2.txt
```

The last line is zsh's `time`; the note reports its `total` (wall clock).
`states over 250000` counts over all states, not only distance 11.

## pdb_merge.txt

From `ida/`, after `make` (it links the generated `tables.c`):

```sh
cc -O2 -std=c99 -Wall -Wextra -Wpedantic -DCORNER_PDB=1 -I. stage2/pdb_merge.c search.c tables.c -o stage2/pdb_merge
( time ./stage2/pdb_merge ) 2>&1 | tee stage2/pdb_merge.txt
```

Part 1 groups the 18 states with `perm_h` = `ori_h` = 1 by true distance
and `c4_h`, with one example each, then prints the example of note 3.2: the
permutation after one `R` with the twists after one `B`.

Part 2 runs IDA* with `h = perm_h + ori_h` on every unsolved state and
counts the answers longer than the true distance. `moves too long: 1:1025178`
means 1,025,178 answers are one move too long. The last line is the first
such state at the smallest distance, with the optimal answer (from the real
search) and the one the sum returns.
