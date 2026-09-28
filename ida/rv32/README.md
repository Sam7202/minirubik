# rv32 — 在 Ripes 上跑搜尋，用 `--iret` 實測

```
host                               Ripes（target）                      host
../gen_tables → ../tables.c ─────▶ search_rv32.bin ─── ripes_out.txt ──▶ check_ripes
（build.c 建表）   唯讀資料連結進去     ../search.c 原封不動跑 IDA*               重播＋BFS 比最短
```

- 表格在 host 產生（上層 `make tables`），以 const 資料放在 `.rodata`，target **不建表、不複製**，直接讀。
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
make run ARCH=rv32im ISAEXTS=M               # 開 M 擴充
make run PROC=RV32_5S                        # pipeline 週期數（慢很多）
make size                                    # 各 section 大小
cd baseline && make run                      # 原版 solver.c 當比較基準（約 3 分 40 秒）
```

預設測資是 `search_rv32.c` 裡的 `cases[]`（6 組，含 PDB=3 的最壞狀態 `51342763312223`）。

## 實測（rv32i，RV32_ISS，`--iret`，單一測資）

| 測資 | v2 PDB=3 | 原版 solver.c（baseline/） |
| :--- | ---: | ---: |
| 12345671111111（已解） | 969 | 1,860,405,300 |
| 21345671111111 | 2,486,132 | 1,860,420,093 |
| 54721631111111 | 7,439,400 | ≈ 1.86 G |
| 51342763312223（最壞） | 8,067,745 | ≈ 1.86 G |

- 原版每次都重建全部 3,674,160 個狀態的 BFS 表，所以 iret 幾乎和輸入無關。
- 數字包含開機、解析輸入、印出結果；已解狀態那列 969 大約就是這部分的固定開銷。
- 6 組一起跑（`make verify`）：PDB=3 共 25,579,949；PDB=2 共 80,641,176。

## 檔案

| 檔案 | 說明 |
| :--- | :--- |
| `search_rv32.c` | target 單一編譯單元：freestanding runtime + `../search.c` + `../tables.c` + 測資 |
| `check_ripes.c` | host 驗證器，讀 Ripes 輸出；連結 host 版 `../search.c` 當節點數參考 |
| `link.ld` | 給 Ripes 用的平坦佈局，`_start` 必須是 image 第一個 byte |
| `baseline/` | 原版 minirubik `solver.c` 一行不改搬上 Ripes，見其 README |

## Ripes 的三個坑

1. **CLI 的 `-t c` 是壞的**：不會呼叫 compiler，安靜地跑出 0 cycles。C 要自己先編好。
2. **`-t bin` 吃 raw binary**：整包載到位址 0、從 0 開始執行，不看 ELF header。所以要
   `objcopy -O binary`，而且 `_start` 要排在最前面（`link.ld` 的 `.text.init`）。
3. **xPack newlib 走 semihosting，Ripes 只認 Linux 風格 ecall**：所以用 `-nostdlib`，自己寫
   `_start`、`sys_write`（a7=64）、`exit`（a7=93），`memset`/`memcpy` 也自己補。

另外，`RV32_ISS` 只算指令（CPI 固定 1），比 `RV32_5S` 快約兩個數量級；要 pipeline 數據才用 `RV32_5S`。

## 與 rv32i-measure/ 的分工

`../rv32i-measure/` 用 clang＋ld.lld 和自製模擬器 `emu.c`，一次量全部 2,644 個 distance-11 狀態、並確認
`tables.s` 和 `tables.c` 內容相同；需要支援 RISC-V 的 clang 和 ld.lld（Apple clang 沒有）。
作業要求的 Ripes `--iret` 實測用這個目錄。
