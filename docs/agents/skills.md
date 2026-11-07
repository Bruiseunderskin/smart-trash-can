# Skills 與文件對照

開工前用這張表決定「用邊個 skill、讀邊份文件」。讀完就夠，不要一次過把所有文件吞下去。

## 1. Skills

| Skill | 位置 | 幾時用 | 主要內容 |
|---|---|---|---|
| `smart-trash-can-ui` | Hermes skill（`/config/.hermes/skills/engineering/`） | 改 UI、跑桌面預覽、重產 render 基準圖、操作 Figma | 檔案位置、build／ctest／capture 指令、九狀態與版面規則、Codex 派工方式、PR 流程 |
| `edge-impulse-project-setup` | Hermes skill（同上） | 上傳資料、訓練、量化、匯出模型、查 EI 專案現況 | EI API／SDK 用法、**上傳一定要 `--label`**、特徵與訓練 job、int8 與 model testing 的坑、S31 尺寸與延遲參考 |
| `codex-remote-mac` | Hermes skill | 要 Mac 上的 Codex 做大批改動（例如整份 UI 重構） | SSH 到 Mac、派工範圍寫法、成本估計、驗收方式 |
| `lvgl-preview` | **repo 內**：`tools/skills/lvgl-preview/SKILL.md` | 改渲染或驗收腳本、理解 `evaluate_render.py` 與 visual review 的關係 | 可攜 skill，含 `preview.py`、`capture.py`、`evaluate_render.py` 的用法與失敗案例 |
| `phone-recipes` | Hermes skill | 要用手機拍照、量測或重複操作 | 手機自動化流程；本 repo 的實拍採集會用到 |
| `api-anything` | Hermes skill | 某個網站沒有 API 但需要它的資料 | 逆向網站請求、抓資料 |

## 2. 文件

| 文件 | 幾時讀 |
|---|---|
| `AGENTS.md` | 每次開工，先讀 |
| `docs/agents/commits.md` | 準備 commit 或開 PR |
| `docs/README.md` | 想知成個 docs 有什麼 |
| `data/README.md` | 動任何資料之前 |
| `ui/lvgl/README.md` | 改 UI 之前 |
| `docs/model-comparison.md` | 比較模型路線、寫結論之前 |
| `docs/issue6/report.md`、`docs/issue8/report.md` | 引用延遲或替代路線數據時 |

## 3. 漸進式披露怎麼做

分三層：

1. **`AGENTS.md`**：每次都要知的事實與硬規則（現時六節＋commit 規則＋skills 指路）。
2. **`docs/agents/`**：做某類工作才需要的細節（commit 規則、skills 對照）。
3. **專門文件**：`data/README.md`、`ui/lvgl/README.md`、issue 報告、`tools/skills/lvgl-preview/`。

寫新規則時的判斷：

- 唔遵守就會立刻出錯 → 放第 1 層
- 做某項工作才需要 → 放第 2 層，並在第一層加一行指路
- 只為某一子系統服務 → 放第 3 層，並在 `docs/README.md` 登記

**不要**把第 2、3 層的內容複製入 `AGENTS.md`。複製會令兩份文件不同步，之後沒人知哪份為準。
