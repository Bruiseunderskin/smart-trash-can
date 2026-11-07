# UI 優化提案（2026-11-03）

狀態：**提案，待拍板。** 對應 PR `docs/ui-optimization`（與 `#20 docs/edu-game-concept` 同支）。

---

## 一 · 問題重新陳述

現有 Figma 設計（9 個 state frame）視覺語言好（深綠／奶油／青檸、粵語短句、圖示統一），但缺乏設計系統：字級混亂、顏色唔承載資訊、冇回合／冇倒數、AI 答案先揭曉、冇中途離場、對比度有 3 個位唔合格。

**優化唔係換風格，而係加系統。**

---

## 二 · 11 屏重設計（見 `ui-redesign-spec.md`）

| 屏 | 名稱 | 改動 |
|---|---|---|
| 01 | 主頁 | 拿走未開局分數（只留最高分）；兩顆按鈕（開始挑戰／圖鑑）等大 |
| 02 | 玩法 | 三步圖示 + 3 分鐘 / 5 回合 |
| 03 | 回合準備 | 回合／倒數首次出現 |
| 04 | 拍攝 | 相機框與 3·2·1 倒數 |
| 05 | 辨識 | **隱藏答案**（只顯示進度） |
| 06 | 投桶 | **新增**：四格蓋狀態（感測值驅動），玩家選擇＝關蓋 |
| 07 | 判定 | AI 判定 vs 玩家選擇；加分／0 分 |
| 08 | 解說 | 知識點 + 三個例子 |
| 09 | 成績 | 總分、答對、最常錯類別 |
| 10 | 圖鑑 | 2×2 四類 |
| 11 | 錯誤 | 人話 + 代號 + 兩出口 |

---

## 三 · 視覺系統（見 `ui-visual-quality.md`）

- **字級 5 級**（Display 112 / H1 56 / H2 32 / Body 26 / Meta 22），全部 8 px 網格上。
- **觸控目標 ≥ 85 px**（1 cm = 85 px @ 217 ppi）；按鈕 88–120 px 高，間距 16 px。
- **顏色修正**：廚餘文字 `#1B7A44`（4.82:1）、橙 `#9A5E14`（4.74:1）、灰 `#5A666C`（5.32:1）、有害 `#B23A2C`（5.34:1）；原色相保留作色塊。
- **一屏一個主行動**、一個主卡片、無陰影、無漸層、無 hover、動效 ≤ 300 ms。
- **圖示統一**一套線稿，1.5–2 px 線寬。

---

## 四 · 技術工具鏈（已決定，見 `embedded-ui-research.md` §8）

| 層 | 決定 |
|---|---|
| BSP | 採用 `espressif/esp32_s31_korvo_1` v1.1.0，取代自建 LCD init |
| 設計稿 | Figma（現有檔案保留風格，補 4 區版面系統） |
| 設計→代碼 | 現階段手寫 C + SDL 預覽為主打；LVGL Pro Community（免費、Figma Flow 可用、無 CLI、非商業）作為備案 / 日後升級 |
| 字型 | 現有生成式 `stc_font_*` 為準，加 22 px meta |

---

## 五 · 決定的三個邊界（寫給自己）

1. 不為工具改路線：現有 `stc_ui.c` + SDL 預覽已是資產，唔值得為 Figma 匯出放棄。
2. 不為「看起來更好」犧牲可驗證：每一個改動都要有對比 / 間距 / 字級的數字依據，唔係感覺。
3. 商業邊界：LVGL Pro Community 夠用於非商業；概念中提及「賣俾學校 ICT」的商業場景，日後需轉 Growth 授權。

---

## 六 · 下一步（建議 11/09 前完成）

- [ ] `esp32_s31` 接入官方 BSP，刪 `t1_lcd.c` 自建 init（Codex / 1 日）
- [ ] Figma 更新 11 個 frame，用同一套 grid / 顏色 token / 字級（隊員 / 0.5 日）
- [ ] `stc_ui.c` 加 `STC_AWAIT_BIN`（4 格蓋狀態、`player_bin`、`lid_mask`）（Codex / 0.5 日）
- [ ] `docs/ROADMAP.md` M3 中「端到端」驗證加一條「蓋關閉觸發記錄」
- [ ] 拍 30 張過膠卡片（啞面膠、統一尺寸、含背面與手持），按 session 分
- [ ] 訓練集不混入 33 張實物相（兩個域分開列）
- [ ] 答辯材料：三行域差距表（garbage265 → 卡片域）

---

文件已存入：`docs/ui-redesign-spec.md`、`docs/ui-visual-quality.md`、`docs/ui-flow-redesign.md`、`docs/embedded-ui-research.md`、`docs/lid-detect-research.md`、`docs/edu-game-concept.md`、`docs/ui-optimization-proposal.md`（本文件）。

本地技能：`/config/.hermes/skills/embedded-ui-visual-quality/`（已建立 SKILL.md + 參考）。
