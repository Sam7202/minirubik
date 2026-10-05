# rv32 — 在 Ripes 上跑搜尋，用 `--iret` 實測

```
host                               Ripes（target）                      host
../gen_tables → ../tables.c ─────▶ search_rv32.bin ─── ripes_out.txt ──▶ check_ripes
（build.c 建表）   唯讀資料連結進去     ../search.c 原封不動跑 IDA*               重播＋BFS 比最短
```

- 表格在 host 產生（上層 `make tables`），以 const 資料放在 `.rodata`，target **不建表、不複製**，直接讀。
- 連結時**不加 `-lgcc`**：RV32I 沒有乘除指令，程式裡只要出現 gcc 無法化成 shift/add 的乘、除、取餘數，
  就會變成呼叫 `__mulsi3`/`__udivsi3`…，連結直接失敗，不會偷偷混進去。
- `search_rv32.c` 直接 `#include "../search.c"` 和 `"../tables.c"`，在 Ripes 跑的就是上層那份搜尋碼。
- `check_ripes.c` 用 `cube.h` 的參考模型重播每個解，並跑完整 BFS 確認步數是最短（這兩項不用 search.c）；
  另外對同一個狀態跑 host 版 `search.c`，要求解法字串、展開數、產生數**逐一相同**（索引錯一格時步數常常還對，但節點數會變）。
  所以 target 程式的輸出必須是 `STATE -> moves  [len N, expanded E, generated G]` 格式。

## 用法

```sh
export PATH="$HOME/Library/xPacks/riscv-none-elf-gcc/current/bin:$PATH"
make verify                                  # 完整流程：建表 → Ripes 搜尋 → host 驗證
make run                                     # 只跑 Ripes，印 --iret / cycles / CPI
make run CASES='"21345671111111"'            # 單一測資，量單次查詢
make run PDB=2                               # 只用兩張 PDB（../tables2.c）
make run DFS=recursive                       # 遞迴版 dfs()（tag stage3-c）；預設是迴圈版
make run ARCH=rv32im ISAEXTS=M               # 開 M 擴充
make run PROC=RV32_5S                        # pipeline 週期數（慢很多）
make size                                    # 各 section 大小
cd baseline && make run                      # 原版 solver.c 當比較基準（約 4–5 分鐘）
```

預設測資是 `search_rv32.c` 裡的 `cases[]`（6 組，含 PDB=3 的最壞狀態 `51342763312223`）。

## 實測（rv32i，RV32_ISS，`--iret`，單一測資）

| 測資 | PDB=3，迴圈版（預設） | PDB=3，遞迴版（`DFS=recursive`） | 原版 solver.c（baseline/） |
| :--- | ---: | ---: | ---: |
| 12345671111111（已解） | 1,968 | 1,750 | 1,860,405,301 |
| 21345671111111 | 1,919,265 | 2,485,721 | 1,860,420,093 |
| 54721631111111 | 5,742,490 | 7,438,599 | ≈ 1.86 G |
| 51342763312223（最壞） | 6,226,078 | 8,066,912 | ≈ 1.86 G |

- 原版每次都重建全部 3,674,160 個狀態的 BFS 表，所以 iret 幾乎和輸入無關。
- 數字包含開機、解析輸入、印出結果；已解狀態那列大約就是這部分的固定開銷。迴圈版多出的約 220 條，
  幾乎都是 `_start` 清零 `.bss`：`levels[]` 讓 `.bss` 從 16 B 變成 236 B，每 4 bytes 要 4 條指令。
- 迴圈版和遞迴版的解法、展開數、產生數完全相同（host 上 `make check` 和 `make check DFS=recursive`
  在全部狀態上的輸出一樣），差的只有指令數：每個展開的節點省掉一次呼叫、stack frame 和返回，
  每個被剪掉的子節點也從約 48 條降到約 42 條（不用再把子節點座標存進 stack）。
- 遞迴版那一欄是 `4af0aac`（拿掉 image 裡所有乘除法）之後量的：印十進位數字改成不用除法，
  固定開銷從 969 變成 1,750；`node_from_state` 不再呼叫 `__mulsi3`，其他狀態反而少了幾百條。
- 6 組一起跑（`make verify`）：迴圈版 PDB=3 共 19,743,395、PDB=2 共 62,682,094；
  遞迴版是 25,576,391 和 80,635,541。

## 檔案

| 檔案 | 說明 |
| :--- | :--- |
| `search_rv32.c` | target 單一編譯單元：freestanding runtime + `../search.c` + `../tables.c` + 測資 |
| `check_ripes.c` | host 驗證器，讀 Ripes 輸出；連結 host 版 `../search.c` 當節點數參考 |
| `link.ld` | 給 Ripes 用的平坦佈局，`_start` 必須是 image 第一個 byte |
| `baseline/` | 原版 minirubik `solver.c` 一行不改搬上 Ripes，見其 README |

## 在 Ripes 上跑 C 要注意的事

1. **`-t c` 用的是 Ripes 設定裡的 compiler 和參數**：它會呼叫 Settings 裡的 `compiler_path`
   （這台是 xPack `riscv-none-elf-gcc`），參數是全域設定的 `compiler_args`（目前 `-O0 -g`），
   連 newlib 一起連結。每次跑沒辦法指定 `-O2`、自己的 linker script 或外殼，
   所以量測用的 image 都在 Makefile 裡自己編好，再用 `-t bin` 載入。
   （沒設定 compiler 時 `-t c` 會安靜地跑出 0 cycles。）
2. **沒有命令列參數**：程式拿不到 `argv`，輸入只能編進 image（這裡是 `cases[]`，
   baseline 是 `-DCASE`）。原封不動的 solver.c 在 Ripes 上只會印 usage、exit 2。
3. **大塊 `malloc` 會失敗**：newlib 的 `malloc` 透過 `brk` 要記憶體，在這個 Ripes build 上要不到
   solver.c 那兩塊（3.5 MB、14 MB），`build_table` 回傳 NULL。所以 baseline 自己寫 bump allocator；
   `search_rv32.c` 完全不用 heap。
4. **`-t bin` 吃 raw binary**：整包載到位址 0、從 0 開始執行，不看 ELF header。所以要
   `objcopy -O binary`，而且 `_start` 要排在最前面（`link.ld` 的 `.text.init`）。這個 Ripes build
   也有 `-t elf`，可以直接吃 ELF；Makefile 用 `.bin` 是因為兩種 build 都能跑。

`printf` 本身沒問題：xPack newlib 的系統呼叫是 Linux 風格的 ecall（write 是 a7=64），Ripes 有實作，
原封不動的 solver.c 印得出 usage。這裡仍然用 `-nostdlib`、自己寫 `_start`、`sys_write`、`exit`，
理由是量測：不連 newlib 和 libgcc，就不會有藏起來的乘除法（newlib 的 `printf` 會做除法），
輸出的程式碼也很小，`--iret` 幾乎全是搜尋本身。

另外，`RV32_ISS` 只算指令（CPI 固定 1），比 `RV32_5S` 快約 70 倍（note 2.3 的寫入迴圈實測）；要 pipeline 數據才用 `RV32_5S`。

## 與 asm/、stage4/ 的關係

這個目錄是 Stage 4 的 **gcc 對照組**：`../search.c` 預設的迴圈版，結構和組語版一樣沒有遞迴；
`DFS=recursive` 是 tag `stage3-c` 的遞迴版。`../asm/` 的手寫組語版借用這裡的 `link.ld`
和 `check_ripes`。`../stage4/batch.py` 把這裡的 C 版和組語版都在 Ripes 上跑完全部 2,644 個 distance-11
狀態；C 版每個狀態各編譯一次，因為寫死的輸入會被 gcc 在編譯時就檢查完（見 `../stage4/README.md`）。
