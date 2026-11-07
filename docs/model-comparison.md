# Issues #6–#8：模型與部署比較

狀態：部分完成。Edge Impulse 和 YOLO 均有離線結果；兩個垃圾分類模型仍須在同一 ESP32 板上量測，才能決定實機方案。

| 項目 | #6 TFLM probe | #7 Edge Impulse | #8 YOLO11n-cls |
|---|---|---|---|
| 模型 | person_detect int8，96×96 灰階，2 類 | MobileNetV2 0.5，160×160 RGB，4 類 | YOLO11n-cls，160×160 RGB，4 類 |
| 評估 | 實板 20 次，ESP-NN 開／關 p95 105.232／5073.260 ms | Studio float32 測試 788 張，accuracy 78.17%，macro-F1 0.800 | 1000 張，ESP-PPQ 模擬 int8 macro-F1 0.8594；ONNX 與 PyTorch 50/50 一致 |
| 產物 | 300568 B 模型；arena 開／關 120988／82300 B | TFLite 匯出含 1087200 B .tflite；metadata arena 601964 B | ONNX 6162676 B；ESP-DL 1796816 B |
| 實板垃圾分類 | 未量測 | 未量測 | 未量測 |

這些準確率不是公平的直接排名：測試數量、資料子集、模型精度與 EI 的 uncertain 拒絕規則不同。EI 的 int8 validation accuracy 83.8% 不能與 float32 test accuracy 混同。ESP-PPQ 結果是主機端量化模擬，不是 ESP32 推理結果。#6 使用另一個小模型，其加速比不可套用至垃圾分類模型。

附件 C++ library 的 metadata 顯示 `EI_CLASSIFIER_COMPILED=0`、TFLite engine；它提供 .tflite 與 C++ 包裝。貼文記錄 Studio 對 ESP32-S3 的估算：EON 2124 ms / 548.6 KB RAM，TFLite 9708 ms / 1.9 MB RAM；這是估算，尚非實板測量。尚未取得可核對的 EON 匯出檔。

要判斷哪個更適合 ESP32，需把 EI int8 和 YOLO ESP-DL 放到同一目標板，以同一保留資料集、前處理及計時範圍測 accuracy、p50/p95、峰值 RAM 和 flash，並確認算子支援。詳見 `issue6/report.md`、`ei-baseline/model-testing.md`、`issue8/report.md`。
