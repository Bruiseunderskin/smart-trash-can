# contracts — 模組介面契約（唯一真相）

四個 seam 的 C 標頭 ＋ 模型 I/O 契約。韌體、stub、訓練腳本都以此為準。

| 檔案 | 內容 | 誰用 |
|---|---|---|
| `stc_types.h` | 共用 enum／struct／錯誤碼 | 全部 |
| `stc_vision.h` | 影像 → 標籤 | 韌體、模型整合 |
| `stc_voice.h` | 語音播報與對話（AI chat 主體） | 韌體 |
| `stc_bin.h` | 舵機／ToF／ESP-NOW 手部 | 韌體 |
| `stc_game.h` | 遊戲核心 v1.3.0（唯一可改分數者） | 韌體 |
| `stc_mode.h` | 模式閘：GAME／CHAT 的唯一切換者 | 韌體 |
| `stc_ui.h` | UI v2.2.0：回合準備、意圖 action、回合回饋 payload、GAME／CHAT shell | UI／控制器 |
| `model/` | 模型輸入輸出契約、類別順序、預處理 | 訓練（W Ed）、韌體 |

## 鐵律

1. **桶位 enum 凍結**：`0` 可回收物、`1` 廚餘、`2` 有害垃圾、`3` 其他垃圾。模型 order 另由 `model/classes.json` 凍結，現為 `food, hazardous, other, recyclable, unknown`，經 `{1,2,3,0,STC_BIN_COUNT}` 映射；`unknown` 不開桶並要求重掃。
2. **只有 `stc_game.h` 的實作可以改分數與回合**；其他模組只出事件。
3. **每個 seam 必須有 stub**，另一邊未完成時仍可跑完整流程。
4. 改契約要升 `contract_version`，並同時改 `model/preprocess.json` 與韌體檢查；唔可以只改一邊。

## 給訓練（W Ed）的一句話

模型輸入、預處理、類別順序、輸出格式全部由 `model/preprocess.json` ＋ `model/classes.json` 定義；訓練完成後把 `model.onnx` 與 `eval.json` 放回同一位置，韌體只認這份契約。

## 變更記錄

| 日期 | 版本 | 改動 |
|---|---|---|
| 2026-11-05 | v1.0.0 | `stc_mode.h` 初版：六個狀態、`SUMMARY`＋`SET_LONG` 進 CHAT、60 s 閒置退出 |
| 2026-11-08 | v1.1.0 | 新增 `STC_IN_CHAT_ENTER`：`GAME_IDLE`＋`IN_CHAT_ENTER`（圖鑑「問綠豆」）進 CHAT；`SUMMARY`＋`SET_LONG` 改為拒絕；`IN_CHAT_TIMEOUT` 維持 60 s（經討論後不改）；不變量 I1 改寫為「CHAT 唯一入口＝圖鑑」 |
| 2026-11-14 | v2.2.0 | UI 狀態追加 `STC_UI_CHAT`；不重編現有 state/action 值，用作 GAME／CHAT 外層 shell 的 CHAT 畫面。 |
| 2026-11-11 | game v1.3.0 | 真實 `now_ms` 啟動、`SCAN_REQUEST`／`CONTINUE`、round generation、總時限、按鍵學習頁與連續答對 |
| 2026-11-11 | UI v2.1.0 | 追加 `SCAN_READY`、`SCAN`／`NEXT` action、回合回饋與 replay/virtual debug flags |
| 2026-11-11 | vision v1.3.0 | 增加五幀聚合 top-two 欄位，供 controller／UI 診斷 |
| 2026-11-11 | hand v2 | 新增 16-byte ESP-NOW 手部訊息、ACK／DROP／STATE／FAULT 與重試契約 |
| 2026-11-15 | hand v3 | 新增 `OPEN_ALL_HOLD`；總結頁持續開蓋直到下一局 `START` 前 `CLOSE_ALL` |
| 2026-11-14 | model v3.0.0 | 新增第五類 `unknown`；輸出 `[1,5]`，index 4 → `STC_BIN_COUNT`、abstain true |
