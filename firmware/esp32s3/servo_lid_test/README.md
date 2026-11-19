# Four-lid direct servo test (ESP32-S3 / ESP-IDF)

Use this standalone test to verify four lid servos connected directly to the
ESP32-S3. It does not use PCA9685, ESP-NOW, the TOF sensors, the camera, or the
S31 game.

## Wiring and mapping

| Bucket | Bin | Signal GPIO | Name |
|---|---:|---:|---|
| 0 | 0 | GPIO12 | recyclable |
| 1 | 1 | GPIO13 | food |
| 2 | 2 | GPIO14 | hazardous |
| 3 | 3 | GPIO15 | other |

Power the servos from a separate regulated 5 V supply with enough current for
the installed servos. Connect the external supply ground to ESP32-S3 ground.
Do not power servo motors from the ESP32-S3 3.3 V or 5 V pin.

## Safe test sequence

The firmware does not move any lid at boot. Open the monitor, then test one
bucket at a time:

```text
lid> open 0
lid> close 0
lid> open 1
lid> close 1
```

Do not use `openall` until each mechanism and its direction have been checked.
Stop immediately if a linkage binds or a servo buzzes continuously.

## Commands

| Command | Action |
|---|---|
| `open N` | Move bin N to its calibrated open pulse |
| `close N` | Move bin N to its calibrated close pulse |
| `angle N A` | Move bin N to angle A, where A is 0–180 |
| `pulse N P` | Set bin N to pulse width P, where P is 1000–2000 µs |
| `openall` | Move all bins to the open angle |
| `closeall` | Move all bins to the closed angle |
| `stop` | Stop all PWM outputs; servos may not hold position |
| `status` | Print the last commanded angle for every bin |
| `help` | List registered commands |

The current calibrated pulse mapping is:

```text
close = 1450 us
open  = 1575 us
```

Each bin has its own `close_us` and `open_us` entry in
`main/servo_lid_test.c`. Change one bin at a time only after physically
confirming its safe travel.

## Build and flash

```powershell
idf.py -C C:\smart_trash_can_espnow\firmware\esp32s3\servo_lid_test `
  -B D:\jack\codex-build\s3-servo-lid-test `
  -p COM9 -D IDF_TARGET=esp32s3 flash monitor
```
