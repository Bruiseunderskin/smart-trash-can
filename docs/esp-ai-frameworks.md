# Edge Impulse 模型與 Espressif AI 框架

## 選擇

先用 `esp-tflite-micro` + `ESP-NN` 試用 Edge Impulse 的 int8 `.tflite`。使用者提供的 TFLite v1 匯出含 1,087,200-byte 模型、160×160 RGB、四類；metadata 指出 int8 輸入與輸出，以及 601,964-byte arena。ESP32-S3 實板以零輸入執行 20 次；`Invoke` p95 858.635 ms、arena 507,836 B（每次推理後讓出一個 tick，讓出時間不計），記錄見 `ei-baseline/s3-invoke-yield-log.txt`。這未測影像正確率或端到端流程。

| 技術 | 用途 | 這個 `.tflite` | 狀態 |
|---|---|---|---|
| ESP-IDF | 平台、RTOS、硬件驅動 | 由應用整合推理庫 | 專案採用 |
| esp-tflite-micro | MCU 推理 runtime | 可讀 TFLite 格式；實際模型仍須確認所有算子、量化及記憶體 | S3、IDF 6.0.2 20 次 Invoke 成功；1.4.1 文件列 IDF 最高至 6.0；S31 + IDF 6.1 未驗證 |
| ESP-NN | TFLite Micro 的晶片最佳化 kernels | 不轉換模型；隨 runtime 支援的算子加速 | 1.4.2 文件列 S3 與 S31；S31 PIE kernel 只在 core 1，該模型 S31 未驗證 |
| EI EON v2 | EI 生成的編譯模型 C++ | 無 `.tflite`；metadata 仍選 TFLite engine，生成檔使用 TFLM 類型 | 未驗證與 Espressif fork 的編譯及算子相容性；非 `.espdl` |
| ESP-DL | Espressif 推理 runtime | 不能直接把 `.tflite` 當 `.espdl` 載入；需轉換並確認算子 | YOLO 的 `.espdl` 主機模擬完成 |
| ESP-PPQ | 量化及 `.espdl` 匯出工具 | 官方介面接受 ONNX／PyTorch；TFLite 可先嘗試轉 ONNX，再驗證轉換與量化 | 此 EI 模型轉換未驗證 |
| ESP-VISION | MicroPython 視覺應用層 | 支援板型提供 `tflite` 與 `espdl` 模組 | 包含 S3-EYE、S31-Korvo-1；S31 文件要求 IDF 6.1；本案不選作 C++ LVGL 主框架 |
| ESP-SR | 喚醒與語音命令 | 不執行影像分類模型 | 離線命令引擎可選；V2 語音必要，粵語和 S31 待驗證 |
| LVGL | 畫面與觸控框架 | 不執行模型 | 4.3 吋實屏整合待驗證 |
| Xiaozhi | V2 必要語音互動候選韌體／協定 | 不能直接代替影像 runtime；可接本地分類結果 | 自由對話依賴服務端；離線分類須獨立 |

Edge Impulse C++ library、`.tflite` 模型與 ESP-NN 並非同一層。保留圖像 resize、RGB 順序與 int8 量化前處理，否則模型能執行也不能保證正確分類。原始資料存在跨分割重複，現有 accuracy 不是乾淨的獨立測試成績。

## 來源

- [Espressif esp-tflite-micro：ESP-NN integration](https://github.com/espressif/esp-tflite-micro/blob/master/README.md)
- [ESP-IDF TFLite Micro component v1.4.1：IDF 支援表](https://components.espressif.com/components/espressif/esp-tflite-micro/versions/1.4.1/readme)
- [Espressif ESP-NN：支援晶片及 TFLite Micro](https://github.com/espressif/esp-nn/blob/master/README.md)
- [ESP-NN component v1.4.2：S3/S31 支援及 S31 PIE 執行限制](https://components.espressif.com/components/espressif/esp-nn/versions/1.4.2/readme)
- [ESP-DL 入門：ESP-PPQ 匯出 `.espdl`](https://docs.espressif.com/projects/esp-dl/en/latest/getting_started/readme.html)
- [ESP-DL 量化工具](https://docs.espressif.com/projects/esp-dl/en/latest/tutorials/how_to_quantize_model.html)
- [ESP-VISION 支援的 S31 板型](https://docs.espressif.com/projects/esp-vision/en/latest/esp32s31/target-support/index.html)
- [ESP-VISION 模型 runtime（ESP32-P4）](https://docs.espressif.com/projects/esp-vision/en/latest/esp32p4/concepts/ai-inference.html)
- [ESP-VISION 新模型導入：TFLite Micro 與 ESP-DL 兩種路徑](https://docs.espressif.com/projects/esp-vision/en/latest/esp32p4/how-to/add-model.html)
- [ESP-VISION S31 支援板型及模組](https://docs.espressif.com/projects/esp-vision/en/latest/esp32s31/target-support/index.html)
- [ESP-VISION S31 開發環境：IDF release/v6.1](https://docs.espressif.com/projects/esp-vision/en/latest/esp32p4/get-started/index.html)
- [ESP-SR 命令詞（ESP32-S3）](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html)

ESP-PPQ 的目標晶片設定影響產物；現有 YOLO `.espdl` 指定 `esp32s31`，不能直接視作 S3 測試產物。若要同 S3 比較，須另為 S3 量化並重新驗證。

### S31 的版本配對風險

ESP-NN 1.4.2 官方列出 S31，但 ESP-TFLite-Micro 1.4.1 的支援表只列到 ESP-IDF 6.0；ESP-VISION 的 S31 韌體則使用 IDF `release/v6.1`。因此採用 ESP-IDF + TFLM + ESP-NN 作 C++ 架構方向，但 S31 產品整合前須用指定 IDF 6.1 分支固定元件版本、完成 build 與模型 Invoke。現有 S3 probe 使用 IDF 6.0.2，不能代替這項驗證。
