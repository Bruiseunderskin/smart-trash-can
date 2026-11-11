# docs

文件入口。呢份只做導覽：**邊個想知乜嘢，去邊份文件**。細節留在各自的檔案。

| 你想做什麼 | 先讀 |
|---|---|
| 了解現況邊界、程式入口 | [architecture.md](architecture.md) |
| 看目標設計、驗收條件 | [planning/system-architecture-decision.md](planning/system-architecture-decision.md) |
| 實作整機整合 | [issue/issue53/README.md](issue/issue53/README.md) |
| 接模型、看部署限制 | [issue/issue53/model-handoff.md](issue/issue53/model-handoff.md)、[../models/wordless-padded/README.md](../models/wordless-padded/README.md) |
| 改 UI、比 Figma 與畫面 | [ui/figma-vs-lvgl.md](ui/figma-vs-lvgl.md)、[../ui/lvgl/README.md](../ui/lvgl/README.md) |
| 改訓練資料或重訓 | [../data/README.md](../data/README.md)、[../models/wordless-padded/README.md](../models/wordless-padded/README.md) |
| 查模型實驗與歷史結果 | [experiments/README.md](experiments/README.md) |
| 改 agent 流程、commit 規則 | [agents/README.md](agents/README.md) |
| 印分類卡 | [cards/300dpi/README.md](cards/300dpi/README.md)、[cards/600dpi/README.md](cards/600dpi/README.md) |

## 子目錄

| 目錄 | 內容 |
|---|---|
| [agents](agents/README.md) | agent 專用：commit 格式、skills 對照、工作流程 |
| [cards](cards/300dpi/README.md) | 300／600 DPI 試印卡：PDF 拼版與逐張 PNG |
| [ei-baseline](ei-baseline/README.md) | 舊 Edge Impulse 路線的量測記錄（歷史） |
| [experiments](experiments/README.md) | 模型實驗記錄（多數已標示為歷史） |
| [images](images/README.md) | README 用的架構圖與流程圖 |
| [issue](issue/README.md) | 逐個 issue 的規劃、量測與結論 |
| [knowledge](knowledge/README.md) | 背景知識與研究筆記 |
| [planning](planning/README.md) | 計劃、決策、規格（現行與歷史並存） |
| [ui](ui/README.md) | UI 設計對照與檢視記錄 |
| [Yolo-model](Yolo-model) | YOLO／ESP-DL 匯出的證據 JSON（資料，不是說明） |

## 規則

- 新增文件要在本表加一行；被取代的不要刪，改成「歷史」並指向現行版本。
- 每份文件開頭寫「讀者」與「何時讀」。
- 連結用相對路徑，`python3 tools/check_docs.py` 會逐條核對。

[專案入口](../README.md)
