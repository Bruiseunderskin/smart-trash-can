# Issues #6–#8：模型與部署比較

**資料洩漏限制：** 原資料有 14 組相同內容跨 train、validation 或 test（11 train/test、2 val/test、1 train/val）；EI 上傳資料另有 5 組 train/test 重複；下列數字是原始觀察，可能偏高，不能證明獨立測試門檻。重建去重資料並重新訓練和評估後才可作正式比較。


狀態：部分完成。Edge Impulse 和 YOLO 均有離線結果；兩個垃圾分類模型仍須在同一 ESP32 板上量測，才能決定實機方案。

| 項目 | #6 TFLM probe | #7 Edge Impulse | #8 YOLO11n-cls |
|---|---|---|---|
| 模型 | person_detect int8，96×96 灰階，2 類 | MobileNetV2 0.5，160×160 RGB，4 類 | YOLO11n-cls，160×160 RGB，4 類 |
| 評估 | 實板 20 次，ESP-NN 開／關 p95 105.232／5073.260 ms | Studio float32 測試 788 張，accuracy 78.17%，macro-F1 0.800 | 1000 張，ESP-PPQ 模擬 int8 macro-F1 0.8594；ONNX 與 PyTorch 50/50 一致 |
| 產物 | 300568 B 模型；arena 開／關 120988／82300 B | TFLite 匯出含 1087200 B .tflite；metadata arena 601964 B | ONNX 6162676 B；ESP-DL 1796816 B |
| 實板垃圾分類 | 不適用此模型 | 零輸入 Invoke 已量測；圖像與全流程未測 | 未量測 |

這些準確率不是公平的直接排名：測試數量、資料子集、模型精度與 EI 的 uncertain 拒絕規則不同。EI 的 int8 validation accuracy 83.8% 不能與 float32 test accuracy 混同。ESP-PPQ 結果是主機端量化模擬，不是 ESP32 推理結果。#6 使用另一個小模型，其加速比不可套用至垃圾分類模型。

附件 C++ library 的 metadata 顯示 `EI_CLASSIFIER_COMPILED=0`、TFLite engine；它提供 .tflite 與 C++ 包裝。貼文記錄 Studio 對 ESP32-S3 的估算：EON 2124 ms / 548.6 KB RAM，TFLite 9708 ms / 1.9 MB RAM；後者的精度選項未在貼文中完整保存，不能把兩個估算當作同模型的引擎比較。這些都是估算，尚非實板測量。尚未取得可核對的 EON 匯出檔。

要判斷哪個更適合 ESP32，需把 EI int8 和 YOLO ESP-DL 放到同一目標板，以同一保留資料集、前處理及計時範圍測 accuracy、p50/p95、峰值 RAM 和 flash，並確認算子支援。詳見 `issue6/report.md`、`ei-baseline/model-testing.md`、`issue8/report.md`。

## 建議

先選 Edge Impulse MobileNetV2 / TFLite 作 ESP32 原型：四類模型較小，且已有 Studio 部署估算與現成 C++ library。這是工程選型，不是速度或準確率的實板勝出結論。保留 YOLO ESP-DL 作對照：其主機端 int8 模擬 macro-F1 0.8594，但尚未證明 S3 或 S31 實機延遲與資源需求。若同板驗證顯示 EI 未達準確率要求，或 YOLO 達到延遲及記憶體預算，應依同一測試集結果重新選擇。

後續以固定權重在 980 張經 SHA-256 排除與原 train/val 完全重複的測試圖上，PyTorch／ONNX macro-F1 為 0.8627，一致率 100%；train/val 選模污染與近重複仍未排除。該結果不能代表 ESP-DL 實板量化模型。

## Edge Impulse 模型的 S3 實測

同一份 EI 匯出 int8 模型在 ESP32-S3、160 MHz、8 MB PSRAM、ESP-NN on、零填充輸入下完成 20 次 `Invoke()`：mean 864.717 ms、p50 858.466 ms、p95 **899.659 ms**，arena used **507,836 B**；完整序列輸出見 `ei-baseline/s3-invoke-log.txt`。這是模型推理時間，尚未包含拍照、resize、UI 或蓋子動作，亦未驗證分類準確率或 S31。連續運行中觸發 task watchdog，因此產品需設計不阻塞任務及避免 watchdog 訊息的排程。此模型推理 p95 低於 3 秒，不能證明拍照至開蓋總 p95 或連續 10 次通過。

在每次 Invoke 後讓出一個 tick 再測 20 次，ESP32-S3 模型 Invoke p95 為 **858.635 ms**（mean 858.607 ms；arena 507,836 B），捕獲日誌無 task watchdog 訊息。讓出時間不包括在 Invoke；本機完整流程仍待量測。
