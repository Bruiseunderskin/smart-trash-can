# Edge Impulse baseline — Waste-Rec (issue #7)

**資料洩漏限制：** 原資料有 14 組相同內容跨 train、validation 或 test（11 train/test、2 val/test、1 train/val）；EI 上傳資料另有 5 組 train/test 重複；下列數字是原始觀察，可能偏高，不能證明獨立測試門檻。重建去重資料並重新訓練和評估後才可作正式比較。


- Project: Waste-Rec, ID `1154067`, account `wedsamuel1230`, target ESP32-S3
- Impulse: Image 160×160 (fit-short, lanczos3, RGB) → Image DSP → Transfer Learning (Images)
- Backbone: MobileNetV2 160×160 0.5; 40 training cycles; lr 0.0005; validation 20%; augmentation on; int8 profiling on
- Dataset: 1,198 training / 788 test; food 299/198, hazardous 299/195, other 300/196, recyclable 300/199 (training/test)
- Test split uploaded by the user; no test split was intentionally used for training, calibration, or model selection; exact duplicates across splits were later found. It was classified once.

## Validation (int8 model, 20% hold-out from training set)

- Accuracy 83.8%, loss 1.10
- AUC 0.97; weighted precision/recall/F1 0.84
- Per-class F1: food 0.90, hazardous 0.84, other 0.83, recyclable 0.78

## Model testing (788-image test split)

The Studio "Model testing" run used the **Unoptimized (float32)** model version.

- Accuracy **78.17%**
- AUC 0.96
- Weighted precision 0.81, recall 0.81, F1 0.81
- Macro F1 (mean of per-class F1): 0.800

Confusion matrix (row = true label, %, columns include EI's `uncertain` rejection):

| true \ predicted | food | hazardous | other | recyclable | uncertain |
|---|---:|---:|---:|---:|---:|
| food | 94.9 | 0.0 | 1.5 | 1.5 | 2.0 |
| hazardous | 1.5 | 82.6 | 5.6 | 5.1 | 5.1 |
| other | 3.1 | 15.3 | 65.3 | 9.2 | 7.1 |
| recyclable | 6.0 | 6.5 | 8.0 | 69.8 | 9.5 |

Per-class F1: food 0.92, hazardous 0.81, other 0.72, recyclable 0.75.

Machine-readable copy: `confusion-matrix.csv`.

## Notes

- The deployable int8 model was not re-scored on the test split, to keep the single-look rule for the test set. Validation accuracy above refers to the int8 model; model-testing accuracy refers to float32.
- EI's confusion matrix includes an `uncertain` column (rejection), so rows sum to 100% across five columns.

## C++ TFLite export

使用者提供的 `waste-rec-cpp-mcu-v1-impulse-#1.zip` 含 `tflite-model/tflite_learn_1154067_2.tflite`，大小 1,087,200 bytes；metadata 為 160×160、4 類、int8 input/output、`EI_CLASSIFIER_COMPILED=0`，TFLite arena 601,964 bytes。此標準 `.tflite` 已在 ESP32-S3 上以零輸入成功執行 Invoke，詳見下節。

## ESP32-S3 模型 Invoke 實測

實測 EI TFLite int8 模型（SHA-256 `a213846fc867f8e2e4a1a032ac79bea858cfc75668dcd02f6e8c55af2b26dd94`），ESP-IDF 6.0.2、ESP-NN on、160 MHz、8 MB PSRAM；零輸入連續 20 次：mean 864717 µs、p50 858466 µs、p95 899659 µs、arena used 507836 B。`s3-invoke-log.txt` 為原始序列輸出。期間觸發 task watchdog，但 20 次完成；現階段需修改排程再驗證穩定運作。本數字不含相機、預處理、屏幕與蓋子，不證明總流程 ≤3 秒，也不代表 S31 結果。

### 加入排程讓出後重測

每次計時結束後讓出一個 tick（不包含在 Invoke 時間內），再次執行 20 次：mean **858607 µs**、p50 **858582 µs**、p95 **858635 µs**、arena **507836 B**；`s3-invoke-yield-log.txt` 的捕獲輸出中沒有 task watchdog 訊息。第一輪原始輸出仍保留。這只證明該 probe 執行期間的情況，不代表產品整合無 watchdog。

## EON 匯出比較（issue #7）

兩份 archive 都來自使用者選定的既有 Waste-Rec 專案（ID `1154067`）。你提供的 v2 ZIP 與透過 Edge Impulse 官方 deployment API 取得的 v2 ZIP，SHA-256 完全相同，因此兩者是同一份檔案。

| 匯出 | Archive 大小 / SHA-256 | 檔案結構 | metadata |
|---|---|---|---|
| TFLite v1 | 6,895,910 B / `bf59fe8ce92bb48520468212c73c1ce48af12709668f79fbd6fdffb016bd701d` | 1,389 個檔案；含 `tflite-model/tflite_learn_1154067_2.tflite`（1,087,200 B） | 160×160、4 類、int8、`EI_CLASSIFIER_COMPILED=0` |
| EON v2 | 7,312,642 B / `fb6af1698a09269b65e59f0b074294be259731245832b199d34f6e9126924503` | 1,387 個檔案；沒有 `.tflite`；含 `_compiled.cpp`（3,542,055 B）、`_compiled.h`（2,685 B）及 ops define header（5,649 B） | 160×160、4 類、int8、`EI_CLASSIFIER_COMPILED=1`、TFLite inferencing engine |

EON 產生的 C++ 仍引用 `TfLiteStatus`、`TfLiteTensor` 和 `MicroMutableOpResolver` 等 TFLite Micro API。這表示它不是 ESP-DL `.espdl` 檔；其模型檔和 EI runtime 與 Espressif `esp-tflite-micro` 的實際相容性仍未測。現有 S3 Invoke 數據只屬於 v1 標準 `.tflite`，不能套用到 EON v2。

Studio deployment 頁對 ESP32-S3、EON engine 的估算：int8 latency 2,124 ms、RAM 548.6 KB、flash 944.1 KB；float32 latency 9,708 ms、RAM 1.6 MB、flash 3.1 MB。這是 EON engine 內的量化／精度設定比較，不是 EON 對 TFLite 的比較，也不是實板量測。

兩份生成 SDK 內的授權聲明要求有效 Edge Impulse 訂閱。產品部署前須按實際方案核對授權條款。模型和生成 SDK 沒有加入 Git。
