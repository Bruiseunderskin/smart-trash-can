# 【UI】4.3 吋板載屏：九狀態教育互動與 LVGL 交付

> 提出：阿吉（Hermes Agent）
> **文件類型：設計規格書（PRD）**。設計以 Figma MCP 的可編輯元件與 variables 為來源，實作輸出 LVGL。
> **2026-10-31 用戶授權修訂：** 板載 UI 擴展為九種狀態，加入分類圖鑑、分類小百科及完成分數回顧。優先重用現有資產。
> 與 #2 的關係：#2 定義硬件與 AI；本文定義「睇得到的那一層」。兩者共用同一份資料契約。

---

## 0. 本次範圍與歷史規格

2026-11-01 依用戶要求完成 4.3 吋板載 LVGL UI 及原生渲染評估，透過 feature branch／PR 交付。原規格的 1080p 大屏、Sketch 備份、後端／實機整合驗證已移至 [#4](https://github.com/Bruiseunderskin/smart-trash-can/issues/4)，沒有宣稱完成。原始規格另保留於 `design/issue-3-original.md`。下文保留跨顯示面資料契約供後續實作參考；本工單驗收以第 9 節修訂清單為準。

### 原設計交接規則

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

### 3.1 板載屏（800×480）：九種狀態

| 狀態 | 畫面 | 必須顯示／互動 |
|---|---|---|
| `idle` | 分類小隊主頁 | 相機小窗、目前分數及「開始挑戰」；首頁不提供延伸學習入口 |
| `category_guide` | 四類分類圖鑑 | 完成挑戰後選擇學更多才進入；四類圖示＋名稱＋例子；返回成績／返回主頁 |
| `learning` | 分類小百科 | `category_id` 對應類別、例子及分類提示；返回圖鑑／返回主頁；不直接重新開始挑戰 |
| `capturing` | 觀察與拍攝 | 相機預覽放大、移開手及保持穩定提示 |
| `classifying` | 一齊找答案 | 「你估會係邊一類？」、單一掃描動畫、上一輪結果或「尚無結果」 |
| `result_ok` | 分類結果＋學習回饋 | 類別大字、圖示、信心值、目前總分、連續答對數、分類小知識、實際開蓋狀態、「完成」 |
| `result_retry` | 鼓勵再試 | 「未睇清，再試就得」、兩個候選類別及機率、換角度提示；不把模型不確定當成小朋友答錯 |
| `summary` | 完成／挑戰回顧 | 控制程式提供的總分＋連續答對數、鼓勵語、返回主頁／繼續學分類 |
| `error` | 裝置或網絡錯誤 | 錯誤碼、已知原因的人話提示、重試；未知錯誤用通用提示 |

原有六個枚舉數值保留（`idle` 至 `error` = 0–5）；新增 `category_guide=6`、`learning=7`、`summary=8`。

**板載轉移契約：**

- `idle → capturing`
- `category_guide → learning | summary | idle`
- `learning → category_guide | idle`
- `capturing → classifying → result_ok | result_retry`
- `result_ok → idle | summary`
- `result_retry → idle | capturing`
- `summary → idle | category_guide`
- 任何狀態可進入 `error`；只有本次錯誤經人手重試確認後，控制程式才可送 `idle`。每次新錯誤清除確認。

**狀態權責：** 按鈕只產生操作意圖，由控制程式確認並套用狀態，避免本機分頁與 SSE 狀態分歧。控制程式須使用 `round_id` 丟棄過期推理結果；不得把舊一輪結果套到新一輪。UI 不自行加分、判定答對或重置分數。學習頁不加分。開始請求在狀態更新前去重；重試每次錯誤最多發出一次。

### 3.2 大屏（1920×1080）

| 狀態 | 畫面 | 必須顯示 |
|---|---|---|
| `idle` | 等候 | 專案名 + 三步玩法說明 + 累計人次 |
| `live` | 進行中 | 相機大畫面 + 倒數／掃描 + 目前分數 |
| `result` | 結果 | 去背物件圖 + 類別 + 得分 + 連續答對 |
| `stats` | 統計（可選） | 四類分佈 + 今日準確率 |
| `offline` | 離線 | 明確提示「本地模式」+ 仍可繼續玩 |

**板載完整轉移**以 3.1 為準；大屏狀態維持本節原有五種，本次未實作大屏。

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
  "candidates": [{"category_id": 0, "category": "可回收物", "p": 0.93}, {"category_id": 3, "category": "其他", "p": 0.05}],
  "lid_open": false,
  "error_code": null,
  "score": 120,
  "streak": 4,
  "cutout_url": "/cutout/ab12cd.png",
  "latency_ms": 412,
  "model": "ei-waste-v3-int8",
  "ts": 1793000000
}
```

規則：

- 板載 `state` 僅接受 3.1 的九種值；大屏使用 3.2 的五種值。未知值必須顯示 `error`。
- `learning` 必須提供有效 `category_id`；`summary` 使用控制程式提供的 `score`、`streak`。兩者均不自行計分。
- `score`、`streak` 為 0–4294967295 整數。一般分數使用 80 px；超過五位數改用 token 指定的 32 px 緊湊數字，完整顯示、不截斷。結果頁超長連對數使用 26 px 完整顯示。
- `candidates` 使用兩個不同且有效的 `category_id`；機率須為有限的 0–1 數值。`NaN`、無效類別或無效轉移顯示 `error`。
- `lid_open` 是裝置實際回報：false 顯示等待開蓋；true 才顯示桶蓋已開。UI 不驅動舵機。
- `error_code` 為短 ASCII 識別碼；已知碼提供對應提示，未知碼提供通用檢查提示。
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

板上 LVGL 動畫以此較嚴格限制為準：每段 ≤ 300 ms、同時最多一個動畫。現版用 260 ms 正向＋260 ms 反向的 opacity 掃描循環（合共 520 ms）；reduced-motion 完全停止動畫。網頁大屏的 900 ms 規則不套用到板載版。

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

## 9. 本次板載 UI 驗收清單（2026-11-01 修訂）

- [x] Figma 可編輯九狀態、四類圖鑑／小百科、完成分數頁；保留原六個枚舉值。
- [x] Figma tokens/layout、共用間距政策及可重現生成的 LVGL 9.5 C component。
- [x] 修正 retry 候選圖文重疊；統一圖文 24 px、卡片內距及 64 px 底列。
- [x] 延伸學習只可完成後進入；小百科可返回主頁；禁止早期捷徑。
- [x] 重用既有 MIT Tabler／OFL 字型，附來源及授權；數據與動作由 controller 管理。
- [x] 49 native 情境、75 對比檢查、13 觸控目標及 27 間距／導覽檢查通過。
- [x] 九狀態及類別／最大計數共 17 個 framebuffer；版面、缺字及視覺檢視證據完整，render evaluation PASS。
- [x] lvgl-preview 改用 PASS／FAIL／INSUFFICIENT_EVIDENCE；22 個 skill 測試通過，無強制人手 OK 閘門。
- [x] 提供 GitHub Actions 原生重建／截圖／評估 workflow；PR 需 CI 通過才可合併。

本清單只確認 source component + native preview。ESP32 target build、實體觸控／相機／舵機、1080p、Sketch 及兒童學習成效由 #4 跟進，不是本次已通過項目。

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


---

## 12. 2026-10-31 板載 UI 實作與教學設計修訂

### 本次交付範圍

- [Figma 可編輯設計：分類小隊／九種狀態](https://www.figma.com/design/g5J7a5DhDuX1UYucR6jitn)
- `design/tokens.json`：Figma variables 匯出；`design/layout.json`：逐畫面幾何與元件資料；`design/content.json`：分類提示及來源。
- `ui/lvgl/`：LVGL 9.5 C 元件、字型子集、既有 SVG 轉換資產、ESP-IDF component CMake、SDL 桌面預覽及檢查。
- `ui/lvgl/include/labels.h` 為類別順序單一來源；`tokens.json` 和 `layout.json` 須一起由 Figma 同步後再生成。修改元件布局不能只改顏色 tokens。
- 優先重用專案已存在的 MIT Tabler 圖示、OFL Noto Sans TC／Inter 字型及 Figma Simple Design System 按鈕。網站角色、插畫與受版權保護素材沒有複製。
- 本次沒有實作 1080p 大屏、Sketch 本地備份、相機／推理／舵機後端或實體燒錄。這些不能由桌面預覽推定完成。

### 教學與趣味性

參考 [Duolingo ABC](https://abc.duolingo.com/how-we-teach) 的短步驟互動，以及 [EPA Recycle City](https://www3.epa.gov/recyclecity/games.htm) 的日常選擇與回饋模式。採用「先觀察 → 看分類 → 學做法 → 回顧分數」，不增加懲罰性倒數或虛構學習成效。

分類提示依 [香港減廢網站 FAQ](https://www.wastereduction.gov.hk/en-hk/recycling-faq/greencommunity-faq) 及 [環保署小學教學資源](https://www.wastereduction.gov.hk/sites/default/files/werecycleschool/Teaching_Resource_Pack_WRR_P6_Eng.pdf) 編寫。四類是本專案教學分類，實際回收需配合收集點規則。有害類例子使用圖片練習，提示交由大人處理，不鼓勵兒童把真實危險廢物放上機器。

### 驗證界線與仍需確認

- native build、逐狀態／類別／資料驗證、對比／觸控目標與截圖，以 `ui/lvgl/preview/render/` 的實際報告為準；不因本段文字自動勾選上方驗收項。
- 桌面預覽使用示範數據；相機無資料時有明確佔位。去背為 null 時使用原圖，兩者皆無則顯示佔位。
- ESP32-S31 實體 RGB565、觸控、相機 buffer、AI 準確率、現場可讀性與學習成效仍需分別實測。
- `lvgl-preview` 以 native build、版面、缺字、對比／觸控、互動及逐畫面視覺檢視證據決定 PASS／FAIL／INSUFFICIENT_EVIDENCE。用戶回饋可選，沒有強制 pending／OK 閘門。



### 用戶後續修訂：完成後延伸學習與統一間距

- 正常學生流程為 `idle → capturing → classifying → result_ok → summary`。只有完成頁的「繼續學分類」可進入圖鑑及小百科。
- 小百科的主要按鈕為「返回主頁」，不再是「我想試一試」。圖鑑提供「返回成績」及「返回主頁」。回到主頁後不能直接進入圖鑑。
- 九種狀態維持不變；轉移圖及按鈕一同收窄。預覽快捷鍵 7–9 會先建立已完成的示範流程，不繞過控制規則。
- 間距來源：`design/spacing-policy.json` 與 Figma `space/*` variables。左右留白 24 px、一般卡片內距 24 px、緊湊分類卡內距 16 px、圖示與文字間距 24 px。
- 底部按鈕／提示列統一 64 px 高。提示列左右內距 24 px；總分／連續答對卡片上、下、左右均有 24 px 留白。
- 間距及延伸學習限制由設計檢查腳本、native 狀態／按鈕測試及實際 framebuffer 截圖驗證；最新結果以 render 報告為準。
