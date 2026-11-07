# Commit 與 CHANGELOG 規則

## 1. 可以直接 commit，但要交代

工作分支（`feat/*`、`fix/*`、`docs/*`）**允許直接 commit**，不需要每個小步都開 PR。
條件只有一個：**同一個 commit 內更新 `CHANGELOG.md` 的 `[Unreleased]`**，講明讀者見到什麼變化。

- 沒有 changelog 條目 → 視為未完成，不要 push。
- 合併到 `main` 仍須 PR，而且 CI 綠燈。

## 2. Changelog 寫「影響」，唔係寫「工序」

| 唔好寫 | 應該寫 |
|---|---|
| 改咗 `stc_ui.c` 同 `main.c` | 圖鑑底部改為兩粒按鈕：「問綠豆知多啲」與「返回主頁」 |
| 修好 render | 預覽基準圖在 UI 改動後未重產，已重產並逐張核對 |
| 加咗 docs | 新增 `docs/agents/`，收納 commit 規則與 skills 對照表 |

判準：一個唔識程式的人讀完，知唔知件事變成點？知就寫對了。

## 3. 分類

用 Keep a Changelog 的四類：

- **Added**：新的東西
- **Changed**：行為或內容改了
- **Fixed**：修好錯誤
- **Removed**：刪走東西

## 4. Commit message 格式

```
<type>(<scope>): <一句講做咗乜>
```

- `type`：`feat`、`fix`、`docs`、`refactor`、`test`、`chore`
- `scope`：`ui`、`eval`、`model`、`agents`、`data` 等
- message 用英文；changelog 用繁體中文

message 與 changelog 的關係：**message 講範圍，changelog 講影響**。兩者要對得上，唔可以一個講 A 一個講 B。

## 5. 一個 commit 一件事

混合改動都會令 changelog 變得不可信。例如「改 UI ＋ 順手更新文件」要拆成兩個 commit，
各自在 `[Unreleased]` 加一條，並在條目後標明對應範圍。

## 6. 發版

把 `[Unreleased]` 的內容搬到新版本標題下：

```markdown
## [0.4.0] - 2026-11-08
```

版本號依語意化版本：介面或行為有變動 → minor；只修錯 → patch；不相容改動 → major。

## 7. 檢查清單（push 前）

- [ ] `git diff --cached` 只含今次想交付的檔案
- [ ] `CHANGELOG.md` 的 `[Unreleased]` 有一條對應今次改動
- [ ] 條目講影響，唔係講工序
- [ ] 有改 UI 就跑過 `ctest`，並親眼看過 `ui/lvgl/preview/render/*.png`
- [ ] 有改資料或模型就寫明來源與量測條件
