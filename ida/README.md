# minirubik：IDA* + PDB（表在 host 產生，Ripes 只跑搜尋）

依照作業的 precomputation rule：transition table 和 heuristic table 在 host 上用 C 產生，以唯讀資料連結進程式；搜尋本身在 target 上執行。

```
host（你的電腦）                                          target（Ripes）
build.c + gen_tables.c ──make tables──▶ tables.s ───────▶ asm/：手寫組語 IDA*（GNU as 連結）
                                        tables_ripes.s    同一份資料，給 Ripes 內建組譯器
                                        tables.c ──▶ solve / verify / rv32/（gcc 對照組）
```

## 檔案

| 檔案 | 在哪裡跑 | 內容 |
| :--- | :--- | :--- |
| `tables.h` | 共用 | **資料約定**：每張表的形狀、座標定義、索引算法、搜尋順序。組語必須照這裡做 |
| `cube.h` | 共用 | 魔方模型；`source`/`twist` 取自 solver.c，rank 編碼相同 |
| `build.c`、`build.h` | 只在 host | 建出所有表（逐層掃描的 BFS） |
| `gen_tables.c` | 只在 host | 產生器：呼叫 build.c，寫出 `tables.c`、`tables.s`、`tables_ripes.s` |
| `search.c`、`search.h` | target | IDA* 搜尋，只讀表、不建表；組語版的對照。深度優先搜尋預設寫成迴圈，`DFS=recursive` 換回 tag `stage3-c` 的遞迴版 |
| `solve.c` | host | C 版搜尋的命令列介面，輸入格式與 solver.c 相同 |
| `verify.c` | 只在 host | 窮舉驗證表和搜尋 |
| `rv32/` | host＋Ripes | **Ripes 實測**：search.c＋tables.c 編成 RV32 在 Ripes 跑 `--iret`，再回 host 驗證（`make verify`）；`baseline/` 是原版 solver.c |
| `asm/` | host＋Ripes | **Stage 4 手寫 RV32I 組語版**：和 search.c 同一個搜尋，在 Ripes 上跑並自我驗證，見 `asm/README.md` |
| `stage1/`…`stage4/` | host＋Ripes | note 各節的原始數據和產生它們的程式；`stage4/` 把全部 2,644 個 distance-11 狀態在 Ripes 上各跑一次 |

`tables.c`、`tables.s`、`tables_ripes.s` 由 `make` 產生，不要手改。

## 建置與執行

```sh
make                       # 產生三個表格檔，編出 gen_tables、solve、verify
make tables                # 只重新產生表格檔
./solve 21345671111111     # 解法印到 stdout；長度、展開數、產生數印到 stderr
make quick                 # 全部狀態的表格檢查，IDA* 每 97 個抽 1 個，約 2 秒
make check                 # 全部 3,674,160 個狀態，約 40 秒（PDB=2 約 3 分鐘）
make PDB=2                 # 只用兩張 PDB 的版本，檔名多一個 2（tables2.s、verify2…）
make check DFS=recursive   # 改用遞迴版 dfs()（tag stage3-c），執行檔多 _rec（verify_rec…）
make -C rv32 verify        # Ripes 實測 --iret，並回 host 驗證解法（見 rv32/README.md）
make -C asm verify         # 組語版：Ripes 實測並回 host 驗證（見 asm/README.md）
make -C asm verify PROC=RV32_5S   # 同上，改在 5 級 pipeline 上跑（T7）
cc -O2 -std=c99 -Wall -Wextra -Wpedantic -DCORNER_PDB=1 -I. stage4/d11.c tables.c -o stage4/d11
stage4/d11 > stage4/d11.txt           # batch.py 的輸入：2,644 個 distance-11 狀態
stage4/batch.py asm > stage4/asm.csv  # 每個狀態在 Ripes 上各跑一次（見 stage4/README.md）
```

## 表格檔

- `tables.s`：放在 `.section .rodata`，給 GNU as／LLVM 組譯成 ELF 再載入 Ripes。
- `tables_ripes.s`：同樣的資料放在 `.data`。Ripes 內建的組譯器只認得 `.text`、`.data`、`.bss`（見 Ripes 原始碼 `src/assembler/gnudirectives.cpp`）。128 KiB 預算把 .rodata 和 .data 一起算，放哪一段都一樣。程式只讀不寫。
- 排列順序是 halfword 表 → byte 表 → `c4_row`。`c4_row` 的內容是 `.word c4_h+840` 這類位址；Ripes 只看得到前面已經定義過的標籤，所以 `c4_h` 必須排在它前面。在 Ripes 裡可以把整個檔案接在你的程式後面，指令裡往後引用標籤沒有問題。
- 開頭的 `.align 4` 在 Ripes 是 4 byte，在 GNU as 是 2⁴ = 16 byte；兩種都讓表從 word 邊界開始。
- `.equ` 定義了每張表一列的 byte 數（`PERM_Q_ROW`、`ADD81_ROW`…），組語可以直接用。
- 每張表的 FNV-1a checksum 寫在檔頭註解，`gen_tables` 執行時也會印出來。

索引算法以 `tables.h` 開頭的說明為準：p、o、cp、co 的定義，一次四分之一轉怎麼查表，h 怎麼算，以及搜尋順序（面 R、B、D；每面 90°、180°、270°）。

## 驗證內容

| 項目 | 在哪裡檢查 |
| :--- | :--- |
| 連結進來的表 = build.c 現在產生的結果（表格檔沒過期） | verify 第 0 步 |
| oracle：完整 BFS 的深度分布與 report.md 第 4 節一致 | verify 第 1 步 |
| 座標轉移與真正的魔方轉動一致（PDB 能當下界的前提） | verify 第 2 步 |
| H2：每張 PDB 都填滿、最大值、已解狀態那格是 0 | verify 第 3 步 |
| H1：h 在全部狀態 admissible、consistent | verify 第 4 步 |
| H3：C 版搜尋在全部狀態都找到最短解 | verify 第 5 步 |
| 解法在參考模型上重播後確實解開（host；清單的 T5 要在 Ripes 上另外做） | verify 第 5 步 |
| tables.s 與 tables.c 內容相同 | stage4：asm/ 連結 tables.s，全部 2,644 個 distance-11 狀態的解法和節點數，都和連結 tables.c 的 host search.c 相同（check_ripes） |
| T5：解法在 Ripes 上重播後確實解開 | asm/ 程式內自我驗證；stage4 全部 2,644 個 distance-11 狀態都通過 |
| T6：`21345671111111` 回傳 11 步最短解 | asm/：`make run CASE=21345671111111 EXPECT=11` |

所有表都是一格一 byte（或 halfword），沒有 nibble 打包，所以沒有 H4 要檢查的奇偶格存取。

## 給組語版對照的參考數字

`./solve` 的結果（三張 PDB）。組語版的搜尋順序跟 search.c 相同的話，步數、解法字串和節點數都應該一模一樣。索引錯一格時，步數常常還是對的，但節點數通常會變，所以建議在 Ripes 上也數展開數來比對。

| 狀態 | 步數 | 展開 | 產生 | 解法 | 與 solver.c |
| :--- | ---: | ---: | ---: | :--- | :--- |
| 12345671111111 | 0 | 0 | 0 | （空） | 相同 |
| 62345713133111 | 8 | 49 | 283 | R2 D B2 R' D R' D B' | 相同 |
| 24316572122213 | 8 | 27 | 148 | R' D' R2 B D' B2 D' R2 | 相同 |
| 25713642221111 | 8 | 30 | 159 | R B2 R' D2 B R B2 R' | 相同 |
| 24513763133333 | 9 | 369 | 2,197 | B' R2 B' R B2 D' R' D2 R' | 相同 |
| 43752611332133 | 9 | 63 | 360 | R D2 R2 D R D2 B D' R2 | 不同，長度相同 |
| 25416373331111 | 10 | 1,136 | 6,794 | R2 D R B2 R D' R D R' B2 | 不同，長度相同 |
| 21345671111111 | 11 | 6,095 | 36,543 | R B' D2 R' B R' B' R D2 R B | 不同，長度相同 |
| 51342763312223 | 11 | 19,797 | 118,767 | R' B R' D R B2 D' R D R2 D' | 最壞情況，不在測資裡 |

前 8 個是 minirubik 的 `tests/solutions.txt`。如果 lab 是逐 byte 比對 solver.c 的輸出，要先確認規格，因為最短解不只一種。

## 結果

節點數和查表次數是 host 上的結果：d=11 的平均和最壞見 `stage3/`，超過預算的狀態數是 `make check` 跑全部狀態的結果（`stage2/`）。指令數是 Ripes `RV32_ISS` 的 `--iret`，量的是 `rv32/`（gcc -O2 編譯 search.c 預設的迴圈版），全部 2,644 個 distance-11 狀態各跑一次（`stage4/`）。遞迴版（tag `stage3-c`）的數字見 `stage4/README.md`，組語版的數字見 `asm/README.md`。

| | PDB=3（預設） | PDB=2 |
| :--- | ---: | ---: |
| .rodata + .data + .bss（`rv32/` 的 image） | 127,512 B | 41,080 B |
| 展開：d=11 平均／最壞 | 4,994.7 / 19,797 | 34,438.9 / 106,635 |
| 查表次數：d=11 平均／最壞 | 269,554.8 / 1,068,903 | 826,472.8 / 2,559,168 |
| 展開數超過 250,000 的狀態（清單的節點預算） | 0 | 0 |
| RV32I 指令：最壞的一次執行 | 6,226,078（`51342763312223`） | 20,844,421（`54721631111111`） |
| RV32I 指令：d=11 平均每次查詢 | 1,575,885.6 | 6,740,242.4 |

- **展開**：通過 bound 檢查、產生了子節點的節點。**產生**：每一步轉動算出一個子節點算一次。兩者都是所有 IDA* 迭代的累加。每產生一個節點固定讀 9 張表（PDB=2 是 4 張），所以查表次數 = 產生數 × 9（× 4），見 `stage3/README.md`。
- 清單的節點預算看的是展開數。用產生數算的話，PDB=2 有 428 個狀態超過 250,000，PDB=3 沒有；實際指令數兩者都在 5 × 10⁷ 以內。
- 指令數包含 C 外殼的開機和印出結果（已解好的狀態約 1,968 條）。每個狀態的輸入在編譯時寫死，gcc 會在編譯時就做完輸入檢查（見 `stage4/README.md`）。
- 搜尋迴圈裡沒有乘法、除法或取餘數；`rv32/` 連結時不加 libgcc，出現就會連結失敗。改用 `__mulsi3` 算 `co × 81` 的代價見 note 第 4.2 節。

## 注意

- 節點數和搜尋順序有關：面依 R、B、D，每個面依 90°、180°、270°。
- `c4_row` 如果在你的組譯器上組不過（不支援「標籤＋偏移」），可以把它從表格檔拿掉，改在程式開頭用迴圈填 81 個位址（每次加 840）。
