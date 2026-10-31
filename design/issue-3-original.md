# 【UI】4.3 吋板載屏 + 1080p 大屏：介面規格與 Figma／Sketch 交接

> 提出：阿吉（Hermes Agent）
> **文件類型：設計規格書（PRD）**。設計由人用 Sketch + Figma MCP 完成，實作由 AI coding agent 完成。
> 與 #2 的關係：#2 定義硬件與 AI；本文定義「睇得到的那一層」。兩者共用同一份資料契約。

---

## 0. 執行者須知（給 agent）

1. 設計檔案是**唯一視覺來源**。agent 唔准自己「美化」或自己選色，所有顏色、字級、間距、圓角、動畫時長一律由 `tokens.json` 讀。
2. `tokens.json` 未提供前，只准做空殼 UI（灰底 + 佔位文字），唔准先用預設值交付。
3. 每個畫面要有**明確狀態枚舉**（見第 3 節）。未定義的狀態一律 render 成 `error`，唔准靜默。
4. 所有尺寸以 **px at 1x** 為單位寫，唔准用 `rem`／`em` 描述設計稿。
5. 對比度、字級、觸控目標尺寸要可以**用腳本驗證**（第 12 節有檢查清單），唔准憑肉眼說「夠大」。
6. 遇到設計稿缺漏，寫 `TODO(design):` 並列出所需欄位，唔准自行補完。

---

## 1. 分工

| 角色 | 做咩 | 交付物 |
|---|---|---|
| 人（設計） | Sketch / Figma 出稿、定義 tokens、出資產 | Figma 檔案 + Sketch 檔案 + `tokens.json` + `assets/` |
| Agent（實作） | 4.3 吋 LVGL 介面、1080p 網頁大屏 | `ui/lvgl/*`、`server/screen/*`、狀態機、資料綁定 |

**交接契約**：設計稿改了，只改 `tokens.json` 同資產；agent 重跑 build 即可，唔准手改樣式常數。

---

## 2. 兩個顯示面

| 面 | 硬件 | 解像度 | 觀看距離 | 用途 |
|---|---|---|---|---|
| **A 板載屏** | Korvo-1 4.3 吋 RGB | 800 × 480 | 0.5 – 1 m | 操作者（小朋友）睇 |
| **B 大屏** | 筆電／電視 HDMI | 1920 × 1080 | 1.5 – 3 m | 評審、圍觀者睇 |

第三個面是 A0 展板（841 × 1189 mm 印刷），共用同一套顏色與字體，但唔屬於 UI 實作範圍，另見展板工單。

---

## 3. 狀態機

### 3.1 板載屏（800×480）

| 狀態 | 畫面 | 必須顯示 |
|---|---|---|
| `idle` | 待機 | 「放物件落台」+ 相機預覽小窗 + 目前分數 |
| `capturing` | 對焦／穩定 | 預覽放大 + 穩定提示（手震提示） |
| `classifying` | 判類中 | 掃描動畫（≤ 900 ms 循環）+ 上一輪結果淡化 |
| `result_ok` | 正確 | 類別大字 + 信心值 + 得分 + 開蓋指示 + 連續答對數 |
| `result_retry` | 判錯／低信心 | 「再試一次」+ 兩個候選類別 + 換角度提示 |
| `error` | 硬件或網絡錯 | 錯誤碼 + 一句人話 + 重試按鈕 |

### 3.2 大屏（1920×1080）

| 狀態 | 畫面 | 必須顯示 |
|---|---|---|
| `idle` | 等候 | 專案名 + 三步玩法說明 + 累計人次 |
| `live` | 進行中 | 相機大畫面 + 倒數／掃描 + 目前分數 |
| `result` | 結果 | 去背物件圖 + 類別 + 得分 + 連續答對 |
| `stats` | 統計（可選） | 四類分佈 + 今日準確率 |
| `offline` | 離線 | 明確提示「本地模式」+ 仍可繼續玩 |

**狀態轉移**：`idle → capturing → classifying → result_ok | result_retry → idle`；`error` 可由任何狀態進入，回到 `idle` 需人手確認。

---

## 4. 資料契約

固件與大屏共用同一份 payload，由固件產生、經 SSE 推送：

```json
{
  "state": "result_ok",
  "round_id": "r-0042",
  "category": "可回收物",
  "category_id": 0,
  "confidence": 0.93,
  "candidates": [{"category": "可回收物", "p": 0.93}, {"category": "其他垃圾", "p": 0.05}],
  "score": 120,
  "streak": 4,
  "cutout_url": "/cutout/ab12cd.png",
  "latency_ms": 412,
  "model": "ei-waste-v3-int8",
  "ts": 1793000000
}
```

規則：

- `state` 枚舉必須與第 3 節完全一致，多一個值都唔准。
- `category_id` 0–3 對應 可回收物／廚餘／有害／其他，**順序由 `labels.h` 單一來源定義**（同 #2 的風險項對應）。
- `cutout_url` 可以為 `null`（板上判類時沒有去背）；UI 要處理 null，顯示原圖裁切。
- `confidence` 一律 0–1 浮點，UI 自己決定顯示成百分比。

---

## 5. Design tokens

### 5.1 schema

```json
{
  "color": {
    "bg": "#FFFFFF", "surface": "#F5F5F4", "fg": "#111827", "fg_muted": "#6B7280",
    "ok": "#15803D", "warn": "#B45309", "error": "#B91C1C",
    "cat0": "#1D4ED8", "cat1": "#15803D", "cat2": "#B91C1C", "cat3": "#4B5563"
  },
  "font": {
    "family_cjk": "Noto Sans TC",
    "family_num": "Inter",
    "cat_px": 112, "score_px": 80, "title_px": 56, "body_px": 32, "caption_px": 26
  },
  "space": {"xs": 8, "sm": 16, "md": 24, "lg": 40, "xl": 64},
  "radius": {"card": 16, "pill": 999},
  "motion": {"scan_ms": 900, "reveal_ms": 260, "settle_ms": 120}
}
```

### 5.2 顏色要求

- 四類顏色**不可以只靠顏色區分**：每個類別必須同時有 icon 或文字標籤（色盲 + 印刷黑白影印都要讀得到）。
- 文字對背景對比 ≥ **4.5:1**；大字（≥ 32 px 粗體）可放寬到 **3:1**。
- 類別色對白底要 ≥ 3:1（做色塊時）。
- 唔准用純紅綠做唯一對比（deuteranopia）。

### 5.3 字級（附計算依據）

可讀性用「視角 0.3°」做下限（大字標題可 0.2°）：

| 面 | 距離 | 0.3° 對應字高 | 建議字級 |
|---|---|---|---|
| 4.3 吋 800×480（約 217 ppi） | 0.5 m | 2.6 mm ≈ 22 px | 類別 112 px、分數 80 px、正文 ≥ 32 px |
| 4.3 吋 | 1 m | 5.2 mm ≈ 45 px | 只保留類別 + 分數，正文可省 |
| 1080p 24 吋（約 92 ppi） | 2 m | 10.5 mm ≈ 38 px | 主標 120–160 px、正文 ≥ 48 px |
| 1080p | 3 m | 15.7 mm ≈ 57 px | 只用大字 + icon |

**CJK 排版規則**：唔准加 letter-spacing；行高 1.4–1.6；唔准用 Thin／Light 字重在 100 px 以上（筆畫會斷）；數字與中文並排時，數字用 `font_num`，基線對齊。

---

## 6. 動畫

| 動畫 | 時長 | 備註 |
|---|---|---|
| 掃描（classifying） | 900 ms 循環 | 只動 transform／opacity，唔准動 layout |
| 結果揭示 | 260 ms | 類別先出、分數後出（相隔 120 ms） |
| 錯誤抖動 | 120 ms × 2 | 可關閉（`prefers-reduced-motion`） |

板上 LVGL 動畫一律 ≤ 300 ms、同時最多一個動畫，避免搶 CPU 影響推理。

---

## 7. 資產交付

| 類型 | 格式 | 尺寸 | 命名 |
|---|---|---|---|
| 類別 icon | SVG（已優化，無 embedded raster） | 48 / 96 px | `icon/cat-{0..3}.svg` |
| 狀態插圖 | PNG（透明） | 1× 與 2× | `art/state-{name}@1x.png` |
| 去背佔位圖 | PNG | 600×600 | `art/cutout-placeholder.png` |
| 品牌／標題字 | 轉曲 SVG | — | `brand/title.svg` |

**唔准**在板上用超過 2 MB 的單一資產；4.3 吋屏的圖一律先縮到 800×480 以內。

---

## 8. Figma／Sketch 交接流程

1. 在 Figma 用 **variables** 定義顏色／字級／間距，變數名**必須等於** `tokens.json` 的 key（例如 `color/cat0`、`font/cat_px`）。
2. Page 結構固定三個：`A-4.3in`、`B-1080p`、`Components`。
3. 圖層命名：`state/{state}`、`cat/{0..3}`、`comp/{name}`，方便 MCP 按名字抓。
4. 用 Figma MCP 讀取 → 匯出 `design/tokens.json` + `design/assets/`。
5. Sketch 檔案同步同一份 naming（Sketch 作為本地備份與離線修改）。
6. 每次改稿，commit 到 `design/` 目錄，並在 PR 描述寫「tokens 改了邊幾個 key」。

**Agent 端的對應**

| tokens | LVGL（4.3 吋） | 大屏（CSS） |
|---|---|---|
| `color.*` | `lv_style_set_bg_color` / `lv_style_set_text_color` | `--color-*` CSS variables |
| `font.*_px` | `lv_style_set_text_font`（需先轉 LVGL font，CJK 用 subset） | `font-size` |
| `space.*` | `lv_style_set_pad_*` | `gap` / `padding` |
| `radius.*` | `lv_style_set_radius` | `border-radius` |
| `motion.*` | `lv_anim_set_time` | `transition-duration` |

**CJK 字型注意**：LVGL 唔可以直接用 TTF 全字集，要用 `lv_font_conv` 生成 subset（只收錄實際用到的字），否則 flash 會爆。設計稿要用固定的字句集合，新增文案要重跑 subset 腳本。

---

## 9. 驗收清單

- [ ] 兩個面、11 個狀態全部有稿（4.3 吋 6 個、1080p 5 個）
- [ ] `tokens.json` 通過 schema 驗證，key 與 Figma variables 一一對應
- [ ] 文字對比 ≥ 4.5:1（大字 3:1），附檢查腳本輸出
- [ ] 四類類別同時有顏色 + icon + 文字，黑白影印仍可分辨
- [ ] 4.3 吋：類別字 ≥ 112 px、正文 ≥ 32 px；1080p：正文 ≥ 48 px
- [ ] 所有觸控目標 ≥ 44 × 44 px（4.3 吋屏）
- [ ] 資產命名符合第 7 節，SVG 已優化，單檔 ≤ 2 MB
- [ ] 動畫有 `reduced-motion` 版本
- [ ] 交付 `design/`（Figma 匯出 + Sketch 檔 + tokens + assets）
- [ ] agent 實作後，截圖與設計稿逐狀態對比，差異要寫明原因

---

## 10. 風險

| 風險 | 對策 |
|---|---|
| 設計稿用大量自訂字型，LVGL subset 爆 flash | 限定 1 套 CJK + 1 套數字字型 |
| 4.3 吋塞太多資訊 | 每狀態最多 3 個資訊層級；詳情放大屏 |
| 顏色在場地燈光下失真 | 對比度用最壞情況（白底 + 強光）驗，唔用螢幕預覽判斷 |
| Figma 改稿但固件未同步 | tokens 進 git，CI 檢查 `tokens.json` 有變但 `ui/` 未更新就報錯 |
| 中英文混排對齊錯 | 統一一套 baseline 規則，設計稿示範 3 個例子 |

---

## 11. 需要設計方先回答

1. 專案主色用邊個？有冇學校／課程的視覺規範要跟？
2. 4.3 吋屏用直度定橫度？（Korvo 預設 800×480 橫度）
3. 小朋友角色／吉祥物要不要出現？（會影響資產數量）
4. 分數用 0–100 定係「星級」？
5. 大屏要不要顯示累計統計（今日幾多人玩、準確率）？

---

*本規格由阿吉（Hermes Agent）整理。字級依 0.3° 視角下限推算，實際以現場預演為準。*
