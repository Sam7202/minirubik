# minirubik：IDA* + PDB（host 端 C 版本）

用兩張小 pattern database 當 heuristic，以 IDA* 求 2×2×2 的最短解（HTM）。
target 端只需要約 40 KB 的表；verify.c 在 host 上窮舉 3,674,160 個狀態驗證。

## 檔案

| 檔案 | 在哪裡跑 | 內容 |
| :--- | :--- | :--- |
| `cube.h` | 共用 | 魔方模型。`source`/`twist` 直接取自 solver.c，rank 編碼相同 |
| `ida.h`、`ida.c` | target（之後翻成 RV32I 的部分） | 座標 move table、PDB 建表（逐層掃描，不用 queue）、IDA* |
| `solve.c` | host | 命令列介面，輸入格式、錯誤碼與 solver.c 相同 |
| `verify.c` | 只在 host | oracle 與全部檢查，不上 target |

`CORNER_PDB=0`（預設）：h = max(排列 PDB, 方向 PDB)。
`CORNER_PDB=1`：再跟「4 顆角塊的位置與方向」PDB 取 max，是節點預算不夠時的備案。

## 建置與執行

```sh
make                       # solve、verify，以及加上 4 角塊 PDB 的 solve_c4、verify_c4
./solve 21345671111111     # 解法印到 stdout；長度、展開數、表格大小印到 stderr
make quick                 # 表格與 heuristic 檢查跑全部狀態，IDA* 每 97 個狀態抽 1 個，約 10 秒
./verify                   # IDA* 跑全部 3,674,160 個狀態，單核約 5 分鐘
./verify 0 1837080 & ./verify 1837080 3674160    # 分兩半平行跑
```

`verify` 有任何一項不過就回傳 1。

## verify 檢查的內容

1. **oracle**：用和 solver.c 相同的 unrank → quarter_turn → rank 做完整 BFS，不共用 target 的任何表。深度分布必須和 report.md 第 4 節完全一致。
2. **表格**：在每個狀態、每個面上，target 的 rank 都和 solver.c 相同，而且「先轉再投影」等於「先投影再轉」。後者就是 PDB 能當下界的條件。
3. **PDB**：抽象空間全部走到、最大值、深度分布。
4. **heuristic**：在全部狀態上 admissible（h ≤ 真實距離）、consistent（相鄰狀態的 h 差不超過 1），而且只有已解狀態的 h 是 0。另外也列出 h_perm + h_orient 會在幾個狀態上高估，說明為什麼只能取 max。
5. **IDA\***：每個解的長度都等於 oracle 距離，而且把解法放回參考模型重播後確實是已解狀態。另外列出各深度的展開數和產生數。

## 結果（全部 3,674,160 個狀態都跑過）

| | `CORNER_PDB=0` | `CORNER_PDB=1` |
| :--- | ---: | ---: |
| target 表格 | 40,383 B（39.4 KiB） | 122,544 B（119.7 KiB） |
| clang -O2 rv32i：.text + .bss | 4,032 + 40,398 B | 7,912 + 122,560 B |
| dfs 每層 stack frame（最多 11 層） | 80 B | 112 B |
| 平均展開 | 2,697 | 348 |
| 最壞展開 | 106,635 | 19,797 |
| 最壞產生 | 639,792 | 118,767 |
| 產生數超過 250,000 的狀態 | 428 | 0 |
| 最短解正確性 | 全部正確 | 全部正確 |

- **展開**：通過 bound 檢查、產生了子節點的節點。**產生**：每次呼叫 `node_quarter` 算一個。兩者都是所有 IDA* 迭代的累加。
- verify.c 的 oracle（3.5 MiB）和 BFS queue（14 MiB）只在 host 上用，不算進 target。
- .text 和 .bss 是只編譯 ida.c 的結果，不含 main 和 I/O。

## 注意

- IDA* 的解和 solver.c 的不一定是同一串。tests/solutions.txt 的 8 組裡有 3 組不同（長度相同，都是最短解）。如果 lab 是逐 byte 比對輸出，要先確認規格。
- `build_pdb` 用了 `memset`，翻成組語時要自己寫迴圈。
- 用 clang 把 C 編成 rv32i 時，init 階段那些「用加法代替乘法」的迴圈會被編譯器改回 `__mulsi3`/`__umodsi3` 呼叫。`CORNER_PDB=0` 的搜尋迴圈（dfs）裡沒有任何 helper 呼叫；`CORNER_PDB=1` 的 dfs 因為要算 `cp * 81` 而有 `__mulsi3`。
- 4 角塊 PDB 的最大值是 8，改用 4 bit 存可以把 `c4_h` 從 68,040 B 降到 34,020 B，總計約 86 KiB。
