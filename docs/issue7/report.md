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

## C++ library 匯出與推理引擎

| 匯出 | ZIP 大小 / SHA-256 | 內容 | Metadata |
|---|---|---|---|
| TFLite v1 | 6,895,910 B / `bf59fe8ce92bb48520468212c73c1ce48af12709668f79fbd6fdffb016bd701d` | 1,389 個檔案；含 `tflite-model/tflite_learn_1154067_2.tflite`（1,087,200 B） | int8、160×160、4 類、`EI_CLASSIFIER_COMPILED=0` |
| EON v2 | 7,312,642 B / `fb6af1698a09269b65e59f0b074294be259731245832b199d34f6e9126924503` | 1,387 個檔案；沒有 `.tflite`；`tflite_learn_1154067_2_compiled.cpp` 為 3,542,055 B | int8、160×160、4 類、`EI_CLASSIFIER_COMPILED=1` |

你提供的 v2 ZIP 與透過 Edge Impulse 官方 deployment API 取得的 ZIP SHA-256 相同。

v2 的 `model_metadata.h` 標記 `EI_CLASSIFIER_COMPILED=1`，並只提供 `_compiled.cpp/.h`；它是 EON 編譯產物，不是可交給 TFLite Micro 模型載入器的 FlatBuffer。程式碼雖引用 TFLite Micro 型別，但尚未把整個 Edge Impulse SDK 在 Espressif component 中編譯及實機執行；不可據此宣稱 v2 可直接當 `.tflite` 或 ESP-DL `.espdl` 使用。官方 EON 說明亦描述其把 TFLite FlatBuffer 編譯為 C++ 權重與推理程式碼。

2026-11-02 在 Studio ESP32-S3 deployment 頁分別選取 EON 與 TensorFlow Lite 後讀取估算。這些是 Studio 的模型估算，不是實板計時：

| Inference engine | Precision | Studio total latency | RAM | Flash | Studio accuracy |
|---|---|---:|---:|---:|---:|
| EON Compiler | int8 | 2,124 ms | 548.6K | 944.1K | 未顯示 |
| TensorFlow Lite | int8 | 2,124 ms | 588.2K | 1.1M | 未顯示 |
| EON Compiler | float32 | 9,708 ms | 1.6M | 3.1M | 78.17% |
| TensorFlow Lite | float32 | 9,708 ms | 1.9M | 3.2M | 78.17% |

兩種 engine 的表列 latency 相同，但 RAM/flash 不同。這是 Studio 預估；不能用來代替硬件測試，也不能把同一 engine 的 int8/float32 當作 EON/TFLite 對照。

v1 `.tflite` 曾在 ESP32-S3 以零輸入連續執行 20 次。ESP-NN 開啟、每次 Invoke 後讓出一個 tick 時，p95 為 858.635 ms、arena 使用 507,836 B。這證明此 EI TFLite FlatBuffer 可經該 S3 TFLM 路線執行；沒有驗證真實圖片分類或 S31 實板支援，也沒有驗證 EON v2 SDK。

## Espressif runtime 與框架選擇

- 在已測 ESP32-S3 上，首個分類原型採 EI v1 int8 `.tflite` + ESP-IDF + `esp-tflite-micro` + ESP-NN；有實際 20-run 零輸入 Invoke 證據。
- Espressif 現行 ESP-VISION 文件列明 ESP32-S31 可走兩條 runtime：`.tflite` 經 TFLite Micro，`.espdl` 經 ESP-DL。ESP-VISION 是 MicroPython 韌體/應用框架，雖由 ESP-IDF 元件構成，但與本專案 XiaoZhi C++ 音訊、舵機及 LVGL 產品韌體不是即插即用整合；可作 S31 視覺原型參考，不建議直接替換整個產品框架。
- 產品控制、顯示與語音底層建議維持 ESP-IDF + LVGL + XiaoZhi 相容 C++ 音訊路徑。目標若是 S31，須針對 IDF 6.1 實際整合及驗證 TFLite Micro；現有 `esp-tflite-micro` 公開 CI 支援表列到 IDF 6.0，不能把 S3/IDF 6.0.2 測量直接推及 S31。
- ESP-DL 只直接載入 `.espdl`；ESP-PPQ 的公開匯出介面支援 PyTorch/ONNX。YOLO11n-cls 可用 ONNX → ESP-PPQ → S31 `.espdl` 路徑評估。EI `.tflite` 不可直接交給 ESP-DL；若要轉換，還須驗證 TFLite→ONNX、算子覆蓋及轉換後精度。

## 「Edge Impulse library vs YOLO」如何公平比較

Edge Impulse C++ library 是 SDK/模型封裝與部署選項，並非與 YOLO 對立的 CV 模型架構。當前兩個候選都是四類單張圖片分類器：EI MobileNetV2 0.5 與 YOLO11n-cls。就「一次放一件物品在固定相機前」而言，先做影像分類即可；只有需要同時定位多件物品/背景中目標時，才值得改成物件偵測。

目前數字不能判定 YOLO 比 EI 準：EI 78.17%/macro-F1 0.800 的 Studio 測試集與 YOLO 的 980 張去重集不同，EI split 還有跨 split 完全重複圖；YOLO 的 0.8627 PyTorch/ONNX 與 0.858549 ESP-PPQ 是同一去重集上的主機評估。要選贏家，須用同一組按來源/場景分組的 train/validation/test、相同輸入與預處理、同一真實影像集，並在目標板量 end-to-end latency、記憶體及分類指標。

## V2 粵語語音候選

官方 `xiaozhi.me` 公開說明有 streaming ASR → LLM → TTS，但沒有列出所有託管模式實際採用的供應商。另一個開源自架 `xiaozhi-esp32-server` 是可替換元件的參考，不代表官方託管後端。V2 語音必須分成：本機預錄粵語鼓勵/引導音（離線必有），及有網絡時的自由對話。

若需掌控粵語服務，原型候選採自架 XiaoZhi 相容服務：`DoubaoStreamASRV2`（其設定註明不填語言可識別粵語）、Qwen Flash LLM、TTS 換成能說繁體粵語的 `zh-HK-HiuGaaiNeural` 或 `zh-HK-WanLungNeural`。上游設定的 EdgeTTS 預設為普通話 `zh-CN-XiaoxiaoNeural`，需改音色；EdgeTTS wrapper 對這兩款 HK voice 的實際相容性、語音自然度、端到端延遲、成本和可用條款尚未測試。這是候選，不宣稱為 `xiaozhi.me` 目前使用的供應商。

## 結論

Edge Impulse v1 的標準 int8 `.tflite` 可經 ESP-IDF 的 TFLite Micro + ESP-NN 跑在已測 ESP32-S3；這是目前最有硬件證據的首個分類 baseline。保留 YOLO11n-cls + ESP-PPQ `.espdl` 作 S31 量化候選，但現有準確度/速度證據不足以宣布它較好。EON v2 是獨立的編譯 C++ SDK 產物，尚待在目標產品韌體整合。生成 SDK 的授權聲明要求符合有效 Edge Impulse 訂閱條款；產品部署前須核對適用方案。

## 官方技術參考

- [ESP-VISION：S31 新增 `.tflite` / `.espdl` 模型與 API](https://docs.espressif.com/projects/esp-vision/en/latest/esp32s31/how-to/add-model.html)
- [ESP-VISION 與 MicroPython / ESP-IDF 專案關係](https://docs.espressif.com/projects/esp-vision/en/latest/esp32s31/project-relationship/index.html)
- [ESP-VISION 原始碼、IDF 支援版本及用途聲明](https://github.com/espressif/esp-vision)
- [ESP-DL：`.espdl` 格式、ESP-PPQ 及 ONNX/PyTorch 匯出](https://docs.espressif.com/projects/esp-dl/en/latest/tutorials/how_to_quantize_model.html)
- [Edge Impulse：EON 將 TFLite FlatBuffer 編譯成 C++ 權重與推理程式碼](https://www.edgeimpulse.com/blog/new-in-eon-compiler-automatically-reduce-memory-with-tensor-deduplication/)
- [小智官方架構：串流 ASR / LLM / TTS](https://xiaozhi.me/home/en/)
- [自架小智服務：ASR / LLM / TTS 候選配置](https://github.com/xinnan-tech/xiaozhi-esp32-server/blob/main/README.md)
- [自架服務 DoubaoStreamASRV2 / EdgeTTS 設定](https://github.com/xinnan-tech/xiaozhi-esp32-server/blob/main/main/xiaozhi-server/config.yaml)
- [Microsoft Speech：繁體粵語 zh-HK 神經語音](https://learn.microsoft.com/azure/cognitive-services/speech-service/language-support?tabs=tts)
