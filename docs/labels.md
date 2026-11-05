# 工作分類標籤

2026-11-05 建立（狀態標籤同日加入）。用於 issue／PR 分流：**邊條線、邊個做、去到邊**。

---

## 1. 三個維度

### 1.1 領域（每 issue 至少一個）

| 標籤 | 意思 |
|---|---|
| `ai-chatbot` | 小智語音／CHAT 模式／ASR-TTS 路徑、台詞 |
| `ai-model` | 模型訓練、量化、評估、推論效能 |
| `hardware` | bring-up、感測器、舵機、電源、接線 |
| `ui` | LVGL 畫面、狀態機顯示 |
| `docs` | 計劃、審核、報告、海報素材 |
| `game-logic` | 回合狀態機、計分、結算、佔位 UI |

### 1.2 狀態（每 issue 剛好一個）

| 標籤 | 意思 | 幾時用 |
|---|---|---|
| `status:pending` | 未開始／等嘢 | 等對方回覆、等零件、等上一手交貨。**唔等於「唔重要」** |
| `status:in-progress` | 做緊 | 有人正在動它 |
| `status:blocked` | 卡住 | 要拍板、要零件、等外部條件；同時應加 `needs:decision` 或寫明卡在什麼 |
| `status:done` | 完成／已被取代 | 關閉前一定加，方便日後篩歷史 |

> 狀態同 issue 開關**唔完全重疊**：`#24` 已完成但等 PR，所以係 `status:pending` 而仍然 open；`#22` 結論已入 plan，所以 `status:done` 而 close。

### 1.3 誰做／要咩

| 標籤 | 意思 |
|---|---|
| `for-codex` | 交給 Codex 執行，驗收表已寫好；它交 PR 後由我們移除 |
| `needs:decision` | 要 W Ed 或 sillysben 拍板，其他人做落去只會白做 |

---

## 2. 一眼睇：三條篩選

| 想知 | GitHub 篩選 |
|---|---|
| 邊啲等 Codex 開工 | `is:issue is:open label:status:pending label:for-codex` |
| 邊啲現時有人做緊 | `is:issue is:open label:status:in-progress` |
| 邊啲卡住、要人拍板 | `is:issue is:open label:status:blocked` ／ `label:needs:decision` |

三條都可以存成 GitHub 的 saved filter（Issues 頁 `Filters` 輸入後按 bookmark）。

---

## 3. 現行對應（2026-11-05）

| Issue | 領域 | 狀態 | 誰 |
|---|---|---|---|
| #26 TOF050C bring-up | `hardware` | `status:pending` | `for-codex` |
| #25 `stc_voice` | `ai-chatbot` | `status:pending` | `for-codex` |
| #24 B2 文字注入 | `ai-chatbot` | `status:pending`（等 PR） | Codex 已交貨 |
| #12 模型驗證 | `ai-model` | `status:in-progress`（等 PR） | Codex |
| #11 S31 外設自測 | `hardware` | `status:in-progress` | — |
| #2 移植到 S31 | `hardware` | `status:in-progress` | — |
| #6 S3 tflite-micro 探測 | `ai-model` `hardware` | `status:pending` | — |
| #31 遊戲邏輯（stub） | `game-logic` | `status:pending` | `for-codex` |
| #30 契約 PR（stc_game v1.1／stc_ui v1.0） | `docs` `ui` | — | — |
| #4 UI 後續 | `ui` | `status:pending` | — |
| #23／#22／#8／#7／#3／#1 | 各自領域 | `status:done` | — |

---

## 4. 規則

1. 一個 issue **一定要有**領域 ＋ 狀態；缺任何一個都算未整理
2. 狀態由**當前負責人**改，唔係由旁觀者改
3. `status:blocked` 必須在 issue 內寫明「卡在什麼、要誰解」，否則等於冇寫
4. 關閉 issue 前先加 `status:done`，再關
5. 唔用 GitHub 預設 `bug`／`enhancement` 作主要分類（保留但不依賴），因為它們講的是性質，唔係工作線
