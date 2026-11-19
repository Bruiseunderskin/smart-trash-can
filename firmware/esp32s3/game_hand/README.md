# S3 game hand

This firmware runs one complete lid/drop round on the ESP32-S3 and exposes it
to the S31 over ESP-NOW:

1. S31 sends `OPEN_ALL` after the card scan completes.
2. All four servos open using `1575 us`.
3. After an 800 ms settle, the four VL6180X sensors capture an empty-bucket
   baseline.
4. Detection runs for 15 seconds.
5. The first confirmed drop selects that sensor's bin.
6. S3 immediately queues the selected bin as a real DROP frame. If detection
   happens during S31's 1.5-second open animation, transmission is deferred
   until 1.8 seconds after `OPEN_ALL` so the game is already in its drop window.
7. S31 sends a DROP ACK only after the game accepts the result. S3 retries up
   to three times at 300 ms intervals until that ACK arrives.
8. A detected drop keeps all lids open for another 3 seconds, then closes them
   using `1450 us`.
9. No drop within 15 seconds closes all lids; S31's 15-second game timeout
   records the round as no answer.
10. When the five-round game reaches its summary, S31 sends
    `OPEN_ALL_HOLD`. S3 opens all lids without starting a drop window and keeps
    them open for card removal. The next game's `START` first sends
    `CLOSE_ALL`, then begins scanning normally.

ESP-NOW runs on channel 6 without encryption. S31 is discovered automatically
from valid command frames, and S3 broadcasts a 250 ms real-hand heartbeat.
Heartbeat frames do not set `STC_HAND_FLAG_VIRTUAL`.

## Commands

| Command | Action |
|---|---|
| `start` | Open all lids and begin a local test round |
| `status` | Print phase, answer and remaining time |
| `close` | Close all lids and discard the current round |
| `stop` | Stop PWM output and reset the round |
| `simdrop 0..3` | Debug-only: inject one ToF DROP into the active transport round |
| `help` | List commands |

The console `start` command is a local hardware test path. It does not send a
DROP frame to S31.

## Hardware mapping

| Bin | Servo signal | TOF XSHUT |
|---:|---:|---:|
| 0 recyclable | GPIO12 | GPIO4 |
| 1 food | GPIO13 | GPIO5 |
| 2 hazardous | GPIO14 | GPIO6 |
| 3 other | GPIO15 | GPIO7 |

TOF I2C uses SDA GPIO8 and SCL GPIO9. Runtime addresses are `0x30` through
`0x33`.

Servo motors require a separate regulated 5 V supply with common ground.

## Build

```powershell
idf.py -C C:\smart_trash_can_espnow\firmware\esp32s3\game_hand `
  -B D:\jack\codex-build\s3-game-hand `
  -p COM9 -D IDF_TARGET=esp32s3 flash monitor
```
