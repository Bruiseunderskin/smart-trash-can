# Issue #6 — ESP32-S3 上 esp-tflite-micro int8 延遲（ESP-NN A/B）

## 回報格式

```
board: ESP32-S3 (module marking not independently verified), 16 MB flash / 8 MB PSRAM detected
idf: v6.0.2
model: person_detect_int8, bytes=300568  (見下方「模型替換」)
esp_nn=on : mean=105222us p50=105216us p95=105232us arena_used=120988
esp_nn=off: mean=5073248us p50=5073236us p95=5073260us arena_used=82300
ratio(p95): 48.21x  (5073260 / 105232)
編譯改動: probe.cc 由 MobileNet 換成 person_detect 的算子表；量測邏輯未改（見 diff）
```

序列埠：`/dev/cu.usbmodem5C843268331`（燒錄 460800，console 115200）。
CPU 160 MHz，PSRAM Octal 40 MHz，模型 300,568 bytes，runs=20。

## 模型替換（重要發現）

Issue 指定下載的 `mobilenet_v1_1.0_224_quant.tflite` 是 **uint8** 模型（input/output tensor type = 3）。`esp-tflite-micro` 1.4.1 的 ESP-NN conv kernel 只接受：

- float32 input + float32 filter
- int16 input + int8 filter
- int8 input + int8/int4 filter

因此燒錄後 `AllocateTensors()` 直接失敗：

```
conv.cc Hybrid models are not supported on TFLite Micro.
Node CONV_2D (number 0) failed to prepare with status 1
E tflite_probe: AllocateTensors failed; check PSRAM and model operators
```

同一限制在 esp-tflite-micro **master** 的 `kernels/esp_nn/conv.cc` 仍然存在，不是版本舊的問題；也不是板子問題（同一塊板後續 ESP-NN on 正常跑完）。

為了完成 A/B，改用 TensorFlow Lite Micro 官方**全 int8** 模型 `person_detect.tflite`（96×96×1 灰階、2 類，300,568 bytes，算子：CONV_2D / AVERAGE_POOL_2D / MAX_POOL_2D / RESHAPE / FULLY_CONNECTED）。這樣 on/off 兩邊跑的是同一份 int8 網絡，比較才有意義。

結論：**舊式 uint8 TFLite 模型不能使用 ESP-NN 加速**；要吃 PIE/ESP-NN 必須先轉成 int8。這一點對 S31 路線同樣成立。

## 量測結果

| 設定 | mean (us) | p50 (us) | p95 (us) | arena_used (B) |
|---|---:|---:|---:|---:|
| ESP-NN on  | 105,222 | 105,216 | 105,232 | 120,988 |
| ESP-NN off | 5,073,248 | 5,073,236 | 5,073,260 | 82,300 |

- p95 加速比：**48.21×**
- 20 次連續推理的樣本非常集中（on 的 p95−p50 = 16 µs），代表量測受快取/排程噪音影響很小。
- ESP-NN on 的 arena 反而較大（120,988 vs 82,300），因為 ESP-NN 的 conv 需要額外 scratch buffer；預留 PSRAM 時要用 on 的數字。
- ESP-NN off 時 20 次 reference 推理約 101.5 s，連續佔用 CPU0 會觸發非致命 task watchdog（`task_wdt: Task watchdog got triggered ... IDLE0`），但 20 次全部完成並印出 RESULT；不影響數字。若日後在產品中使用 reference kernel，需要讓出 CPU 或縮短單次推論。

原始輸出：

```
I (1121) tflite_probe: target=esp32s3 idf=v6.0.2 model=person_detect_int8 bytes=300568 esp_nn=on runs=20 cpu_mhz=160 psram_bytes=8388608
I (3271) tflite_probe: RESULT esp_nn=on runs=20 mean_us=105222 p50_us=105216 p95_us=105232 arena_used=120988
```

ESP-NN off 的完整原始 RESULT 行未保存；上表是先前記錄的摘要，因此該測量需要重跑並保存完整輸出。

## 編譯改動與範圍

`esp32s3/tflite_probe/main/probe.cc`：

1. log 的 model 字串改為 `person_detect_int8`。
2. `MicroMutableOpResolver<5>` → `<7>`。
3. 新增 `resolver.AddMaxPool2D()`、`resolver.AddFullyConnected()`（person_detect 需要）。
4. 錯誤訊息由 `Failed to register MobileNet operators` 改為 `Failed to register model operators`。

相對 PR #9 的 probe 整體 diff 超過 30 行；包括 API 適配、PSRAM 配置及模型算子變更。因此原 issue 的少於 30 行限制未達成。20 次 run、mean/p50/p95 計算及 zero-fill 輸入用於本次量測。

A/B 開關：`CONFIG_TFLITE_PROBE_USE_ESP_NN`（`main/Kconfig.projbuild`，預設 y）。關閉時 `main/CMakeLists.txt` 對 esp-tflite-micro 加 `-UESP_NN`，因為該版本無 menuconfig 開關。

## 重現步驟

```bash
cd esp32s3/tflite_probe
source ~/.espressif/v6.0.2/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodem5C843268331 -b 460800 flash

# console 115200，等 RESULT
python3 - <<'EOF'
import serial, time
s = serial.Serial('/dev/cu.usbmodem5C843268331', 115200, timeout=1)
s.setDTR(False); s.setRTS(False); time.sleep(0.3); s.reset_input_buffer()
end = time.time() + 180
while time.time() < end:
    d = s.read(4096)
    if d: print(d.decode('utf-8', 'replace'), end='')
s.close()
EOF

# ESP-NN off
# 把 sdkconfig 的 CONFIG_TFLITE_PROBE_USE_ESP_NN 改成 n，再 build/flash/重複上面
```

## 對 S31 的含意

- ESP-NN 的實際加速在此 workload 是 48×（p95），值得繼續走 esp-tflite-micro 路線。
- 必須確保模型是 **int8**（input/output type 9）；uint8 模型在 ESP-NN 下直接 Prepare 失敗。
- 120,988 B 只適用於此模型與本次設定；S31 和四類模型需要獨立量測。
- 本次量的是 96×96 灰階小模型；换成 160×160 RGB 四分類模型後延遲會明顯上升，需在 S31 到手後用同一份 probe 重測。
