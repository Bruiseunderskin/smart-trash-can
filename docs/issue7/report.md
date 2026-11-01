# Issue #7 — Edge Impulse baseline 與匯出格式

## 專案與訓練基線

- 按使用者決定沿用既有 Waste-Rec 專案（ID `1154067`），沒有另建專案。
- Impulse：Image 160×160 RGB → Image DSP → MobileNetV2 0.5 transfer learning。
- 訓練 40 cycles，learning rate 0.0005，開啟 augmentation 與 int8 profiling。
- 2026-11-02 Studio 資料頁的類別數：

| 類別 | Training | Test |
|---|---:|---:|
| food | 299 | 198 |
| hazardous | 299 | 195 |
| other | 300 | 196 |
| recyclable | 300 | 199 |
| 總數 | 1,198 | 788 |

- Validation accuracy 為 83.8%；各類 F1：food 0.90、hazardous 0.84、other 0.83、recyclable 0.78。
- Studio float32 Model Testing accuracy 為 78.17%，macro-F1 為 0.800；混淆矩陣見 `../ei-baseline/confusion-matrix.csv`。
- 已發現 train/test split 間有完全重複圖。上述 baseline 可能偏高，不能當作獨立測試結果。

## C++ library 匯出檢查

| 匯出 | ZIP 大小 / SHA-256 | 內容 | Metadata |
|---|---|---|---|
| TFLite v1 | 6,895,910 B / `bf59fe8ce92bb48520468212c73c1ce48af12709668f79fbd6fdffb016bd701d` | 1,389 個檔案；`tflite-model/tflite_learn_1154067_2.tflite` 為 1,087,200 B | int8、160×160、4 類、`EI_CLASSIFIER_COMPILED=0` |
| EON v2 | 7,312,642 B / `fb6af1698a09269b65e59f0b074294be259731245832b199d34f6e9126924503` | 1,387 個檔案；沒有 `.tflite`；compiled C++ graph 為 3,542,055 B | int8、160×160、4 類、`EI_CLASSIFIER_COMPILED=1` |

你提供的 v2 ZIP 與透過 Edge Impulse 官方 deployment API 取得的 ZIP SHA-256 相同。

EON 生成 C++ 使用 TFLite Micro 類型與 `MicroMutableOpResolver`，不是 ESP-DL `.espdl`。尚未用 Espressif 的 TFLM component 編譯這份 EON graph，因此其相容性與實機 latency 未知。

Studio 對 ESP32-S3、EON engine 的估算：int8 為 2,124 ms、548.6 KB RAM、944.1 KB flash；float32 為 9,708 ms、1.6 MB RAM、3.1 MB flash。這是 EON 內的量化設定比較，並非兩種 engine 的 A/B 或實板結果。

v1 `.tflite` 曾在 ESP32-S3 以零輸入連續執行 20 次。ESP-NN 開啟、每次 Invoke 後讓出一個 tick 時，p95 為 858.635 ms、arena 使用 507,836 B。這證明 v1 檔案可經已測 TFLM 路線執行；沒有驗證真實圖片分類、S31 支援或 EON v2。

## 結論

Edge Impulse 可匯出標準 int8 `.tflite`，作為 TFLite Micro 模型來源。首個整合原型採 TFLite v1 + `esp-tflite-micro` + ESP-NN。EON v2 是生成 C++ 模型，需另驗證 Espressif runtime 的相容性。生成 SDK 內的授權聲明要求有效付費 Edge Impulse 訂閱；產品部署前須核對適用方案。
