# 加框無字卡模型產物

讀者：模型與韌體整合人員。何時讀：取用或替換加框無字卡分類模型時。

## 內容

| 檔案 | 說明 | 未見卡準確率 |
|---|---|---:|
| `model-int8.onnx` | **建議使用**。YOLO11n-cls 160×160 int8，158 張卡面訓練 | **0.8251 ± 0.0632**（5 折） |
| `wordless_s31.espdl` | ESP32-S31 用，w8a8 量化，目標 `esp32s31`，1,812,848 bytes | 見下方量化章節 |
| `model-float32.onnx` | 量化前的 float32 版本，供對照與重新量化 | 同 int8 |
| `model-int8-scene-split.onnx` | 舊版：38 張卡的場景切分模型 | 0.558 至 0.6875 |
| `labels.txt` | 輸出次序 `food, hazardous, other, recyclable` | |

輸入 `images`: `[1, 3, 160, 160]` float32；輸出 `output0`: `[1, 4]`，已是 softmax 機率。已用 onnxruntime 實跑驗證。

## 5 折卡片身分交叉驗證

每一折把整張卡抽出驗證集，所以分數代表「未見過的卡」。

| 折 | 驗證卡 | 準確率 | macro-F1 |
|---|---:|---:|---:|
| 0 | 31 | 0.7788 | 0.7800 |
| 1 | 31 | 0.8521 | 0.8533 |
| 2 | 32 | 0.7637 | 0.7605 |
| 3 | 32 | 0.9207 | 0.9199 |
| 4 | 32 | 0.8104 | 0.8120 |
| **平均** | | **0.8251** | |
| **標準差** | | **0.0632** | |

改善來源：卡面由 38 張增至 158 張（新增 120 張由實物相生成），每折驗證集由 96 張增至約 800 張。

## ESP32-S31 量化模型

`wordless_s31.espdl` 由 `tools/quantize_espdl.py` 以 w8a8、`target=esp32s31`、240 張校準圖產生，檔案開頭為 `EDL2`，與既有 `mixed_s31.espdl` 同一格式。

- 單一測試集（163 張場景）：準確率 0.9509（155/163）、macro-F1 0.9556
- 逐類 F1：food 1.00、hazardous 0.951、other 0.929、recyclable 0.942
- 混淆：other 有 5 張判去 recyclable，hazardous 2 張判去 other，recyclable 1 張判去 hazardous

此測試集與訓練集同源（同一批卡面渲染），只證明量化沒有明顯掉點，不是未見卡數字。完整報告見 `wordless_s31.metrics.json`。

## 最終模型

最後一輪用全部 158 張卡、以場景切分訓練（留 10% 場景驗證），驗證準確率 0.9985。此數字只證明流程正常，因為同一張卡同時出現在訓練與驗證，不可當作未見卡表現。

## 信心校準

最終模型的溫度 `T = 0.65`，ECE 由 0.0036 降至 0.0015。此 ECE 在同一批卡面的場景驗證集上量度，數字偏樂觀；逐折校準未記錄。

## 舊版本對照

| 版本 | 設定 | 未見卡準確率 |
|---|---|---:|
| v4 | MobileNetV3-small，38 張卡 | 0.558 |
| v7 / v8 | YOLO11n-cls，班級平衡＋真實背景 | 0.6875 |
| v9 | 192 px，更強增強 | 0.6562 |
| v10 | 雙 seed 平均 | 0.625 |
| **現版** | 158 張卡，5 折 | **0.8251 ± 0.0632** |

## 限制

- 生成卡面由 Kaggle `sumn2u/garbage-classification-v2`（MIT）去背產生，個別卡有背景殘留或透明物件邊緣瑕疵
- 未在 S31 實板載入，未量 RAM／PSRAM／延遲；未比較 int8 與 float32 的逐張差異
- 卡面只有 158 張，5 折標準差 0.0632 仍偏大
- 驗證集與訓練集同源（同一批卡面渲染），不代表實物或現場表現

## 重現

Kaggle kernel `wedsamuel1230/stc-wordless-folds`（`kaggle/train_wordless_folds.py`）；資料與場景設定見 [`../../data/mixed-card-object/wordless-v1/padded-training.md`](../../data/mixed-card-object/wordless-v1/padded-training.md)。
