# 智分桶 EDU

智分桶 EDU 是一個開發中的環保教育原型：學生透過卡片與物件分類遊戲，認識可回收物、廚餘、有害及其他垃圾。目標是把相機、本機影像分類、4.3 吋畫面、遊戲和四個實體桶連成互動流程；綠豆提供教學引導。

**目前仍在整合階段。** UI 預覽、模型實驗、驅動測試和板級 bring-up 各有獨立成果，但未證明由相機到開蓋的完整流程；實屏、四蓋、實際準確率及整機延遲仍待驗證。

## 由這裡開始

| 讀者／目的 | 入口 |
|---|---|
| 實作整機整合 | [Issue 53 整合規劃](docs/issue53/README.md)及 [GitHub issue](https://github.com/Bruiseunderskin/smart-trash-can/issues/53) |
| 了解現行入口、架構與限制 | [架構](docs/architecture.md)、[文件索引](docs/README.md) |
| 讀取 YOLO 模型與部署限制 | [模型交接](docs/issue53/model-handoff.md)、[模型實驗記錄](docs/experiments/mixed-card-object/README.md) |
| 預覽畫面 | [LVGL UI 與 SDL 預覽](ui/lvgl/README.md) |
| 檢查板級 bring-up | [S31 bring-up](firmware/esp32s31/bringup/README.md)、[S3 硬件測試](firmware/esp32s3/README.md) |
| 進行資料集工作 | [資料來源和隔離規則](data/README.md)、[工具](tools/README.md) |
| 參與開發 | [開發指南](docs/development.md)、[AGENTS.md](AGENTS.md) |

## 目標流程與現況

```text
相機 → 前處理 → YOLO／ESP-DL int8 推理 → 四類映射
     → 遊戲狀態與計分 → LVGL 畫面 + 桶蓋／投放感測
```

GAME 分類目標是本機離線；語音對話有獨立模式邊界。模型結果、UI 動作和感測器事件必須經控制器處理，不能由 UI 直接改分數或操作硬件。**單蓋與四蓋開啟的遊戲規格仍有衝突**，實作前須按[決策記錄](docs/issue53/decisions.md)釐清。

| 部分 | 現有內容 | 未驗證或待整合 |
|---|---|---|
| UI | 800×480 LVGL 教學介面與 SDL 預覽 | 實屏與觸控、遊戲及模型接線 |
| 視覺模型 | YOLO11n-cls 訓練所得 S31 int8 `.espdl` 和標籤 | S31 實機推理、相機前處理一致性、未知物件拒判及現場表現 |
| 桶蓋 | S3 GPIO 和 PCA9685 獨立測試 | 目標 CDC 節點、四蓋實際動作與整機鏈路 |
| S31 | 板級 bring-up 工程 | 相機整合、codec、上板驗證 |
| 遊戲／模式 | 契約及設計文件 | 完整狀態機、計分、互鎖與端到端流程 |

## 模型與四個桶

本專案目前的 CV 路線是 **YOLO 分類模型 → ESP-DL `.espdl`**。已提供 [S31 int8 模型](models/mixed-card-object/mixed_s31.espdl)、[labels.txt](models/mixed-card-object/labels.txt) 及 [SHA256SUMS](models/mixed-card-object/SHA256SUMS)。這是現有實驗產物，不能只憑 host 結果聲稱 S31 或整機通過。

| 模型輸出次序 | 類別 | 固定桶位 |
|---|---|---|
| 0 | food／廚餘 | 1 |
| 1 | hazardous／有害 | 2 |
| 2 | other／其他 | 3 |
| 3 | recyclable／可回收 | 0 |

目前 `contracts/model/classes.json` 仍待按 issue 對齊；不要假設其 index 與模型輸出一致。獨立的 `data/field-probe` 不用於訓練、校準、選模或閾值選擇；來源域限制見[模型實驗記錄](docs/experiments/mixed-card-object/README.md)。

## 分類卡圖片與印製

[卡片圖片及印製說明](docs/images/cards/README.md)提供兩組圖片：

- [Set A 卡通拼版 PNG](docs/images/cards/set-a-cartoon-sheet.png)及[單張圖片](docs/images/cards/set-a/README.md)。
- [Set B 實物照片拼版 PNG](docs/images/cards/set-b-photo-sheet.png)及[單張圖片](docs/images/cards/set-b/README.md)。

印製前請看[來源與授權](docs/images/cards/credits.md)及圖片說明；Set B 部分去背和解析度仍建議修整，使用照片須遵守各自授權與署名要求。圖片存在不代表模型已驗證能辨識實體印刷卡。

## 硬件方向

目標板為 ESP32-S31，目標介面為 4.3 吋 800×480 LVGL；實物型號、顯示及觸控待核對。issue #53 設計以 S31 USB host 連接 S3 CDC 節點，節點使用 PCA9685 和 TCA9548A 接四個 ToF；此拓撲尚未完成整合及實測。既有 S3 GPIO 直接驅動是另一條測試路徑，不能假設與目標節點已接通。接線及量測記錄見[硬件交接表](docs/issue53/hardware-runbook.md)。

## 專案目錄

| 路徑 | 內容 |
|---|---|
| [firmware](firmware/README.md) | S31、S3、Arduino 和語音框架 |
| [contracts](contracts/README.md) | 模組介面與模型契約 |
| [ui](ui/README.md)及 [design](design/README.md) | LVGL 介面、設計來源及資產 |
| [models](models/README.md)、[data](data/README.md)、[tools](tools/README.md) | 模型、資料規則與評估工具 |
| [docs](docs/README.md) | 架構、計劃、測試與歷史證據 |

硬件命令、接線和先決條件以各子專案 README 為準。歷史實驗保留原始模型、資料與設備資訊，不應把 ESP32-S3 量測換算成 S31 成績。
