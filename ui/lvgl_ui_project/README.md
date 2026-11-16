# ui/lvgl_ui_project — 最新一版設計（LVGL Pro）

**這是 UI 的設計來源（source of truth）。** 畫面在 **Figma** 上設計，再同步到這個 LVGL Pro 專案；
`ui/lvgl/generated/` 的 C 是從這裡匯出的產物，不是手寫的。改畫面改這裡，不要改生成的 C。

> 上一版 README 是 LVGL Pro 的空白範本（寫「nothing in it yet」），與實際內容不符 —— 這個專案已經有
> 完整的四類遊戲畫面。本次改動換成本檔。

## 這個目錄裡有什麼

| 路徑 | 內容 | 誰維護 |
|---|---|---|
| `project.xml` | LVGL 版本（9.6.0）、顯示目標、800×480 解析度 | 手改 |
| `globals.xml` | 專案層級的常數、顏色、字型、style、subject | 手改，改之前先讀 |
| `translations.xml` | 多語字串 | 手改 |
| `screens/` | 每個畫面一個 XML | **Figma 同步後更新** |
| `components/` | 可重用元件（`ui_*`、`mascot_*`、`comp_cat_*`） | **Figma 同步後更新** |
| `images/`、`fonts/` | 由 Editor 轉檔與編譯 | Editor |
| `sim/` | 內建模擬器，見 [`sim/README.md`](sim/README.md) | Editor |

**Figma 對應**：視覺決策（顏色、間距、字級）落在 `design/tokens.json`，由 `ui/lvgl/tools/generate.py` 讀取；
畫面結構落在本目錄的 XML。兩者不一致時，以 Figma／`design/tokens.json` 為準，不是以匯出的 C 為準。

## 不要做的事

1. **不要在這裡手寫 C。** C 由 Editor／CLI 匯出到 `ui/lvgl/generated/`。
2. **不要發明 XML 屬性。** 每個 widget 的合法 API 在 schema 檔；先讀再寫。詳見 [`AGENTS.md`](AGENTS.md)。
3. **不要為 `components/` 下每個 widget 資料夾補 README。** 那些目錄由 Editor 建立，
   `tools/check_docs.py` 已把本目錄列為 tool-managed tree，文件責任在**本檔**與 [`AGENTS.md`](AGENTS.md)。

## 驗證

```bash
lvglpro validate .     # XML 語法與 API 檢查
lvglpro screenshot     # 看實際畫面
```

改完之後要重跑匯出，再跑 `ui/lvgl` 的預覽與 render 驗收，才叫改完。

## 延伸閱讀

- LVGL Pro 文件：<https://lvgl.io/docs/pro>
- 本專案的 UI 生成流程：[`../lvgl/README.md`](../lvgl/README.md)
- 目錄與檔案分類規則：[`../../docs/repo-map.md`](../../docs/repo-map.md)
