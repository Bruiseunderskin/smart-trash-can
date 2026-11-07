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

## 與 YOLO 共用去重測試集的 int8 host 評估

使用匯出的 v1 `.tflite`（SHA-256 `a213846fc867f8e2e4a1a032ac79bea858cfc75668dcd02f6e8c55af2b26dd94`），在與 YOLO 相同的 980 張 test manifest 上以 LiteRT 2.3.0 執行。980 張影像 SHA-256 全部與 manifest 相符；與本地 1,198 張 EI 訓練影像的完全相同 hash 數為 0。manifest 與逐類混淆矩陣見 [`shared-dedup-test.json`](../ei-baseline/shared-dedup-test.json)。

| 指標 | EI int8 TFLite host 結果 |
|---|---:|
| Accuracy | 0.807143（80.71%） |
| Macro-F1 | **0.805371** |
| Food F1 | 0.912698 |
| Hazardous F1 | 0.804829 |
| Other F1 | 0.722581 |
| Recyclable F1 | 0.781377 |

輸入按 EI `FIT_SHORTEST` 中央裁切及 RGB 固定點 bilinear resize，再依模型 scale/zero-point 量化；18 張原圖須上採樣，host evaluator 對超出影像範圍的鄰點使用 edge clamp。這是明確記錄的 deterministic host 邊界規則，不是 EI SDK 對小圖的逐位重現。這組分數是在主機 LiteRT 上測準確率，不能當作 S3/S31 延遲或相機實拍結果。與 YOLO 的 shared-set macro-F1 對照見 [`../model-comparison.md`](../model-comparison.md)。近重複及訓練/驗證選模洩漏仍未排除。

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

v1 `.tflite` 曾在 ESP32-S3 以零輸入連續執行 20 次。ESP-NN 開啟、每次 Invoke 後讓出一個 tick 時，p95 為 858.635 ms、arena 使用 507,836 B。這證明此 EI TFLite FlatBuffer 可經該 S3 TFLM 路線執行；後續四張真實圖片 smoke test 通過；S31 實板及 EON v2 SDK 未驗證。

## Espressif runtime 與框架選擇

- EI v1 int8 `.tflite` 可經 ESP-IDF + `esp-tflite-micro` + ESP-NN 在 S3 完成零輸入 Invoke；實圖 probe 每次重載輸入後，四張各執行 20 次並正確分類。
- Espressif 現行 ESP-VISION 文件列明 ESP32-S31 可走兩條 runtime：`.tflite` 經 TFLite Micro，`.espdl` 經 ESP-DL。ESP-VISION 是 MicroPython 韌體/應用框架，雖由 ESP-IDF 元件構成，但與本專案 XiaoZhi C++ 音訊、舵機及 LVGL 產品韌體不是即插即用整合；可作 S31 視覺原型參考，不建議直接替換整個產品框架。
- 產品控制、顯示與語音底層建議維持 ESP-IDF + LVGL + XiaoZhi 相容 C++ 音訊路徑。目標若是 S31，須針對 IDF 6.1 實際整合及驗證 TFLite Micro；現有 `esp-tflite-micro` 公開 CI 支援表列到 IDF 6.0，不能把 S3/IDF 6.0.2 測量直接推及 S31。
- ESP-DL 只直接載入 `.espdl`；ESP-PPQ 的公開匯出介面支援 PyTorch/ONNX。YOLO11n-cls 可用 ONNX → ESP-PPQ → S31 `.espdl` 路徑評估。EI `.tflite` 不可直接交給 ESP-DL；若要轉換，還須驗證 TFLite→ONNX、算子覆蓋及轉換後精度。

## 「Edge Impulse library vs YOLO」如何公平比較

Edge Impulse C++ library 是 SDK/模型封裝與部署選項，並非與 YOLO 對立的 CV 模型架構。當前兩個候選都是四類單張圖片分類器：EI MobileNetV2 0.5 與 YOLO11n-cls。就「一次放一件物品在固定相機前」而言，先做影像分類即可；只有需要同時定位多件物品/背景中目標時，才值得改成物件偵測。

原 Studio 78.17%/macro-F1 0.800 與 YOLO 980 張數字仍不是同一測試集。新增的 980 張共享測試提供直接的 host accuracy 比較：EI LiteRT macro-F1 0.805371、YOLO ONNX 0.862707、YOLO ESP-PPQ S31 目標模擬 0.858549、S3 目標模擬 0.796771。故 S31 目標模擬偏向 YOLO，S3 目標模擬則略低於 EI，但 near-duplicate、來源場景及 train/validation 選模污染尚未排除，18 張小原圖的 EI resize 邊界亦採 host edge clamp。量產選型仍要在目標板量 end-to-end latency、記憶體，並以現場相機影像確認分類指標。

## V2 粵語語音決定

官方 `xiaozhi.me` 網站公開了 streaming ASR → LLM → TTS 架構及 offline wake word，沒有公開託管服務目前實際採用的 ASR/TTS 供應商。開源 `xiaozhi-esp32-server` 是可替換元件的自架參考，README 推薦的 streaming 組合為 XunfeiStreamASR + Qwen Flash + HuoshanDoubleStreamTTS；這是開源後端建議，不代表官方託管服務。

V2 建議使用 XiaoZhi 相容協議和服務，但粵語 provider 配置如下：

- **ASR：** Xunfei streaming ASR，設 `language=zh_cn`、`domain=xfime-mianqie`、`accent=cantonese`；官方 API 文件列出方言免切包含粵語，並指出方言需在控制台授權/購買。這比把 `DoubaoStreamASRV2` 空語言參數當作已驗證粵語更有明確依據。
- **LLM：** Qwen Flash，依 XiaoZhi server README 的 streaming 建議；system prompt 指定用繁體粵語、短句及兒童友善語氣。實際粵語自然度與回答安全性須測試。
- **在線 TTS：** XiaoZhi server `EdgeTTS` provider 配置 `voice` 欄，可試 `zh-HK-HiuGaaiNeural` 或 `zh-HK-WanLungNeural`。Microsoft 的 voice list 列出兩者為繁體粵語 neural voices；EdgeTTS adapter 的串流、首音延遲和實際語音效果仍須裝置端實測。
- **離線語音：** 遊戲狀態、鼓勵、重新嘗試和學習提示預錄為本機粵語音檔，並在斷網時繼續播放。V2 保證斷網遊戲可用及有聲提示；自由生成式對話只在有網時可用，未宣稱 ESP32 可離線跑完整 LLM/TTS。
- **互動呈現：** 4.3 吋屏幕顯示動畫、分類、得分、開蓋狀態和「了解更多」內容；語音補充鼓勵，不由 LLM 決定分數或舵機動作。例句：「你做得好好！」「唔緊要，我哋再試一次！」

README 推薦的 `HuoshanDoubleStreamTTS` 可作低延遲方案候選，但其設定範例聲線尚未證明支援粵語；必須先確認粵語 speaker ID 再選用。雲端 TTS 與服務商仍要做延遲、成本、隱私及使用條款評估。

## 結論

目前 S31 目標的分類準確率模擬偏向 YOLO11n-cls + ESP-PPQ：同一 980 張共享測試 macro-F1 為 0.858549，EI int8 TFLite 為 0.805371；S3 目標 YOLO 模擬為 0.796771，略低於 EI；產品部署建議先在 S31 驗證 YOLO `.espdl`。EI v1 標準 int8 `.tflite` 有 ESP32-S3 零輸入 Invoke 證據，四張實圖 smoke test 通過；p95 858.635 ms；它不是整機時延。EON v2 是獨立的編譯 C++ SDK 產物，尚待在目標產品韌體整合。生成 SDK 的授權聲明要求符合有效 Edge Impulse 訂閱條款；產品部署前須核對適用方案。

## 官方技術參考

- [ESP-VISION：S31 新增 `.tflite` / `.espdl` 模型與 API](https://docs.espressif.com/projects/esp-vision/en/latest/esp32s31/how-to/add-model.html)
- [ESP-VISION 與 MicroPython / ESP-IDF 專案關係](https://docs.espressif.com/projects/esp-vision/en/latest/esp32s31/project-relationship/index.html)
- [ESP-VISION 原始碼、IDF 支援版本及用途聲明](https://github.com/espressif/esp-vision)
- [ESP-DL：`.espdl` 格式、ESP-PPQ 及 ONNX/PyTorch 匯出](https://docs.espressif.com/projects/esp-dl/en/latest/tutorials/how_to_quantize_model.html)
- [Edge Impulse：EON 將 TFLite FlatBuffer 編譯成 C++ 權重與推理程式碼](https://www.edgeimpulse.com/blog/new-in-eon-compiler-automatically-reduce-memory-with-tensor-deduplication/)
- [小智官方公開架構：串流 ASR / LLM / TTS、離線 wake word](https://xiaozhi.me/home/en/)
- [自架小智服務：streaming 的 Xunfei ASR / Qwen Flash / Huoshan TTS 組合及 provider 清單](https://github.com/xinnan-tech/xiaozhi-esp32-server/blob/main/README.md)
- [自架 server：ASR 方言參數及 EdgeTTS voice 設定](https://github.com/xinnan-tech/xiaozhi-esp32-server/blob/main/main/xiaozhi-server/config.yaml)
- [訊飛流式聽寫：方言免切及粵語授權要求](https://www.xfyun.cn/doc/asr/voicedictation/API.html)
- [Microsoft Speech：繁體粵語 zh-HK 神經語音](https://learn.microsoft.com/azure/cognitive-services/speech-service/language-support?tabs=tts)
