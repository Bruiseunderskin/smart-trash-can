# main

Application component for the TOF bring-up tool.

- `tof_driver.*` — TCA9548A and VL6180X register access on ESP-IDF `i2c_master`
- `stc_bin.c` — `stc_bin` contract implementation, NVS calibration and fault mask
- `drop_detector.*` — hardware-independent baseline and drop-event logic
- `calibration.*` — signed offset calculation
- `main.c` — UART commands, stream output and event logging

[Upper guide](../README.md) · [Project entry](../../../../README.md)
