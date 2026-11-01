# Issue #6 — ESP32-S3 上 esp-tflite-micro int8 延遲（ESP-NN A/B）

## 結果

```text
board: ESP32-S3-CAM（Issue #6 / ROADMAP 的板型稱呼；晶片為 ESP32-S3 QFN56 rev v0.2；16 MB flash / 8 MB PSRAM detected；廠牌及模組標記未由實物獨立確認)
idf: v6.0.2
model: TensorFlow Lite Micro person_detect.tflite, int8, 300,568 B, SHA-256 808cfdfc0cf3a6fa6f6fa26bfa379ea97c16d5db7334637766e39c3408502e9d
input: [1,96,96,1], int8; zero-filled; runs=20; CPU=160 MHz
esp_nn=on : mean=104.92 ms p50=104.91 ms p95=104.92 ms arena_used=120,996 B
esp_nn=off: mean=5071.97 ms p50=5071.96 ms p95=5071.98 ms arena_used=82,292 B
ratio(p95): 48.34x (5071.98 / 104.92)
probe.cc diff from PR #9: 11 insertions / 7 deletions (18 changed lines); timed Invoke loop unchanged
```

完整 ESP-IDF monitor serial capture（只遮蔽裝置 MAC）：

- ESP-NN on: [`s3-person-detect-espnn-on.log`](s3-person-detect-espnn-on.log)
- ESP-NN off: [`s3-person-detect-espnn-off.log`](s3-person-detect-espnn-off.log)

兩次都在同一塊 ESP32-S3 QFN56、同一 IDF 6.0.2、同一份全 int8 `person_detect.tflite` 上完成 20 次 `Invoke()`。兩個 build 都成功，刷寫後 flash hash 驗證成功；serial 輸出有 `runs=20` 的完整 RESULT。每次推理後 `vTaskDelay(10 ms)`，yield 不計入 Invoke 計時。ESP-NN on capture 沒有 watchdog 訊息；off capture 有 20 次 task-watchdog 訊息，但 20 次推理完成，輸出 RESULT 並正常由 `app_main()` 返回。

ESP-NN on 的 arena 較大（120,996 vs 82,292 B），符合 optimized convolution 需要額外 scratch memory 的觀察；此數字只適用於這個模型和設定。

## 指定 MobileNet 測試及模型替換

Issue 指定的公開 `mobilenet_v1_1.0_224_quant.tflite` 是 uint8 模型。ESP-NN on 時它在 `AllocateTensors()` 階段因 convolution kernel 支援限制而失敗，沒有產生可比較的 latency。為完成純推理時間 A/B，改用 TensorFlow Lite Micro 公開的全 int8 `person_detect.tflite`（96×96×1 灰階、2 類）；這是 latency stand-in，不是四類垃圾分類模型。此失敗不能推廣成「所有 uint8 模型都不支援」，S31 和產品模型仍須獨立驗證。

`person_detect.tflite` 下載來源及預期 SHA-256 已記在 [`README.md`](../../esp32s3/tflite_probe/README.md)。模型檔存於 Git 忽略路徑，沒有提交。

## Probe 修改範圍

相對 PR #9 的 `esp32s3/tflite_probe/main/probe.cc` 是 11 行新增、7 行刪除，共 18 行變更，符合 issue 少於 30 行限制。修改只移除已不適用的 `MicroErrorReporter` 建構參數，並在輸出加入 IDF、ESP-NN 設定及 arena 數字；原有 20-run 計時迴圈、統計計算和固定輸入方式沒有改變。A/B build 開關位於同一 probe 專案的 Kconfig/CMake。

## Issue #6 驗收

| 驗收 | 狀態 | 證據 |
|---|---|---|
| ESP-IDF build 成功 | 通過 | on/off build 均完成 |
| ESP-NN on/off 均燒錄，20 次完成 | 通過 | 兩份 monitor log；flash hash verified |
| mean、p50、p95、ratio、arena 有記錄 | 通過 | 上表及兩份完整 serial log |
| `probe.cc` 變更少於 30 行 | 通過 | 11 insertions / 7 deletions，相對 PR #9 |
| 產品模型、實際圖片與 S31 延遲 | 未涵蓋 | 本 probe 是 `person_detect` 零輸入 S3 microbenchmark；須在模型及 S31 就緒後另測 |

因此這組結果只描述 S3 上 `person_detect` 的 microbenchmark，不代表垃圾分類準確率、EI 模型速度、相機至開蓋 latency 或 S31 實測。
