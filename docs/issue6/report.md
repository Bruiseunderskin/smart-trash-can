# Issue #6 — ESP32-S3 的 TFLite Micro 延遲與 ESP-NN A/B

## 指定 MobileNet 實測

```text
board: ESP32-S3 QFN56 rev v0.2；16 MB flash、8 MB PSRAM
idf: v6.0.2
model: mobilenet_v1_1.0_224_quant.tflite；4,276,352 B
sha256: ecc3a67c47c5a609ec35f6a58a7d97532834e43df4cb7d3f1204a8164b7d20dd
input: uint8 [1,224,224,3]；1001 outputs
arena: 4,194,304 B allocated in PSRAM
```

模型來自 [Google Android Gitiles mirror](https://android.googlesource.com/platform/test/mlts/models/%2B/73313692ee90e6d768cd086fbb7c94298ecaf1ac/assets/mobilenet_v1_1.0_224_quant.tflite)。Issue 內原 Google Cloud Storage URL 回傳 403；mirror 同檔名及大小，但無法與受阻的 GCS 物件作逐位比對。

ESP-NN on、off 的 build 及 flash 均成功。兩個版本都找到 8 MB PSRAM，並配置 4 MB arena；兩次都在 `AllocateTensors()` 的第一個 `CONV_2D` prepare 遇到同一錯誤：

```text
Hybrid models are not supported on TFLite Micro.
Node CONV_2D (number 0) failed to prepare with status 1
AllocateTensors failed
```

ESP-NN off 的 build command 確認對 TFLM component 加上 `-UESP_NN`。因此這不是 A/B 開關失效、arena 配置失敗或缺少 resolver operator 的結果。兩次均未進入 `Invoke()`，沒有指定 MobileNet 的 mean、p50、p95 或 ratio。完整、已遮蔽 MAC 的 serial captures：

- [ESP-NN on](mobilenet-espnn-on.log)
- [ESP-NN off](mobilenet-espnn-off.log)

## int8 latency stand-in

指定 MobileNet 無法執行後，原有 A/B probe 仍以全 int8 TensorFlow Lite Micro `person_detect.tflite` 完成計時。這只是 kernel 延遲 stand-in，輸入為 96×96 灰階、兩類、零填充；不是四類垃圾分類模型。

```text
ESP-NN on : mean=104.92 ms p50=104.91 ms p95=104.92 ms arena_used=120,996 B
ESP-NN off: mean=5071.97 ms p50=5071.96 ms p95=5071.98 ms arena_used=82,292 B
ratio(p95): 48.34x
runs: 20 per mode
```

兩種設定各完成 20 次 `Invoke()`；off 組出現 20 次 task-watchdog 訊息，但仍完成並輸出結果。完整 serial captures：

- [person_detect ESP-NN on](s3-person-detect-espnn-on.log)
- [person_detect ESP-NN off](s3-person-detect-espnn-off.log)

## Probe 變更與驗收

Probe 保留 20 次固定輸入計時邏輯。它記錄明確設定的模型名稱、`CONFIG_IDF_TARGET`、IDF 版本及 arena；`AllocateTensors()` 錯誤訊息改為要求檢查前一個 kernel 錯誤，避免誤稱主要原因一定是 arena 或 operator。

相對 PR #9 的 `probe.cc` diff 為 13 insertions / 8 deletions，共 21 行變更，少於 30 行限制。

| 驗收 | 結果 | 證據 |
|---|---|---|
| ESP-IDF build 成功 | 通過 | ESP-NN on/off build 完成 |
| 指定 MobileNet on/off 各完成 20 次並產生數字 | 未通過 | 兩種設定都在 `AllocateTensors()` 失敗；沒有 `RESULT` |
| mean、p50、p95、ratio、arena | 部分完成 | 只對 int8 `person_detect` stand-in 有完整 A/B 數字 |
| 不改 timed loop | 通過 | 統計與 20-run `Invoke()` 區間不變 |
| 垃圾分類圖片、S31、相機至開蓋延遲 | 未涵蓋 | 此測試只在 ESP32-S3 跑零輸入模型 probe |

因此 Issue #6 的指定模型驗收仍未完成。不能用 `person_detect` 數字推論 Edge Impulse 模型或 ESP32-S31 的速度，也不能把單次 `Invoke()` 說成相機到開蓋的端到端時間。
