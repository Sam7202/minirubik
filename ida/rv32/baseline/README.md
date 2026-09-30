# baseline — 原版 minirubik solver.c 在 Ripes 上跑

`--iret` 的比較基準。`baseline_rv32.c` 直接 `#include "../../../solver.c"`（fork 根目錄的原版）,
**solver.c 一行都沒改**;`main` 改名成 `solver_main`。

外殼非做不可的只有兩件事(直接把 solver.c 用 newlib 編好丟進 Ripes 試過):

1. **輸入**:Ripes 沒有命令列參數,原封不動的 solver.c 只會印 usage、exit 2。這裡用
   `-DCASE` 把輸入寫死,再自己組 `argv` 呼叫 `solver_main`。
2. **記憶體**:newlib 的 `malloc` 透過 `brk` 要記憶體,在 Ripes 上要不到那兩塊
   (3.5 MB 表、14 MB queue),`build_table` 回傳 NULL,印出 `could not build complete
   state table`。所以 `malloc` 換成 bump allocator,從程式後面的空間一路切(Ripes 記憶體是
   sparse 的,放得下)。

`include/` 的極簡 stdio/stdlib/string 標頭和 `%s` 版 `printf` 不是必要的(newlib 的 `printf`
在 Ripes 上印得出來),只是讓 image 不連 newlib,量到的 `--iret` 幾乎全是 solver.c 本身。

```sh
export PATH="$HOME/Library/xPacks/riscv-none-elf-gcc/current/bin:$PATH"
make run                          # rv32i,README 的範例,RV32_ISS 約 4–5 分鐘
make run CASE=54721631111111
make run ARCH=rv32im ISAEXTS=M
```

注意:solver.c 的 `build_table` 在 stack 上放了約 34 KB 的 move table,所以 `link.ld`
的 stack 開到 256 KB(上層 rv32/link.ld 的 16 KB 會溢位、把程式碼蓋掉,跑不完)。

每次執行都會重建全部 3,674,160 個狀態的 BFS 表,所以 iret 幾乎和輸入無關:
rv32i 約 1.860 G,rv32im 約 0.734 G。
