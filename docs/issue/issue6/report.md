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

該錯誤字串是 kernel 的通用型別檢查訊息，單憑它不能證明模型必然是 hybrid：本次 component 的 `conv.cc` prepare 只接受 float32/float32、int16/int8 及 int8/int4 或 int8/int8 組合，並未列出 uint8 input/filter 組合。重新從報告所列 Gitiles mirror 下載後，SHA-256 與實測檔一致。直接解析 FlatBuffer：第 0 個 operator 是 `CONV_2D`，輸入 tensor 88 為 `UINT8 [1,224,224,3]`，filter tensor 8 為 `UINT8 [32,3,3,3]`，bias tensor 6 為 `INT32 [32]`。因此本次第一層的 uint8/uint8 組合不在此 kernel 的允許清單；錯誤訊息本身不能證明它是 hybrid。同一版本的一般 reference `conv_common.cc` 也只列出相同的允許型別，`conv.cc` 沒有 uint8 卷積分支；單純切換到 reference kernel 不能補上 uint8 支援。

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

## EI 四類實圖 smoke test

ESP32-S3、160 MHz、ESP-IDF 6.0.2、ESP-NN 啟用、8 MB PSRAM。四張經 host LiteRT 核對的 int8 影像 tensor，每類一張，每次 Invoke 前重新載入輸入。四張各執行 20 次，最後輸出全部符合真值；p95 分別為 food **858.187 ms**、hazardous **858.157 ms**、other **858.196 ms**、recyclable **858.141 ms**，arena 使用 507,836 B。這是選定樣本的 smoke test，不是準確率估計；不包含資料複製、相機、預處理、顯示或開蓋。

先前 probe 只在迴圈前載入一次輸入，最後四張均判為 other；修正後結果上列，因此舊結果不能用來判定 runtime 不相容。ESP-NN 關閉的診斷在首個 Invoke 執行逾 92 秒並出現 watchdog 訊息後中止，沒有完成時間。

240 MHz 同板補測：EI 四張各 20 次，最後輸出皆正確，p95 分別為 684.019、684.003、684.085、684.006 ms；arena 507,836 B。記錄見 [240 MHz 摘錄](ei-s3-real-input-240mhz.log)。不包括複製輸入、相機、前處理、畫面及開蓋。
