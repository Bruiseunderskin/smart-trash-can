# ESP32-S3 TFLite Micro latency probe

This is a hardware probe for the `esp-tflite-micro` + ESP-NN path. It reports 20 `Invoke()` durations and tensor-arena use for a public int8 person-detection model. It does not include a camera, a product UI, or waste-classification accuracy.

## Requirements

- ESP-IDF 5.1 or newer; this project was prepared against the installed ESP-IDF 6.0.2.
- An ESP32-S3 board with PSRAM that can hold a 4 MiB arena.
- The public model at `main/models/waste4_int8.tflite` (download steps below).

## Prepare the model

```bash
cd esp32s3/tflite_probe
curl -L --fail --output main/models/waste4_int8.tflite \
  https://raw.githubusercontent.com/tensorflow/tflite-micro/main/tensorflow/lite/micro/models/person_detect.tflite
shasum -a 256 main/models/waste4_int8.tflite
# Expected: 808cfdfc0cf3a6fa6f6fa26bfa379ea97c16d5db7334637766e39c3408502e9d
ls -lh main/models/waste4_int8.tflite
```

The downloaded model is intentionally ignored by Git. It is a latency stand-in, not a four-class waste model.

## Build and run the ESP-NN-on measurement

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbserial-XXXX flash monitor
```

Capture the full `RESULT` line. Leave the default `TFLite probe → Enable ESP-NN optimized kernels` setting enabled for this first run.

## Build and run the ESP-NN-off comparison

```bash
idf.py menuconfig
# TFLite probe → Enable ESP-NN optimized kernels: n
idf.py build
idf.py -p /dev/cu.usbserial-XXXX flash monitor
```

Capture the second `RESULT` line. The run count and model remain the same. `p95` uses the nearest-rank definition over 20 samples. Calculate `ratio(p95)` as `esp_nn=off p95_us / esp_nn=on p95_us`; a value above 1 means ESP-NN was faster.

## Report

Record the actual board/module and PSRAM size from its markings or vendor specification, the `idf` version, model byte size, both `RESULT` lines, the p95 ratio, and whether `probe.cc` needed any compatibility edits. Do not report timing as physical evidence unless it came from the connected board's serial log. Do not commit the `.tflite` model.

The program logs the SoC target, IDF version, model size, CPU clock, available PSRAM, kernel setting, timing statistics, and arena bytes. It stops with an explicit error if PSRAM allocation, tensor allocation, or inference fails.
