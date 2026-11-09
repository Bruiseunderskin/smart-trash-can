# 加框無字卡模型產物

讀者：模型與韌體整合人員。何時讀：取用或替換加框無字卡分類模型時。

## 內容

| 檔案 | 說明 |
|---|---|
| `model-int8.onnx` | YOLO11n-cls，160×160，動態 int8 量化（ultralytics 匯出） |
| `labels.txt` | 輸出次序 `food, hazardous, other, recyclable`（按資料夾字母排序） |

## 這個檔案從哪來

由 Kaggle kernel `wedsamuel1230/stc-wordless-padded-cards` 的 v5 執行產生，設定為**場景切分**：38 張加框卡全部入訓練，驗證集是同批卡面的另外 682 個場景。

該次結果：val 準確率 1.000、macro-F1 1.000（混淆矩陣 116／121／151／294 全對角）。

## 限制（重要）

- 驗證集與訓練集同源，這個 1.000 只表示流程正確、示範用這 38 張卡可行。
- 換未見過的卡面（卡片身分切分，8 張 held-out）準確率只有 **0.558**，即模型未學到跨卡面的類別特徵。
- 未在 S31 實板載入，未量 RAM／PSRAM／延遲。
- 量化精度影響未逐張比較（只知 float32 與 int8 兩份都已匯出）。

## 重現

見 [`../../data/mixed-card-object/wordless-v1/padded-training.md`](../../data/mixed-card-object/wordless-v1/padded-training.md)。
