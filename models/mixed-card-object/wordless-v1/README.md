# Wordless v1：S31 int8 候選模型

讀者：S31 整合與模型驗證人員。何時讀：載入、評估或重現候選模型之前。

**候選版本，未取代上一層 `mixed_s31.espdl`。** 同一個 292 張同源實物開發集的 ESP-PPQ int8 主機模擬，新版 241/292、舊版 240/292；結果只差一張，且個別類別退步，不能據此聲稱現場泛化更佳。逐圖及混淆矩陣見 [實驗結果](../../../docs/experiments/mixed-card-object/wordless-v1/README.md)。

## 交付和追溯

- `starting_mixed_yolo.pt`：現有混合模型的 float32 起始權重；`wordless_yolo.pt`：第 7 輪 checkpoint（檔名 `epoch6.pt`）。
- `wordless_yolo.onnx`：該 checkpoint 的 ONNX opset 18，float32，1×3×160×160；八張數碼卡上與 PyTorch 最大絕對輸出差 1.20e-7，未證明 target parity。
- `wordless_s31.espdl`：ESP-PPQ `espdl_quantize_onnx()` 產生的 ESP32-S31 `w8a8` int8 模型；同名 `.info` 和 `.json` 是量化工具產物。`labels.txt` 為 `food, hazardous, other, recyclable`；對應桶位 1、2、3、0。
- `SHA256SUMS`：所有上述產物及標籤的 SHA-256。

新增的封面負面檢查中，新 int8 仍以高信心錯誤放行數碼封面；見 [負面檢查](../../../docs/experiments/mixed-card-object/wordless-v1/negative-controls/README.md)。故此候選不能以分類 confidence 作為自動開蓋的充分條件。

## 實際流程

1. 以 [資料集與生成方法](../../../data/mixed-card-object/wordless-v1/README.md) 產生模擬卡變體並連接實物訓練圖，按身份分組。
2. 以 `tools/train_wordless_candidate.py`、Ultralytics 8.4.204、160×160 訓練；此候選使用 AdamW、lr 1e-5、10 epochs、seed 42、實物圖每張重複 5 次。第 7 輪（`epoch6.pt`）用既有實物開發集選出。
3. 將選定 YOLO checkpoint 用 Ultralytics 匯出 ONNX opset 18；圖的 ONNX checker 通過。
4. 執行 `tools/quantize_espdl.py`，內部調用 `espdl_quantize_onnx()`：以現有 100 張實物**訓練圖**（每類 25 張）校準，`--target esp32s31 --calib-images 100 --save-native`。100 張清單見 [calibration-physical.csv](../../../data/mixed-card-object/wordless-v1/calibration-physical.csv)。輸出 `.espdl`、`.info`、`.json`，可選 native graph 用於主機測試。
5. ESP-PPQ CPU native graph 評估只證明主機模擬。**部署至 S31 並驗證仍待實機**；必須核對相機色彩轉換、裁切、resize、輸入正規化、標籤順序、RAM 及 log。主機使用 Ultralytics `classify_transforms(160)`（bilinear resize、center crop、RGB tensor／255）；與目前 firmware 的 UYVY/PPA 路徑尚未證明一致。

部分來源素材授權未逐張核實；使用或再分發前應先完成權利核對。
