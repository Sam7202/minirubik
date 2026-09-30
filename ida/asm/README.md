# asm — 手寫 RV32I 組語版的 IDA*

`solve.s` 是 Stage 4 的組語版搜尋。演算法、座標、表格和搜尋順序都跟 `../search.c` 相同（以 `../tables.h` 為準），所以解法字串和展開數、產生數會跟 C 版逐一相同，可以直接用 `../rv32/check_ripes` 在 host 上驗證。

## 和 C 版不同的地方

- **沒有遞迴**：深度優先搜尋是一個迴圈，搭配固定大小的層級陣列 `slots`（11 層，每層 16 bytes）。只有往下走進子節點時才寫入陣列；試同一個節點的子節點時，座標、這個 face 的四個表格列、剪枝上限都留在暫存器裡。
- **剪枝時提早結束**：上限 `limit = bound - g - 1` 每層只算一次；先查 4 角塊 PDB，任何一張超過上限就剪掉，不算 max。
- **沒有乘除法**：rank 用 Horner 法，×2 到 ×6 都用 shift 加 add（例如 ×6 = (x << 1) + (x << 2)、×5 = (x << 2) + x）。印數字用 Ripes 的 print-integer 呼叫，不用自己除以 10。`-march=rv32i` 組譯，原始碼裡如果出現 `mul`、`div`、`rem` 會直接組譯失敗。
- **程式內自我驗證**：每組測資解完之後，用 `cube.h` 的直接模型（`source`、`twist`）重播解法，確認回到解好的狀態（T5），並比對步數和 `cases.s` 裡的期望值（T6）。exit code 是失敗的測資數。

## 用法

```sh
export PATH="$HOME/Library/xPacks/riscv-none-elf-gcc/current/bin:$PATH"
make run
make verify
make run CASE=21345671111111 EXPECT=11
make run MOD3=branchless
make run PROC=RV32_5S
make size
```

- `make run`：跑 `cases.s` 的 10 組測資（已解好、3 步、`tests/solutions.txt` 的 7 組、最壞的 distance-11 狀態）。
- `make verify`：跑完後用 `../rv32/check_ripes` 在 host 上檢查每一組：重播、BFS 最短距離、解法和節點數跟 host 的 `search.c` 相同。
- `CASE`、`EXPECT`：只跑一組，用來量單一狀態的 `--iret`。`EXPECT` 是期望步數，不知道就省略（255）。
- `MOD3=branchless`：mod 3 改用不含分支的寫法，見下面的量測。

## 實測（RV32_ISS，`--iret`，單一測資）

| 測資 | 組語 | gcc -O2（`../rv32`，`stage3-c`） | 組語 / gcc |
| :--- | ---: | ---: | ---: |
| `12345671111111`（已解好） | 769 | 1,750 | 0.44 |
| `21345671111111` | 1,321,643 | 2,485,721 | 0.53 |
| `54721631111111` | 3,954,531 | 7,438,599 | 0.53 |
| `51342763312223`（最壞） | 4,296,273 | 8,066,912 | 0.53 |
| `.text` bytes | 1,948 | 2,824 | 0.69 |

- `.rodata` + `.bss` = 127,076 + 248 = 127,324 bytes，在 131,072 以內。
- 10 組一起跑（`make run`）共 5,997,516 條。
- 比較時要注意：組語版多做了自我驗證（重播解法），gcc 版沒有；gcc 版印數字用減法迴圈，組語版用 Ripes 的 print-integer 呼叫。搜尋本身的差距才是主要來源，詳見 note。

## 全部 2,644 個 distance-11 狀態（`../stage4/`）

每個狀態在 RV32_ISS 上各跑一次，8 個同時跑，組語版共 2 分 45 秒：

| | 組語 | gcc -O2（`stage3-c`） |
| :--- | ---: | ---: |
| 失敗 / 超過 5 × 10⁷ 條 | 0 / 0 | 0 / 0 |
| 平均 `--iret` | 1,084,723.0 | 2,040,094.5 |
| 最大（`51342763312223`） | 4,296,273 | 8,066,912 |
| 最小（`34165273222322`） | 519,419 | 977,198 |

每一個狀態都通過程式內的自我驗證（重播回到解好、步數 11）。`../rv32/check_ripes` 在 host 上再逐一檢查：2,644 個解法和節點數都跟 host 的 `search.c` 相同。

## mod 3 的兩種寫法

只有 `parse`（每組 7 次）和 `replay`（每轉一次 7 次）用到 mod 3，搜尋本身靠 `ori_q`、`add81` 查表。

| 測資 | ISS 指令：分支 / branchless | 5S cycles：分支 / branchless |
| :--- | ---: | ---: |
| `12345671111111`（0 步） | 769 / 790 | 1,095 / 1,102 |
| `35621472211121`（3 步） | 2,353 / 2,549 | 3,145 / 3,229 |
| `62345713133111`（8 步） | 13,080 / 13,407 | 16,237 / 16,386 |

分支版（`bltu` 跳過一條 `addi`）是 1–2 條，branchless 版（`sltiu`、`addi`、`andi`、`sub`）固定 4 條。在 5 級 pipeline 上，跳躍成立的分支要多付 cycle，差距縮小到大約一半，但分支版仍然比較快，所以預設用分支版。另外，同樣的 branchless 寫法在 C 裡會被 gcc 轉回分支（見 `../stage3/mod3_gcc.txt`）。

5S 的 `--iret` 比 ISS 少 1：最後那條 exit ecall 在 5S 上沒有計入。

## 建置上的兩個坑

1. **連結時要加 `--no-relax`**。GNU ld 預設會做 relaxation：位址放得進 12 bits 的 `la` 會縮成一條指令，`.bss` 附近的符號會改成相對 gp 的定址。結果是指令數會隨符號的位置改變：branchless 版多了 32 bytes 程式碼，把 `face_rows` 推過 2,048，兩個 build 就差了 18,693 條，而那跟 mod 3 無關。另外這個程式不設定 gp，gp 相對定址本來就不該出現。
2. **Ripes 的印字串呼叫（a7 = 4）會把結尾的 NUL 也印出來**，`check_ripes` 用 C 字串函式解析，碰到 NUL 就斷掉。所以字串都用 write（a7 = 64）加上長度印。Ripes 內建的組譯器也不能用：它不支援 `.if`、`.macro`、`.include`，`.equ` 的運算順序也不對（`3*4+1` 算成 15）。所以這裡用 GNU as 組譯，再用 `-t bin` 載入。

## 檔案

| 檔案 | 說明 |
| :--- | :--- |
| `solve.s` | 組語程式：輸入檢查、起點座標、IDA*、輸出、自我驗證 |
| `cases.s` | 測資：14 個字元、期望步數、補齊用的 1 byte；0 表示結束 |
| `Makefile` | GNU as + `../rv32/link.ld` + `objcopy`，Ripes `-t bin` |
