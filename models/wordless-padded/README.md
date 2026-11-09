# 加框無字卡模型產物

讀者：模型與韌體整合人員。何時讀：取用或替換加框無字卡分類模型時。

## 內容

| 檔案 | 說明 | 未見卡準確率 |
|---|---|---:|
| `model-int8.onnx` | **建議使用**。YOLO11n-cls 160×160，int8；班級平衡＋真實照片背景＋實物相加入訓練 | **0.6875** |
| `model-int8-scene-split.onnx` | 早期版本：38 張卡全部入訓練的場景切分模型 | 0.558（場景切分本身 1.000） |
| `labels.txt` | 輸出次序 `food, hazardous, other, recyclable`（按資料夾字母排序） |

輸入 `images`: `[1, 3, 160, 160]` float32；輸出 `output0`: `[1, 4]`。已用 onnxruntime 載入實跑過一次隨機輸入，形狀與輸出正常。

## `model-int8.onnx` 的訓練設定

Kaggle kernel `wedsamuel1230/stc-wordless-padded-cards` v8（與 v7 同配方）：

- 卡片身分切分：30 張卡入訓練、8 張卡留作驗證（未見過的卡）
- 班級平衡：每類約 1,400 個訓練樣本（場景＋實物），驗證每類 24
- 場景背景改用真實照片（模糊處理），另把 104 張實物相（每類 26）×8 加入訓練
- YOLO11n-cls，160×160，AdamW，early stop（best epoch 2，共跑 9 epoch）

結果：val 96 張，準確率 0.6875、macro-F1 0.6287。

混淆矩陣（列＝真值，欄＝預測，次序 food／hazardous／other／recyclable）：

- food 24/24
- hazardous 18/24（5 張判去 other、1 張判去 recyclable）
- other 24/24
- recyclable 24/24

## 前後對照（同一驗證協議：8 張未見卡、96 張場景）

| 版本 | 設定 | 準確率 | macro-F1 |
|---|---|---:|---:|
| v4 | MobileNetV3-small，6,000 場景 | 0.558 | — |
| v5 | YOLO11n-cls，場景切分（已知卡） | 1.000 | 1.000 |
| v6 | ＋真實照片背景＋實物相，未平衡 | 0.5521 | 0.4759 |
| v7 / v8 | ＋班級平衡 | **0.6875** | **0.6287** |

v6 的教訓：加了真實照片但未平衡班級，模型傾向全判 `recyclable`（該類佔訓練卡 14/30），反而變差。平衡後才見改善。

## 限制

- 驗證集與訓練集同源（同樣 38 張卡面渲染），不代表實物、現場或真正新卡表現。
- 只有 8 張 held-out 卡，每張卡約 ±4%，0.6875 的置信區間寬。
- 剩餘錯誤集中在 hazardous → other；該類只有 5 張訓練卡、26 張實物相，是瓶頸。
- 未在 S31 實板載入，未量 RAM／PSRAM／延遲，未比較 float32 與 int8 的逐張差異。

## 重現

見 [`../../data/mixed-card-object/wordless-v1/padded-training.md`](../../data/mixed-card-object/wordless-v1/padded-training.md)。
