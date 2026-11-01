# Edge Impulse 模型與 Espressif AI 框架

## 選擇

先用 `esp-tflite-micro` + `ESP-NN` 試用 Edge Impulse 的 int8 `.tflite`。使用者提供的 C++ TFLite 匯出含 1,087,200-byte 模型、160×160 RGB、四類；metadata 指出 int8 輸入與輸出，以及 601,964-byte arena。這些是檔案內容，不是運行成功的證據。實板 probe 正在驗證算子、記憶體與 `Invoke` 時間。

| 技術 | 用途 | 這個 `.tflite` | 狀態 |
|---|---|---|---|
| ESP-IDF | 平台、RTOS、硬件驅動 | 由應用整合推理庫 | 專案採用 |
| esp-tflite-micro | MCU 推理 runtime | 可讀 TFLite 格式；實際模型仍須確認所有算子、量化及記憶體 | S3 實板 probe 中 |
| ESP-NN | TFLite Micro 的晶片最佳化 kernels | 不轉換模型；隨 runtime 支援的算子加速 | S3 / S31 支援不等於此模型已通過 |
| ESP-DL | Espressif 推理 runtime | 不能直接把 `.tflite` 當 `.espdl` 載入；需轉換並確認算子 | YOLO 的 `.espdl` 主機模擬完成 |
| ESP-PPQ | 量化及 `.espdl` 匯出工具 | 官方介面接受 ONNX／PyTorch；TFLite 可先嘗試轉 ONNX，再驗證轉換與量化 | 此 EI 模型轉換未驗證 |
| ESP-VISION | 視覺應用／模型整合層 | 文件提供 TFLite 與 ESP-DL 兩條 runtime 路徑；官方板型表列 S3-EYE 與 S31-Korvo-1 | 本專案具體板型、映像與整合待驗證 |
| ESP-SR | 喚醒與語音命令 | 不執行影像分類模型 | 語音可選，粵語和 S31 待驗證 |
| LVGL | 畫面與觸控框架 | 不執行模型 | 4.3 吋實屏整合待驗證 |
| Xiaozhi | 語音互動韌體／協定 | 不能直接代替影像 runtime；可接本地分類結果 | 自由對話依賴服務端；離線分類須獨立 |

Edge Impulse C++ library、`.tflite` 模型與 ESP-NN 並非同一層。保留圖像 resize、RGB 順序與 int8 量化前處理，否則模型能執行也不能保證正確分類。原始資料存在跨分割重複，現有 accuracy 不是乾淨的獨立測試成績。

## 來源

- [Espressif esp-tflite-micro：ESP-NN integration](https://github.com/espressif/esp-tflite-micro/blob/master/README.md)
- [Espressif ESP-NN：支援晶片及 TFLite Micro](https://github.com/espressif/esp-nn/blob/master/README.md)
- [ESP-DL 入門：ESP-PPQ 匯出 `.espdl`](https://docs.espressif.com/projects/esp-dl/en/latest/getting_started/readme.html)
- [ESP-DL 量化工具](https://docs.espressif.com/projects/esp-dl/en/latest/tutorials/how_to_quantize_model.html)
- [ESP-VISION 支援的 S31 板型](https://docs.espressif.com/projects/esp-vision/en/latest/esp32s31/target-support/index.html)
- [ESP-VISION 模型 runtime（ESP32-P4）](https://docs.espressif.com/projects/esp-vision/en/latest/esp32p4/concepts/ai-inference.html)
- [ESP-SR 命令詞（ESP32-S3）](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html)

ESP-PPQ 的目標晶片設定影響產物；現有 YOLO `.espdl` 指定 `esp32s31`，不能直接視作 S3 測試產物。若要同 S3 比較，須另為 S3 量化並重新驗證。
