# TOF050C × 4 bring-up (ESP32-S3)

P0 tool for Issue #26. It drives four TOF050C (VL6180X) sensors through one TCA9548A and implements the `stc_bin` contract.

## Wiring

| Signal | Connection |
|---|---|
| I²C SDA / SCL | GPIO 8 / GPIO 9, 400 kHz |
| TCA9548A | 0x70, 3.3 V, common ground |
| CH0–CH3 | TOF050C #0–#3, all left at 0x29 |
| TOF SHDN | tie high / AVDD |

The channel number is the `stc_bin_t` value: 0 recyclable, 1 food, 2 hazardous, 3 other. Use external I²C pull-ups if the modules do not provide them.

## Build and flash

Use ESP-IDF 6.0.2:

```sh
idf.py -C firmware/esp32s3/tof_bringup -B build/tof_bringup -D IDF_TARGET=esp32s3 build
idf.py -C firmware/esp32s3/tof_bringup -B build/tof_bringup -p YOUR_PORT flash monitor
```

CI and `tools/dev.py` compile this project with target `esp32s3`. A successful build does not prove sensor or geometry behaviour.

## Serial commands

- `tof info` — offset, crosstalk rate, last distance and fault bit per channel
- `tof stream on|off` — prints `tof,<ms>,<ch>,<dist_mm>,<fault>`
- `tof cal <ch> <known-mm>` — averages five readings, updates the part-to-part offset, reads it back and stores it in NVS
- `tof selftest` — probes and reads all four channels; retries initialization before declaring a fault
- `tof monitor [off|ms]` — arms the drop detector; a confirmed event prints `EVENT DROP ...`

## Calibration correction

VL6180X is not VL53L0X. Its official API does not provide `PerformRefSpadManagement` or implicit `PerformOffsetSimpleCalibration` calls. Factory SPAD/NVM calibration is loaded during initialization. This project preserves the factory part-to-part offset and crosstalk rate, then supports an installation-specific offset adjustment through register `0x024`; both values are persisted as `tof_ch<n>_offset` and `tof_ch<n>_xtalk`.

## Detection policy

The detector learns a median baseline from the most recent ten samples and requires three consecutive samples at least 25 mm closer, spanning at least 60 ms. The distance must return to within 15 mm of baseline within 500 ms. Readings during the first 300 ms after arming are ignored. If several bins confirm together, the largest distance drop wins. One event is emitted per monitor arm.

Servo open/close is intentionally unsupported in this phase. G1 material/geometry measurements are manual; collect the serial stream into [g1-results.csv](../../../docs/g1-results.csv).

[Upper guide](../README.md) · [Project entry](../../../README.md)
