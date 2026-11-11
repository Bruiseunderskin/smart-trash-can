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
| 四個桶 | 桶位 0 可回收、1 廚餘、2 有害、3 其他。2026-11 硬件改版：S3 節點用 **GPIO 直出 PWM 驅動舵機**（無 PCA9685）、4 × TOF050C（VL6180X）用 **XSHUT 逐支改址 0x30–0x33**（無 TCA9548A）、S31 ↔ 節點走 **ESP-NOW**（S31 是 host）。尚未實測 |
| 屏幕 | 目標 4.3 吋 800×480 LVGL（S31-Korvo-1）；實屏與觸控待驗證 |
| 舊資料 | 文件寫 ESP32-S3 的延遲數字屬當時量測，**不可換算成 S31** |

## 2. 資料：先查，再談生成

**repo 內追蹤的資料**（`git ls-files data`）：

- `data/field-captures/`：33 張自拍實物相，只作場景背景
- `data/field-probe/`：獨立測試資料，**不得混入訓練、校準或閾值選擇**
- `data/README.md`：資料規則與來源

**訓練資料唯一 source of truth**：`models/wordless-padded/`（158 張卡面 → 珠光紙剪影 sprite → 每張 200 張場景 ＋ 470 張持牌真實相片）。
由 `tools/build_wordless_padded.py` 重建，38 張印刷卡 hash 逐張對 `docs/cards/300dpi`；真實相片由 `tools/fetch_real_photos.py` 收集並在 `real-photos.csv` 逐張記授權。
**授權規則：來源或授權唔清楚嘅相片一律唔用。** 詳見 `models/wordless-padded/DATA-LICENSES.md`。
`data/mixed-card-object/` 與 `models/mixed-card-object/` 已於 2026-11-11 移除，理由見 `data/README.md`。

**只存在於本機、被 `.gitignore` 排除**（不入 repo）：

- `data/modelscope_cache/`：下載快取（約 631 MB），可刪。
- `data/waste4_ei_*`、`data/garbage265`：舊 Edge Impulse／garbage265 路線的資料，2026-11-11 檢查時本機已經冇。

訓練資料由 `tools/build_wordless_padded.py` 在 `models/wordless-padded/` 建立。

**硬規則：任何資料集或模型工作開始前，先 `ls data/`、讀 `data/README.md`，並寫出類別方案與四個桶的對應。** 沒有真實資料就停手報告「blocked，需要 N 張 X 照片」，不要用程序生成的合成圖代替。

合成資料唯一的合法用途是驗證管線（上傳→特徵→訓練→量化→匯出→量測），不得以其準確率描述實物表現，而且要在同一句講明它是合成。

## 3. 模型事實

- CV 路線：YOLO11n-cls 160×160 + ESP-DL `.espdl`。**現行產物是 `models/wordless-padded/`**：`wordless_s31.espdl`（w8a8、`esp32s31`、1,794,896 bytes）、`model-float32.onnx`、`model-int8.onnx`、`labels.txt`、`cards.csv`、`dataset/`、`SHA256SUMS`。訓練只用 38 張印刷白底卡（`docs/cards/300dpi`）。**尚未在 S31 實機驗證**。
- 已量測：已知卡未見場景 769 張，float32 769/769、int8 768/769（macro-F1 0.9988）。未見卡（10 張 held-out 身份）數字見 `models/wordless-padded/README.md`。未有印刷卡實拍，實物表現未測。
- 模型輸出 `food, hazardous, other, recyclable` 對應桶位 `1, 2, 3, 0`；`contracts/model/classes.json` 已同序對齊（`class_order_frozen`），`labels.txt` 由 `tools/build_wordless_padded.py` 產生。不得把模型 index 當桶位。
- 舊 Edge Impulse impulse、六類與 S3 數據是歷史記錄，不是現行模型參數。詳見 `docs/issue53/model-handoff.md`。舊 `mixed-card-object` 模型與資料已於 2026-11-11 移除。

## 4. 驗收規則

1. **int8 才是交付物**。float32 與 int8 要分開報，並在同一句寫明是哪一個。
2. 每個數字都要帶來源（哪個模型、哪個測試集、幾張）。
3. 桌面預覽 `ctest` 過 ≠ 屏幕對。要拉 `ui/lvgl/preview/render/*.png` 親眼看過。
4. 延遲只認實機量測（p50／p95）。估算可以寫，但要標明是估算，且不得寫進交付結論。
5. 沒有檔案的「完成」不算完成：本路線匯出模型要有 `.espdl`、`labels.txt` 與 hash，量測要有 log。

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
| YOLO 訓練、ESP-DL 量化、模型整合 | `docs/issue53/model-handoff.md`、`data/README.md` 及 `docs/agents/skills.md` |
| 經 SSH 叫 Mac 上的 Codex 做大批改動 | `docs/agents/skills.md` → `codex-remote-mac` skill |
| 渲染驗收腳本、`evaluate_render.py` 用法 | `tools/skills/lvgl-preview/SKILL.md`（repo 內可攜 skill） |
| Commit 格式、changelog 寫法 | `docs/agents/commits.md` |
| 準備開 PR、避免改錯對象或數字不一致 | `docs/agents/workflow.md` |
| 資料從哪來、可否混用 | `data/README.md` |
| 整機整合、契約衝突、驗收 | `docs/issue53/README.md` |
| 文件全貌 | `docs/README.md` |

**漸進式披露的做法**：AGENTS.md 只寫「事實＋硬規則＋去哪裡找」，細節一律放 `docs/agents/` 或其他專門文件。新規則到來時，先問「這條是否每次都要知？」是就寫這裡，不是就寫 `docs/agents/` 並在此加一行指路。
