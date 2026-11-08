# contracts — 模組介面契約（唯一真相）

四個 seam 的 C 標頭 ＋ 模型 I/O 契約。韌體、stub、訓練腳本都以此為準。

| 檔案 | 內容 | 誰用 |
|---|---|---|
| `stc_types.h` | 共用 enum／struct／錯誤碼 | 全部 |
| `stc_vision.h` | 影像 → 標籤 | 韌體、模型整合 |
| `stc_voice.h` | 語音播報與對話（AI chat 主體） | 韌體 |
| `stc_bin.h` | 舵機／ToF／ESP-NOW 手部 | 韌體 |
| `stc_game.h` | 遊戲核心（唯一可改分數者） | 韌體 |
| `model/` | 模型輸入輸出契約、類別順序、預處理 | 訓練（W Ed）、韌體 |

## 鐵律

1. **類別順序凍結**：`0` 可回收物、`1` 廚餘、`2` 有害垃圾、`3` 其他垃圾。UI、ToF 桶位、模型輸出全部用同一順序。
2. **只有 `stc_game.h` 的實作可以改分數與回合**；其他模組只出事件。
3. **每個 seam 必須有 stub**，另一邊未完成時仍可跑完整流程。
4. 改契約要升 `contract_version`，並同時改 `model/preprocess.json` 與韌體檢查；唔可以只改一邊。

## 給訓練（W Ed）的一句話

模型輸入、預處理、類別順序、輸出格式全部由 `model/preprocess.json` ＋ `model/classes.json` 定義；訓練完成後把 `model.onnx` 與 `eval.json` 放回同一位置，韌體只認這份契約。
