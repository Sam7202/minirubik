# stage3 — C-level optimization

Raw data for note section 4. Tag `stage3-c` marks the C search after
sections 4.1 to 4.4, with a recursive `dfs()`. Section 4.5 then writes the
search as a loop over a stack of levels (`DFS_LOOP`, now the default); that
is the gcc reference for Stage 4. Both forms visit the same nodes, so every
count below holds for both.

| File | What |
| :--- | :--- |
| `lookups.c` | expanded nodes, generated nodes and table lookups of the C search, mean and maximum over the 2,644 distance-11 states |
| `lookups_pdb3.txt` | its output for the default build (three PDBs) |
| `lookups_pdb2.txt` | its output for `PDB=2` (permutation and orientation PDBs only) |
| `mod3_gcc.txt` | the twist-sum loop of `rv32/search_rv32.c` as gcc compiles it, for both ways of writing mod 3 |

## lookups_pdb3.txt, lookups_pdb2.txt

From `ida/`, after `make` and `make PDB=2` (they link the generated tables):

```sh
cc -O2 -std=c99 -Wall -Wextra -Wpedantic -DCORNER_PDB=1 -I. stage3/lookups.c search.c tables.c -o stage3/lookups
cc -O2 -std=c99 -Wall -Wextra -Wpedantic -DCORNER_PDB=0 -I. stage3/lookups.c search.c tables2.c -o stage3/lookups2
./stage3/lookups > stage3/lookups_pdb3.txt
./stage3/lookups2 > stage3/lookups_pdb2.txt
```

Every generated node costs the same table reads (`search.c` `step()` and
`node_h()`): 9 with three PDBs (`perm_q`, `ori_q`, `c4tw_q`, `add81`,
`c4pos_q`, `perm_h`, `ori_h`, `c4_row`, `c4_h`) and 4 with two (`perm_q`,
`ori_q`, `perm_h`, `ori_h`). So lookups = generated nodes x 9 (or x 4); the
program counts generated nodes with the search's own counter and multiplies.

## mod3_gcc.txt

The only mod 3 on the target is the input check in `parse_state`: the
search itself updates twists by table (`ori_q`, `add81`). Writing it without
a branch in C does not change the program. Replacing

```c
if (sum >= 3)
    sum -= 3;
```

with `sum -= 3 & -(unsigned) (sum >= 3);` and rebuilding gives a
byte-for-byte identical disassembly: gcc -O2 turns both into `bgeu` over one
`addi`. The branchless form (`sltiu`, `addi`, `andi`, `sub`) can only be
compared with the branch form in assembly (Stage 4).

To repeat: make the replacement, run `make -B search_rv32.elf` in `rv32/`
for each form, and diff `riscv-none-elf-objdump -d search_rv32.elf`.
