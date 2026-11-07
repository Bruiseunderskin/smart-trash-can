# AGENTS.md — smart-trash-can

給任何在此 repo 工作的 agent（含 Codex、Claude Code、Hermes）。先讀十行，再動手。

## 0. 一句話

分類小隊：四個桶、一塊 4.3 吋屏、一個 int8 模型。**任何交付都要對得住這三樣實物**，不是對得住測試數字。

## 1. 硬件事實（不要自行假設）

| 項目 | 事實 |
|---|---|
| 部署板 | **ESP32-S31**（RISC-V 雙核 @320 MHz、PIE/QACC 128-bit SIMD、512 KB SRAM、DDR PSRAM 250 MHz） |
| 加速 | ESP-NN v1.3.0 起支援 S31，assembly 自動選用，與 ESP32-P4 共用 kernel |
| 四個桶 | 可回收 `GPIO9`、廚餘 `GPIO10`、有害 `GPIO11`、其他 `GPIO12` |
| 屏幕 | 4.3 吋 480×272 串口屏（TJC），另有大屏設計 800×480 LVGL |
| 舊資料 | 文件寫 ESP32-S3 的延遲數字屬當時量測，**不可換算成 S31** |

## 2. 資料：先查，再談生成

repo 內已有真實標註照片：

- `data/waste4_ei_train/`：4 類 × 300（food / hazardous / other / recyclable）
- `data/waste4_ei_test/`：4 類 × 200
- `data/garbage265/`：265 類中國垃圾資料集
- 合計約 8,000 個檔案

**硬規則：任何資料集或模型工作開始前，先 `ls data/` 並寫出類別方案與四個桶的對應。** 沒有真實資料就停手報告「blocked，需要 N 張 X 照片」，不要用程序生成的合成圖代替。

合成資料唯一的合法用途是驗證管線（上傳→特徵→訓練→量化→匯出→量測），不得以其準確率描述實物表現，而且要在同一句講明它是合成。

## 3. 模型事實

| 項目 | 值 |
|---|---|
| Edge Impulse 專案 | `Smart-Trash-Can`，ID `1157996` |
| 現行 impulse | `S31-160`：160×160 RGB、Transfer Learning、16 neurons、dropout 0.1 |
| int8 模型 | 1,005,632 bytes；`zero_point = -128`、`scale = 0.00390625` |
| 六類標籤（實驗性） | `battery, bottle, can, carton, food_waste, other` — **與四個桶不一致，屬早期實驗，勿直接上機** |
| 正確類別方案 | 四類：recyclable / food / hazardous / other，由四個桶固定 |

## 4. 驗收規則

1. **int8 才是交付物**。float32 與 int8 要分開報，並在同一句寫明是哪一個。
2. 每個數字都要帶來源（哪個模型、哪個測試集、幾張）。
3. 桌面預覽 `ctest` 過 ≠ 屏幕對。要拉 `ui/lvgl/preview/render/*.png` 親眼看過。
4. 延遲只認實機量測（p50／p95）。估算可以寫，但要標明是估算，且不得寫進交付結論。
5. 沒有檔案的「完成」不算完成：匯出模型要有 `.tflite` 與 `labels.txt`，量測要有 log。

## 5. 文件

`docs/INDEX.md` 是唯一登記處。新增文件要加一行（現行／已被取代／歷史）；被取代的不要刪，改狀態。

## 6. 慣例

- 語言：繁體中文；程式與 commit message 用英文。
- 角色一律叫**綠豆**，「小智」永久禁用。
- 主按鈕全寬貼底；不要放右下角落按鈕（小朋友手肘位）。
- 文件與 commit 不要寫「已完成」而實際未驗；寫「已完成並附證據」或「未完成，卡在 X」。
