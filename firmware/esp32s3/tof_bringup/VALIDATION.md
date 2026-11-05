# VALIDATION — Issue #26

Date: 2026-11-05, Asia/Shanghai

## Automated and build evidence

- Host `drop_detector` and offset-calculation tests: PASS
- ESP-IDF 6.0.2 `idf.py ... -D IDF_TARGET=esp32s3 build`: PASS
- Application image: `0x3bc60` bytes, 77% free in the 1 MB factory partition

## Hardware evidence

No ESP32-S3 board or TOF050C hardware was connected during implementation. COM5 identified as ESP32-S31 and was not flashed.

| Test | Status | Evidence required |
|---|---|---|
| H1 four sensors identified | PENDING | `tof info`, fault mask 0 |
| H2 100 mm stability | PENDING | 30 s stream, sigma <= 5 mm |
| H3 sample rate | PENDING | stream timestamps, each channel >= 10 Hz |
| H4 10-minute run | PENDING | full serial log, zero I2C errors |
| H5 hot-plug recovery | PENDING | fault transition and `tof selftest` recovery |
| H6 NVS calibration persistence | PENDING | `tof cal`, reset, repeated reading within 3 mm |

Do not mark PENDING rows as passed from compilation alone.
