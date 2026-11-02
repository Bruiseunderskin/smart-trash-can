# Issues #6–#8：模型與部署比較

**資料洩漏限制：** 原資料有 14 組相同內容跨 train、validation 或 test（11 train/test、2 val/test、1 train/val）；EI 上傳資料另有 5 組 train/test 重複；下列數字是原始觀察，可能偏高，不能證明獨立測試門檻。重建去重資料並重新訓練和評估後才可作正式比較。


狀態：#6–#8 的工程實驗與匯出證據已記錄；#6 A/B 使用通用 person-detection latency stand-in，不是垃圾分類器；`probe.cc` 最小相容改動為 21 行（13 insertions / 8 deletions），符合 issue 上限。off 模式有 20 次 watchdog 訊息但完整返回。EI 標準 TFLite 已在 S3 做零輸入 Invoke，YOLO ESP-DL 只在主機模擬。模型選型仍是原型決定；兩個分類模型仍須在同一目標板量實圖、延遲和記憶體，才能決定產品方案。

| 項目 | #6 TFLM probe | #7 Edge Impulse | #8 YOLO11n-cls |
|---|---|---|---|
| 模型 | person_detect int8，96×96 灰階，2 類 | MobileNetV2 0.5，160×160 RGB，4 類 | YOLO11n-cls，160×160 RGB，4 類 |
| 評估 | 實板 20 次，ESP-NN 開／關 p95 104.92／5071.98 ms | 同一去重 980 張：LiteRT host int8 accuracy 80.71%、macro-F1 0.8054；TFLite 在 S3 零輸入 Invoke p95 858.635 ms | 同一去重 980 張：PyTorch/ONNX macro-F1 0.8627；ESP-PPQ host simulation int8 macro-F1 0.8585 |
| 產物 | 300568 B 模型；arena 開／關 120996／82292 B | TFLite v1：1,087,200 B `.tflite`；EON v2：3,542,055 B compiled C++，沒有 `.tflite` | ONNX 6,162,676 B；ESP-DL 1,796,864 B + graph JSON |
| 實板垃圾分類 | 不適用此模型 | v1 僅零輸入 Invoke；沒有驗證圖片分類；EON v2 未上板 | 未量測 |

表中 Studio float32 baseline 與舊 YOLO 結果仍不能直接排名；新增的 980 張 SHA-256 去重共享測試集，則提供 EI int8 與 YOLO 的同圖 host-side macro-F1 對照。EI 使用 Edge Impulse 的 FIT_SHORTEST 前處理，YOLO 使用其分類 pipeline；18 張較小原圖在 EI host evaluator 上採邊緣像素延拓。EI 訓練資料與這 980 張沒有完全相同 SHA-256，但近重複、場景關聯及 validation 選模洩漏仍未排除。LiteRT 在 Apple Silicon 主機執行，ESP-PPQ 用 TorchExecutor 主機模擬；兩者都不是同一目標板上的延遲測試。#6 使用另一個小模型，其加速比不可套用至垃圾分類模型。

## 共享去重測試集比較

| 模型輸出 | 同一 980 張 macro-F1 | 執行位置與限制 |
|---|---:|---|
| Edge Impulse MobileNetV2 0.5 int8 `.tflite` | **0.805371**（accuracy 0.807143） | LiteRT 2.3.0 host；EI FIT_SHORTEST RGB 前處理；980/980 測試檔 hash 已核對 |
| YOLO11n-cls ONNX / PyTorch | **0.862707** | host；ONNX/PyTorch 預測一致率 100% |
| YOLO11n-cls ESP-PPQ int8 simulation | **0.858549** | host TorchExecutor 模擬；不是 ESP32-S31 inference |

在這組共同測試上，YOLO ONNX macro-F1 比 EI 高 5.73 個百分點；ESP-PPQ 模擬比 EI 高 5.32 個百分點。這使 YOLO 成為較強的準確率候選，但尚未證明它在 S31 上可維持此分數或達到時延要求。EI `.tflite` 的 20 次 S3 零輸入 Invoke p95 是 858.635 ms，是目前唯一的分類候選實板 latency 記錄；它不含相機取像、前處理、顯示、語音或蓋子動作。

可重跑 EI host 評估：`tools/evaluate_ei_tflite.py`，鎖定依賴見 `tools/requirements-tflite-eval.txt`，結果及混淆矩陣資料見 `docs/ei-baseline/shared-dedup-test.json`。小於 160 像素的來源圖共 18 張；Evaluator 對上採樣右／下鄰點採 edge clamp，這是為 host 評估定義的安全邊界行為，不能說成與 Edge Impulse 韌體逐像素相同。

使用者提供的 EON v2 ZIP 與官方 API 下載檔 SHA-256 相同。它沒有 `.tflite`，metadata 是 `EI_CLASSIFIER_COMPILED=1`，並把 inferencing engine 標成 TFLite；生成 C++ 使用 TFLite Micro 類型與 resolver。這不是 ESP-DL `.espdl`，也未證明可直接連到 Espressif runtime。Studio 在 EON engine 下估算 int8 為 2,124 ms、548.6 KB RAM、944.1 KB flash；float32 為 9,708 ms、1.6 MB RAM、3.1 MB flash。這些是同一 EON engine 的量化設定估算，不能當成 EON/TFLite A/B 或實板數據。

要判斷哪個更適合 ESP32，需把 EI int8 和 YOLO ESP-DL 放到同一目標板，以同一保留資料集、前處理及計時範圍測 accuracy、p50/p95、峰值 RAM 和 flash，並確認算子支援。詳見 `issue6/report.md`、`issue7/report.md`、`ei-baseline/model-testing.md`、`issue8/report.md`。

## 建議

選型建議：若優先目標是準確率，先將 YOLO11n-cls → ESP-PPQ `.espdl` 作為 S31 候選；共享測試比 EI 高約 5.32 個 macro-F1 百分點。若優先目標是已知的 S3/TFLM 硬件整合路線，EI v1 int8 `.tflite` 是目前已有 S3 Invoke 證據的 baseline。兩者仍要在最終目標板以真實相機圖、同一 980 張保留集及完整測時範圍做測量後才能決定產品模型。EON v2 留作第三條候選，先確認生成 code 與 Espressif runtime 的整合相容性。

後續以固定權重在 980 張經 SHA-256 排除與原 train/val 完全重複的測試圖上，PyTorch／ONNX macro-F1 為 0.8627，一致率 100%；train/val 選模污染與近重複仍未排除。該結果不能代表 ESP-DL 實板量化模型。

## Edge Impulse 模型的 S3 實測

同一份 EI v1 int8 模型在 ESP32-S3、160 MHz、8 MB PSRAM、ESP-NN on、零填充輸入下完成 20 次 `Invoke()`：mean 864.717 ms、p50 858.466 ms、p95 **899.659 ms**，arena used **507,836 B**；完整序列輸出見 `ei-baseline/s3-invoke-log.txt`。這是模型推理時間，尚未包含拍照、resize、UI 或蓋子動作，亦未驗證分類準確率或 S31。第一輪連續執行觸發 task watchdog；後續每次 Invoke 讓出一個 tick 再測時，捕獲日誌無 watchdog 訊息。此模型推理 p95 低於 3 秒，不能證明拍照至開蓋總 p95 或連續 10 次通過。

在每次 Invoke 後讓出一個 tick 再測 20 次，ESP32-S3 模型 Invoke p95 為 **858.635 ms**（mean 858.607 ms；arena 507,836 B），捕獲日誌無 task watchdog 訊息。讓出時間不包括在 Invoke；本機完整流程仍待量測。

對同一 980 張排除完全重複的測試圖，以 200 張原訓練圖重新校準的 S31 ESP-PPQ 量化圖，在主機模擬 macro-F1 **0.8585**；重做量化產物與舊產物不完全相同，近重複及 train/val 選模污染仍未排除。不是 S31 實機分數。

## V2 系統驗收狀態

| 需求 | 目前狀態 | 已有證據／缺口 |
|---|---|---|
| 影相 → 判類 → 開蓋 p95 ≤ 3 秒 | **未證明** | EI 只有 S3 零輸入 Invoke p95 858.635 ms；沒有相機到蓋子全開的整機計時 |
| 4.3 吋主要顯示 | **未驗證實機** | 架構指定 ESP-IDF + LVGL，螢幕動畫、分類結果、分數與學習內容應為主介面；無目標屏幕現場證據 |
| 斷網可用 | **未完成整機驗收** | 建議離線分類、計分、動畫、伺服控制及本機粵語預錄語音；目前無斷網實機流程紀錄 |
| 連續 10 次 | **未測** | 尚無整機連續 10 次拍攝、判類、開蓋與復位日誌 |
| V2 粵語語音 | **架構建議已定，功能未實作驗證** | 離線本機鼓勵／教學音必有；有網絡時接 XiaoZhi 相容串流 ASR → LLM → TTS，實際粵語品質及服務供應商仍待量測 |

語音建議以小智相容服務作可選對話層，遊戲狀態、分類可信度、分數與伺服安全則由本機狀態機決定。斷網仍播放本機粵語短句；自由對話在網絡恢復後提供。`xiaozhi.me` 公開頁說明 ASR/LLM/TTS 架構，未能據此確認託管服務的實際 ASR/TTS 供應商。
