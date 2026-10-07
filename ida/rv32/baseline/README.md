# baseline: the original solver.c on Ripes

The `--iret` reference. `baseline_rv32.c` includes the fork's
`../../../solver.c` without changing a line; only `main` is renamed
`solver_main`. Two things need a harness:

1. **Input.** Ripes has no command line, so `-DCASE` fixes the input and the
   harness builds `argv`.
2. **Memory.** newlib's `malloc` cannot get the 3.5 MB table and the 14 MB
   queue on Ripes (`build_table` returns NULL), so `malloc` is a bump
   allocator over Ripes' sparse memory.

The minimal headers in `include/` only keep newlib out, so that `--iret` is
almost all solver.c.

```sh
export PATH="$HOME/Library/xPacks/riscv-none-elf-gcc/current/bin:$PATH"
make run                          # rv32i, the README's example, 4 to 5 minutes on RV32_ISS
make run CASE=54721631111111
make run ARCH=rv32im ISAEXTS=M
```

`build_table` keeps about 34 KB of move tables on the stack, so this
`link.ld` gives the stack 256 KB; the 16 KB of `../link.ld` overflows into
the code. Every run rebuilds the whole table, so `--iret` hardly depends on
the input: about 1.860 G with rv32i, 0.734 G with rv32im.
