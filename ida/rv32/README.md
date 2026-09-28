# rv32 — 在 Ripes 上建表並搜尋

`search_rv32.c` 直接 `#include "../ida.c"`，開機先呼叫 `ida_init()` 在 target 上建好
move table 和兩張 PDB，再對 `cases[]` 裡的每個狀態跑 IDA*。

## 用法

```sh
export PATH="$HOME/Library/xPacks/riscv-none-elf-gcc/current/bin:$PATH"
make run                              # rv32i，RV32_ISS（預設，最快）
make run PROC=RV32_5S                 # 要看 pipeline 週期數就換這個
make run ARCH=rv32im ISAEXTS=M        # 開 M 擴充
make size-ida                         # 只編 ida.c，看 .text / .bss
```

需要 **continuous build**（`v2.2.6-106-g5b8a616` 以上），v2.2.6 正式版沒有 `RV32_ISS`。

要換測資就改 `search_rv32.c` 裡的 `cases[]`，或 `-DCASES='"...","..."'`。

## Ripes 的三個坑

1. **CLI 的 `-t c` 是壞的。** 不會去呼叫設定裡的 compiler，安靜地跑出 0 cycles、也不報錯。
   C 要自己先編好。
2. **`-t bin` 吃的是 raw binary，不是 ELF。** 整包載到位址 0、從 0 開始執行，不看 ELF header。
   所以要 `objcopy -O binary`，而且 `_start` 必須是 image 的第一個 byte
   （`link.ld` 用 `*(.text.init)` 排在最前面）。
3. **xPack 的 newlib 走 semihosting，Ripes 只認 Linux 風格的 ecall。** 所以改成 `-nostdlib`
   自己寫 `_start`、`sys_write`（`a7=64`）、`exit`（`a7=93`），`memset`/`memcpy` 也自己補。
