# 工作分類標籤

2026-11-05 建立。用於 issue／PR 的分流：邊條線、邊個做、做咗未。

| 標籤 | 意思 | 典型內容 |
|---|---|---|
| `ai-chatbot` | 小智語音／CHAT 模式／ASR-TTS 路徑 | `stc_voice`、模式閘、台詞、文字注入驗證 |
| `ai-model` | 模型訓練、量化、評估、推論 | 資料集、confusion matrix、ONNX／espdl、延遲 |
| `hardware` | bring-up、感測器、舵機、電源、接線 | ToF、PCA9685、GPIO 預算、S31 外設自測 |
| `ui` | LVGL 畫面、狀態機顯示 | 四屏、字級、模式指示 |
| `docs` | 計劃、審核、報告文件 | PROJECT-PLAN、方案審核、報告素材 |
| `for-codex` | 交由 Codex 執行的 issue | 有明確驗收表、唔需要真人硬件判斷的部分 |
| `done` | 已完成／已被取代 | 關閉前先加，方便日後篩選歷史 |

## 用法

- 每個 issue **至少一個**領域標籤（`ai-chatbot`／`ai-model`／`hardware`／`ui`／`docs`）
- 交給 Codex 的加 `for-codex`；Codex 交回 PR 後由我們移除
- 結案時加 `done`（唔好只靠 state，因為 `not planned` 也可以是「已取代」）
- 一個 issue 可以跨領域（例如模型在裝置上跑 = `ai-model` ＋ `hardware`）

## 現行對應

| Issue | 標籤 |
|---|---|
| #26 TOF bring-up | `hardware` `for-codex` |
| #25 `stc_voice` | `ai-chatbot` `for-codex` |
| #24 B2 文字注入 | `ai-chatbot` `done` |
| #23 B2 第一輪 | `ai-chatbot` `done` |
| #22 手部架構審核 | `hardware` `docs` |
| #12 模型驗證 | `ai-model` |
| #11 S31 外設自測 | `hardware` |
| #6 S3 tflite-micro 探測 | `ai-model` `hardware` |
| #4 UI 後續 | `ui` |
| #2 移植到 S31 | `hardware` |
