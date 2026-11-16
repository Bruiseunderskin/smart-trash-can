# 目錄地圖與檔案管理規則

這份是「東西該放哪裡」的唯一入口。改動任何新檔案類型、或想搬目錄之前，先讀這裡。

**結論先講：這個 repo 的目錄結構已經對了，不要再大改。** 25 個 top-level 項目各有單一職責，
每個目錄都有 README，`docs/README.md` 已是導覽索引。真正缺的只有兩件事：
**(1) 沒有檔案分類規則，(2) 沒有標記哪個目錄是第三方、哪個是機器生成的。** 這份文件補這兩件。

---

## 1. 目錄與職責

| 目錄 | 放什麼 | 不放什麼 |
|---|---|---|
| `firmware/` | S31／S3／Arduino 韌體與語音框架 | 模型權重（→ `models/`） |
| `firmware/xiaozhi-esp32/` | **上游第三方碼**，見同目錄 `VENDORED.md` | 自家邏輯（→ `firmware/common/`） |
| `app/` | 應用層：遊戲流程、計分、狀態機 | 硬體驅動 |
| `contracts/` | 跨模組契約：模型類別、前處理、通訊 | 實作 |
| `ui/lvgl/` | 手寫 UI C、生成 C、字型 C array、預覽 render | 設計稿（→ `design/`） |
| `ui/lvgl_ui_project/` | GUI designer 專案（XML／screens／components） | 手寫 C |
| `design/` | 設計來源：`tokens.json`、`content.json`、字型、icon SVG | 產生的 C（→ `ui/lvgl/generated/`） |
| `models/` | 模型最終產物（int8 `.espdl`）、訓練設定、`metrics.json` | 中間產物（→ gitignore） |
| `data/` | 授權清楚、可入訓練的原始資料，每份有 README 寫來源與授權 | 快取、下載物 |
| `tools/` | 建置／渲染／評估腳本；測試在 `tools/tests/` | 一次性臨時腳本 |
| `runs/` | 訓練執行輸出，**本機用，不進 repo** | 任何要交付的模型（→ `models/`） |
| `server/` | 後端服務 | 韌體 |
| `kaggle/` | Kaggle notebook 與訓練入口 | 本機腳本（→ `tools/`） |
| `docs/` | 給人讀的文字與小圖（單檔 < 2 MB） | 列印 PDF 拼版（見第 4 節） |
| `docs/issue/` | 逐個 issue 的證據與報告 | 現行規格（→ `docs/planning/`） |
| `docs/experiments/` | 歷史實驗記錄，須標明是否現行 | 現行數字（→ `models/*/metrics.json`） |
| `.github/workflows/` | CI | 本機腳本 |

`AGENTS.md` 留在 root，因為 agent 工具預設只讀 root。

---

## 2. 五類檔案，五種處理

新增任何檔案前，先分類。分類錯是這個 repo 唯一真正的雜亂來源。

| 類別 | 例子 | 去哪 | 進 git？ |
|---|---|---|---|
| **原始碼** | `.c .h .py .xml .json`（設定） | 對應 src 目錄 | 是 |
| **第三方碼** | `firmware/xiaozhi-esp32/` | `firmware/<name>/` ＋ `VENDORED.md` | 是，但要標 `linguist-vendored` |
| **資料** | `data/field-captures/*.jpg` | `data/<name>/` ＋ 該目錄 README 寫來源與授權 | 是，且必須有授權紀錄 |
| **生成物** | `ui/lvgl/generated/*.c`、`dataset/*`、`runs/*` | 生成物目錄，`.gitignore` 排除 | **否**（例外見下） |
| **交付物** | 印製 PDF、release zip | GitHub Release asset | 否 |

### 生成物的例外
若建置流程**無法**在乾淨 clone 上重生，才可以進 git；並要在檔頭寫 `GENERATED — do not edit`，
且 `.gitattributes` 標 `linguist-generated`。目前只有 `ui/lvgl/generated/` 屬此類。

### 大小門檻
- 單檔 **> 2 MB**：先問「這是資料、生成物、還是交付物」。三者都不是，才考慮進 git。
- 單檔 **> 10 MB**：不進 git。走 Release asset（單檔上限 2 GiB，一個 release 最多 1,000 個 asset）。
- **不要用 Git LFS 存會被頻繁 clone 的大檔。** 免費額度只有 1 GB 儲存 ／ **1 GB/月流量**；
  本 repo 有兩個 owner 加 agent 每日 clone，42 MB 的相片只要 clone 24 次就爆流量上限。
  真要搬，搬去 Release，不要搬去 LFS。

---

## 3. 永不提交

- `runs/`（訓練輸出：checkpoint、`best.onnx`、log）
- 建置產物：`build/`、`managed_components/`、`sdkconfig`、`compile_commands.json`、`ui/lvgl/dist/*.zip`
- 中間模型：float32 `*.onnx`（int8 `.espdl` 例外，那是交付物）
- 下載快取：`data/modelscope_cache/` 等
- 任何含憑證、token、個人資料的檔案

`models/wordless-padded/{cards-padded,dataset}/` 已由 `.gitignore` 排除，因為可由
`tools/build_wordless_padded.py` ＋ `dataset.csv`（記錄每張場景的卡、seed、SHA-256）完整重建。

---

## 4. 已知債務（未在這次改動處理，因為需要 owner 決定）

量測於 2026-11-15，HEAD `270dcf8`。全 repo 3,259 個已追蹤檔案、工作區 492 MB（`.git` 佔 204 MB）。

| 項目 | 實測 | 建議 | 為何這次不做 |
|---|---|---|---|
| `docs/cards/**/PDF/*.pdf` | 8 個 PDF，其中 600 dpi 兩個共 46 MB，合計 **89 MB**（全 repo 檔案體積第一） | 打包成 Release asset `cards-v1.0.zip`，`docs/cards/` 只留 README ＋ 縮圖 | 需要上傳 release，屬 owner 權限；且要改 `docs/cards/*/README.md` 的下載指引 |
| `ui/lvgl/dist/smart-trash-can-lvgl-800x480.zip` | 10.3 MB，可由 `python3 ui/lvgl/tools/package_release.py` 重建 | 移去 Release，README 改指向 release | 現在 `ui/lvgl/README.md` 直接連結這個路徑，直接刪會斷 link |
| `models/wordless-padded/model-float32.onnx` 等 | 4 個 float32 ONNX，共約 25 MB | gitignore，由訓練腳本重生 | 要先確認 `tools/evaluate_*.py` 的預設路徑不會找不到檔 |
| `firmware/xiaozhi-esp32/` 位置 | 1,732 檔（53%） | 可考慮搬去 `firmware/third_party/xiaozhi-esp32/` | 搬 1,732 個檔會令 `git blame` 斷裂，且要同步改 CMake 路徑；現階段加 `VENDORED.md` ＋ `linguist-vendored` 已解決「agent 誤改」這個真問題 |

---

## 5. 這次改動做了什麼

1. 新增 `.gitattributes`：把 vendored 目錄、生成目錄、二進位檔標清楚，令語言統計與 diff 不再說謊。
2. 新增 `firmware/xiaozhi-esp32/VENDORED.md`：讓人和 agent 一眼知道這是上游碼。
3. 新增本文件，並在 `AGENTS.md` §8 與 `docs/README.md` 加入入口。

**沒有搬任何檔案。** 結構本來就對，搬動只會製造 diff 噪音與斷 link 的風險。
