# baseline — 原版 minirubik solver.c 在 Ripes 上跑

`--iret` 的比較基準。`baseline_rv32.c` 直接 `#include "../../../solver.c"`（fork 根目錄的原版）,
**solver.c 一行都沒改**;`main` 改名成 `solver_main`,輸入用 `-DCASE` 寫死。
`include/` 是極簡的 stdio/stdlib/string 標頭,malloc 是 bump allocator(Ripes 記憶體是
sparse 的,放得下 3.5 MB 表 + 14 MB queue)。

```sh
export PATH="$HOME/Library/xPacks/riscv-none-elf-gcc/current/bin:$PATH"
make run                          # rv32i,README 的範例,RV32_ISS 約 3 分 40 秒
make run CASE=54721631111111
make run ARCH=rv32im ISAEXTS=M
```

注意:solver.c 的 `build_table` 在 stack 上放了約 34 KB 的 move table,所以 `link.ld`
的 stack 開到 256 KB(上層 rv32/link.ld 的 16 KB 會溢位、把程式碼蓋掉,跑不完)。

每次執行都會重建全部 3,674,160 個狀態的 BFS 表,所以 iret 幾乎和輸入無關:
rv32i 約 1.860 G,rv32im 約 0.734 G。
