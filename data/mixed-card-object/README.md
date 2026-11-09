# 混合卡片／實物模型資料（未完整）

讀者：需要檢查或重現現有 YOLO11n-cls 混合模型的人。何時讀：使用本資料前。此資料集已整理，尚未證明可完整重建訓練。

[無額外卡片文字的新版本](wordless-v1/README.md) 包含 38 張卡面及其模擬變體；原始實物圖片的缺口仍在。

## 固定分類

`recyclable` → 桶 0 可回收；`food` → 桶 1 廚餘；`hazardous` → 桶 2 有害；`other` → 桶 3 其他。未知／拒判不是 other。

## 內容及缺口

| split | 類型 | 現存 | 原 manifest 缺失 |
|---|---|---:|---:|
| train | 真實物件 | 100 | 1117 |
| train | 模擬卡片 | 8360 | 0 |
| val | 真實物件 | 292 | 0 |
| val | 模擬卡片 | 1900 | 0 |

`images/` 按 split／類型／四類保存實際 JPEG；檔名為內容 SHA-256。`inventory.csv` 逐列列出原本訓練 manifest、dataset path 及 availability；空 path 的 1,117 列並無圖片。`calibration.csv` 指向 200 張原訓練圖片，不另複製數據。`fixed-card-lineage.csv` 記錄每張模擬圖的卡片 ID、原卡 hash、seed、variant 和輸出 hash。`fixed-card-reference/` 是 38 張低解析參考圖，並非印刷原始卡圖。

## 證據界線及權利

模擬卡同一個卡片身份出現在 train／val，故結果只描述該批合成卡的變體，不能當新卡或實物準確率。實物開發驗證與訓練同源，與 field-probe 亦可能同來源域；field-probe 不包含在此，也沒有用來訓練、校準、選模型或選閾值。

部分真實物件圖片的來源、個別授權及再分發權利尚未逐張確認。**資料已存於公開 repo，並不代表已有全部再分發權利**；後續分享及使用前需核實每張來源與授權或取得適當權限。不得宣稱可完整重建：1,117 張實物訓練圖、部分原始卡面及來源授權資料仍缺失。

模型、訓練參數和結果見 [實驗記錄](../../docs/experiments/mixed-card-object/README.md)，原始 manifest 見同一目錄的 `input-manifest.csv`。原始資料未包含後來 AI 修整的三張印刷卡；新增版本包含它們，也沒有模型在 S31 或實卡現場表現的證據。

## 核對

對 `inventory.csv` 每個 `available` 路徑重新計 SHA-256，並與該列比對；確認缺失列 path 空白、`calibration.csv` 的 hash 均在 train 中。勿更改測試集用途或以現有圖填補缺失。
