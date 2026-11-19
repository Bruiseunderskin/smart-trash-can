# ESP32-S3 virtual hand

Reader: hardware owner running the S31/S3 stage-one smoke test.

This firmware implements only the hand protocol. It does not initialize
PCA9685, servos, TCA9548A, or VL6180X.

## Build and flash

```sh
. "$IDF_PATH/export.sh"
idf.py -C firmware/esp32s3/virtual_hand \
  -B /root/stc-build/s3-virtual-hand \
  -D IDF_TARGET=esp32s3 build flash monitor
```

Use the USB Serial/JTAG console. Serial commands:

| Command | Effect |
|---|---|
| `drop 0`–`drop 3` | Override the bucket for the next OPEN_ALL |
| `pattern 1,0,3,0,2` | Set the default five-round pattern |
| `delay 1500` | Set simulation delay in milliseconds |
| `status` | Print delay, pattern and logical open state |
| `help` | Print commands |

Both boards use fixed ESP-NOW channel 6 and unencrypted frames for this
laboratory milestone. The S3 node must be powered independently and share
ground only where the external power design requires it.

The 16-byte wire `round` field is the low eight bits of the S31 game's
monotonic round ID. S3 echoes it and uses the OPEN command sequence on DROP;
S31 retains the full ID and rejects ACK/DROP from another round.

If the ESP-IDF USB Serial/JTAG console has already installed the driver, the
firmware reuses it instead of installing a second driver (which would abort at
boot). The ESP-IDF 6.1 S3 target build passed on 2026-11-11; flashing and
two-board RF behaviour were not exercised in that build check.

[Firmware index](../../README.md)
