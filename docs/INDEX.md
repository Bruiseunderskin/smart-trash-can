# 文件索引 · smart-trash-can

開 repo 第一件事睇這張表。每份文件都要在此登記：它是什麼、幾時核實、係現行還是已被取代。
新增文件時同時加一行；被取代的文件不要刪，改狀態欄，保留歷史。

核實日期：2026-11-07

## 1. 現行文件

| 文件 | 內容 | 幾時核實 | 備註 |
|---|---|---|---|
| `README.md` | 專案總覽、目錄、接線、編譯 | 2026-11-07 | 入口，指向本索引 |
| `ui/lvgl/README.md` | LVGL UI：800×480、九狀態、四類圖鑑、SDL 預覽 | 2026-11-07 | UI 的單一真實來源 |
| `ui/lvgl/preview/render/VERIFICATION.md` | 桌面預覽驗收記錄 | 2026-11-01 | 只證明桌面，不證明板載顯示與觸控 |
| `design/issue-3.md` | UI 設計規格（PRD，現行版） | 2026-11-07 | 取代 `issue-3-original.md` |
| `docs/model-comparison.md` | Issue #6–#8 模型與部署比較 | 2026-11-07 | 標題寫「部分完成」，實機量測仍未齊 |
| `docs/issue6/report.md` | esp-tflite-micro int8 延遲、ESP-NN A/B | 2026-11-07 | 數字來自 ESP32-S3，不是 S31 |
| `docs/issue8/report.md` | YOLO11n-cls → ONNX → esp-ppq int8 → esp-dl | 2026-11-07 | 與 Edge Impulse 路線並行的替代方案 |
| `deliverables/environmental_qa_upload_pack/README.md` | 環保問答內容包（10 個檔案，md／docx／pptx／xlsx／epub／pdf） | 2026-11-07 | 對外教材，與模型無關 |

## 2. 已被取代（保留作歷史）

| 文件 | 被誰取代 | 原因 |
|---|---|---|
| `design/issue-3-original.md` | `design/issue-3.md` | 舊版 PRD，狀態與交付範圍已改 |
| `docs/ei-baseline/model-testing.md` | 本索引第 3 節 | 描述舊 Edge Impulse 專案 `1154067`（Waste-Rec），現行專案是 `1157996`（Smart-Trash-Can） |

## 3. 現行模型事實（取代舊 baseline 文件）

| 項目 | 值 |
|---|---|
| Edge Impulse 專案 | `Smart-Trash-Can`，ID `1157996` |
| Impulse | `S31-160`，輸入 160×160 RGB |
| 類別 | `battery, bottle, can, carton, food_waste, other`（順序固定） |
| int8 模型 | 1,005,632 bytes；`zero_point = -128`、`scale = 0.00390625` |
| 部署目標 | ESP32-S31（RISC-V @320 MHz，PIE/QACC SIMD，ESP-NN 已支援） |
| 已量測延遲 | 未量。舊數字 858 ms 來自 ESP32-S3 @160 MHz，不可當 S31 結果 |
| 訓練資料 | 目前為程序化合成圖，**只證明管線通，不代表實物準確率** |

## 4. 未納入版本控制的目錄

下列內容目前是 untracked，`git status` 會看到但不在 repo 內：

| 路徑 | 是什麼 | 建議 |
|---|---|---|
| `docs/`、`deliverables/`、`poster.pdf` | 文件與教材 | 應入 git（文件要有歷史） |
| `esp32s3/` | 板載專案與量測工具 | 應入 git |
| `libraries/TjcHmi/`、`tools/tjc_mock/` | 串口屏庫（已擱置） | 入 git 或另開 repo，不要留在 untracked |
| `output/` | 產出物 | 加 `.gitignore` |
| `tools/*.py` | 資料集、量化、parity 腳本 | 應入 git |

## 5. 未關閉 issue 對應文件

| Issue | 題目 | 相關文件 |
|---|---|---|
| #45 | mode-gate 轉移表更新（CHAT 由圖鑑進入、閒置 90 s） | `design/issue-3.md`、`ui/lvgl/README.md` |
| #31 | 遊戲邏輯完整實作（stub ＋ 佔位 UI） | `design/issue-3.md` |
| #26 | TOF050C × 4 + TCA9548A bring-up | 未有任何文件 |
| #25 | AI chat 現階段實作（stc_voice） | `design/issue-3.md` |
