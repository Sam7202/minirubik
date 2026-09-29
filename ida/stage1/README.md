# stage1 — baseline measurements

Raw data for note sections 2.2 and 2.3. Host: Apple M1, 8 GB RAM, macOS 15.0;
Ripes v2.2.6-106-g5b8a616 (see ../ENVIRONMENT.md).

| File | What |
| :--- | :--- |
| `write_sb.s` | stores `0x5A` to N consecutive bytes from `0x10000000` with `sb`; `--iret` = 4N + 7 |
| `write_sw.s` | the same N bytes with `sw` (N/4 stores of the word `0x0000005A`); `--iret` = N + 7 |
| `rss_results.txt` | both programs, N = 0, 1 MiB, 4 MiB, 16 MiB, three runs each, `RV32_ISS` |
| `baseline_run.txt` | `../rv32/baseline` (original solver.c, input `21345671111111`) on `RV32_ISS` under `/usr/bin/time -l` |

## rss_results.txt

Produced by, for each program and N (N set by editing `.equ N`):

```sh
/usr/bin/time -l Ripes --mode cli --src _w.s -t asm --proc RV32_ISS --iret 2>&1 \
  | awk '/instructions retired/ {getline; printf "iret=%s  ", $1}
         /maximum resident/     {printf "rss=%s  ", $1}
         /peak memory footprint/{print "footprint=" $1}'
```

Columns: the first `iret=` is Ripes' guest `--iret`. The second `iret=` is
**not** a guest count: the awk pattern also matched the `instructions
retired` line of `time -l`, which is the host (M1) instruction count of the
whole Ripes process. `rss` is `maximum resident set size` and `footprint` is
`peak memory footprint`, both in host bytes.

## baseline_run.txt

```sh
/usr/bin/time -l Ripes --mode cli --src baseline_rv32.bin -t bin --proc RV32_ISS \
  --iret --exectime > baseline_run.txt 2>&1
```

The first byte of the file is a NUL written by the program's output, which
is why `grep` needs `-a` on it.
