# Issues #6–#8：模型與部署比較

**資料洩漏限制：** 原資料有 14 組相同內容跨 train、validation 或 test（11 train/test、2 val/test、1 train/val）；EI 上傳資料另有 5 組 train/test 重複；下列數字是原始觀察，可能偏高，不能證明獨立測試門檻。重建去重資料並重新訓練和評估後才可作正式比較。


狀態：#6–#8 的工程實驗與匯出證據已記錄；#6 A/B 使用通用 person-detection latency stand-in，不是垃圾分類器；`probe.cc` 最小相容改動為 18 行，符合 issue 上限。off 模式有 20 次 watchdog 訊息但完整返回。EI 標準 TFLite 已在 S3 做零輸入 Invoke，YOLO ESP-DL 只在主機模擬。模型選型仍是原型決定；兩個分類模型仍須在同一目標板量實圖、延遲和記憶體，才能決定產品方案。

| 項目 | #6 TFLM probe | #7 Edge Impulse | #8 YOLO11n-cls |
|---|---|---|---|
| 模型 | person_detect int8，96×96 灰階，2 類 | MobileNetV2 0.5，160×160 RGB，4 類 | YOLO11n-cls，160×160 RGB，4 類 |
| 評估 | 實板 20 次，ESP-NN 開／關 p95 104.92／5071.98 ms | Studio float32 測試 788 張，accuracy 78.17%、macro-F1 0.800；TFLite 在 S3 零輸入 Invoke p95 858.635 ms | 去重後 980 張：PyTorch/ONNX macro-F1 0.8627；ESP-PPQ 主機模擬 int8 macro-F1 0.8585 |
| 產物 | 300568 B 模型；arena 開／關 120996／82292 B | TFLite v1：1,087,200 B `.tflite`；EON v2：3,542,055 B compiled C++，沒有 `.tflite` | ONNX 6,162,676 B；ESP-DL 1,796,768 B + graph JSON |
| 實板垃圾分類 | 不適用此模型 | v1 僅零輸入 Invoke；沒有驗證圖片分類；EON v2 未上板 | 未量測 |

這些準確率不是公平的直接排名：測試數量、資料子集、模型精度與 EI 的 uncertain 拒絕規則不同。EI 的 int8 validation accuracy 83.8% 不能與 float32 test accuracy 混同。ESP-PPQ 結果是主機端量化模擬，不是 ESP32 推理結果。#6 使用另一個小模型，其加速比不可套用至垃圾分類模型。

使用者提供的 EON v2 ZIP 與官方 API 下載檔 SHA-256 相同。它沒有 `.tflite`，metadata 是 `EI_CLASSIFIER_COMPILED=1`，並把 inferencing engine 標成 TFLite；生成 C++ 使用 TFLite Micro 類型與 resolver。這不是 ESP-DL `.espdl`，也未證明可直接連到 Espressif runtime。Studio 在 EON engine 下估算 int8 為 2,124 ms、548.6 KB RAM、944.1 KB flash；float32 為 9,708 ms、1.6 MB RAM、3.1 MB flash。這些是同一 EON engine 的量化設定估算，不能當成 EON/TFLite A/B 或實板數據。

要判斷哪個更適合 ESP32，需把 EI int8 和 YOLO ESP-DL 放到同一目標板，以同一保留資料集、前處理及計時範圍測 accuracy、p50/p95、峰值 RAM 和 flash，並確認算子支援。詳見 `issue6/report.md`、`issue7/report.md`、`ei-baseline/model-testing.md`、`issue8/report.md`。

## 建議

首個 ESP32 原型採 Edge Impulse MobileNetV2 int8 標準 `.tflite`，以 ESP-IDF + `esp-tflite-micro` + ESP-NN 整合。理由是現有 archive 確有 `.tflite`，而該檔已在 S3 執行 Invoke；這只是格式與推理路線的原型依據，還沒有影像分類正確率或整機時延證據。EON v2 保留作第二條 TFLM 整合候選，待確認生成 code 與 Espressif runtime 的相容性後再比。YOLO ESP-DL 保留為準確率對照；其主機模擬分數不能代表 S3/S31 實機。

後續以固定權重在 980 張經 SHA-256 排除與原 train/val 完全重複的測試圖上，PyTorch／ONNX macro-F1 為 0.8627，一致率 100%；train/val 選模污染與近重複仍未排除。該結果不能代表 ESP-DL 實板量化模型。

## Edge Impulse 模型的 S3 實測

同一份 EI v1 int8 模型在 ESP32-S3、160 MHz、8 MB PSRAM、ESP-NN on、零填充輸入下完成 20 次 `Invoke()`：mean 864.717 ms、p50 858.466 ms、p95 **899.659 ms**，arena used **507,836 B**；完整序列輸出見 `ei-baseline/s3-invoke-log.txt`。這是模型推理時間，尚未包含拍照、resize、UI 或蓋子動作，亦未驗證分類準確率或 S31。第一輪連續執行觸發 task watchdog；後續每次 Invoke 讓出一個 tick 再測時，捕獲日誌無 watchdog 訊息。此模型推理 p95 低於 3 秒，不能證明拍照至開蓋總 p95 或連續 10 次通過。

在每次 Invoke 後讓出一個 tick 再測 20 次，ESP32-S3 模型 Invoke p95 為 **858.635 ms**（mean 858.607 ms；arena 507,836 B），捕獲日誌無 task watchdog 訊息。讓出時間不包括在 Invoke；本機完整流程仍待量測。

對同一 980 張排除完全重複的測試圖，以 200 張原訓練圖重新校準的 S31 ESP-PPQ 量化圖，在主機模擬 macro-F1 **0.8585**；重做量化產物與舊產物不完全相同，近重複及 train/val 選模污染仍未排除。不是 S31 實機分數。
