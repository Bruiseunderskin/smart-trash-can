# AGENTS.md — smart-trash-can

給任何在此 repo 工作的 agent（含 Codex、Claude Code、Hermes）。先讀這份，再按需要往下鑽。

**這份文件只放「事實」與「硬規則」。** 細節故意不塞進來，放在 `docs/agents/`，需要時才讀（漸進式披露，見第 8 節）。

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

**repo 內追蹤的資料**（`git ls-files data`）：

- `data/field-captures/`：少量實拍
- `data/field-probe/`：獨立測試資料，**不得混入訓練、校準或閾值選擇**
- `data/README.md`：資料規則與來源

**只存在於本機、被 `.gitignore` 排除**（不入 repo，但仍可用）：

- `data/waste4_ei_train/`：4 類 × 300（food / hazardous / other / recyclable）
- `data/waste4_ei_test/`：4 類 × 200
- `data/garbage265/`：265 類中國垃圾資料集

訓練資料由 `tools/build_dataset.py` 在本機建立。

**硬規則：任何資料集或模型工作開始前，先 `ls data/`、讀 `data/README.md`，並寫出類別方案與四個桶的對應。** 沒有真實資料就停手報告「blocked，需要 N 張 X 照片」，不要用程序生成的合成圖代替。

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

- **索引**：`docs/README.md`（現行文件的入口）
- 新增文件要在索引加一行；被取代的不要刪，改狀態
- 文件要寫「讀者」「何時讀」，唔好寫給自己睇

## 6. 慣例

- 語言：繁體中文；程式與 commit message 用英文。
- 角色一律叫**綠豆**，「小智」永久禁用。
- 主按鈕全寬貼底；不要放右下角落按鈕（小朋友手肘位）。
- 文件與 commit 不要寫「已完成」而實際未驗；寫「已完成並附證據」或「未完成，卡在 X」。

## 7. Commit 與 CHANGELOG（硬規則）

1. **可以直接 commit 到工作分支**，不需要每一步都開 PR。
2. **但每個 commit 必須同時更新 `CHANGELOG.md` 的 `[Unreleased]` 段落**，列明讀者見到什麼變化（Added / Changed / Fixed / Removed）。
3. 沒有 changelog 條目的 commit 視為**未完成**，不要 push。
4. commit message 與 changelog 條目要對得上：message 講範圍（`fix(ui): ...`），changelog 講影響（讀者會見到什麼）。
5. 一個 commit 一件事。混合改動要拆開，changelog 逐條對應。
6. **合併到 `main` 一定要經 PR，而且 CI 綠燈**。直接 commit 只准在工作分支。
7. 發版時把 `[Unreleased]` 的內容移到版本標題下，加日期。

> 細節與範例：`docs/agents/commits.md`

## 8. Skills 與漸進式披露

這份 AGENTS.md 故意保持短。**不要把所有細節塞進來**；需要時才讀對應文件。

| 你想做什麼 | 先讀 |
|---|---|
| 改 UI、跑預覽、regen render | `docs/agents/skills.md` → `smart-trash-can-ui` skill |
| 上傳資料、訓練、量化、匯出模型 | `docs/agents/skills.md` → `edge-impulse-project-setup` skill |
| 經 SSH 叫 Mac 上的 Codex 做大批改動 | `docs/agents/skills.md` → `codex-remote-mac` skill |
| 渲染驗收腳本、`evaluate_render.py` 用法 | `tools/skills/lvgl-preview/SKILL.md`（repo 內可攜 skill） |
| Commit 格式、changelog 寫法 | `docs/agents/commits.md` |
| 準備開 PR、避免改錯對象或數字不一致 | `docs/agents/workflow.md` |
| 資料從哪來、可否混用 | `data/README.md` |
| 文件全貌 | `docs/README.md` |

**漸進式披露的做法**：AGENTS.md 只寫「事實＋硬規則＋去哪裡找」，細節一律放 `docs/agents/` 或其他專門文件。新規則到來時，先問「這條是否每次都要知？」是就寫這裡，不是就寫 `docs/agents/` 並在此加一行指路。
