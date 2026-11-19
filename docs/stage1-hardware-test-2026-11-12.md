# Stage one hardware test — 2026-11-12

Reader: firmware integrator or hardware owner. Read before claiming the staged
S31 game or S3 virtual hand is hardware-complete.

## Result

Both boards boot from the stage-one apps and the ESP-NOW heartbeat link is
discoverable. The five-round game did not run because the required S31 model
self-test is still below its 40/40 gate. This is a fail-closed result, not an
accepted hardware pass.

| Check | Result | Evidence |
|---|---|---|
| S31 flash | PASS | COM5, app at `0x10000`, esptool hash verified |
| S31 display/UI | PASS | Korvo-1 800×480 RGB panel and GT1158 initialized |
| S31 audio | PASS | ES8389 opened at 16 kHz; local PCM path ready |
| S31 model assets | PASS | `main@d4440b3`, CRC `6e1f6dec`, pack `232915`, seed `20261111`, labels OK |
| S31 model input | PASS | int8, exponent `-7`, shape `[1,160,160,3]` |
| S31 replay self-test | **FAIL 39/40** | `B-19_003`: expected hazardous, predicted recyclable |
| S3 flash | PASS | COM9, 32 MB octal flash configuration |
| S3 virtual hand | PASS | `status`, `pattern`, `delay`, `drop`, `help` |
| S3 stability | PASS | still responsive after 15 s idle; no reboot loop |
| ESP-NOW discovery | PASS | S31 discovered S3 MAC `90:e5:b1:d5:fd:88` on channel 6 |
| Five-round two-board smoke | NOT RUN | blocked by the S31 model gate |

App binaries used for this run:

```text
S31  432C9CEFC2971778E2DECEB37DD6442F85BB9818429F245012F098FB2C6D0E89
S3   78795089863D355982850269F816A0421B189FE9333CDE3D78A958C8B9726ED6
```

## Boards

| Role | Port | MAC |
|---|---|---|
| ESP32-S31 Korvo-1 | `COM5` | `1c:29:04:d0:58:a0` |
| ESP32-S3 virtual hand | `COM9` | `90:e5:b1:d5:fd:88` |

The S3 board has a 32 MB octal flash device and uses the UART console through
its CP210x bridge. It does not use the USB Serial/JTAG console.

## Model finding

The replay JPEGs are already 160×160 RGB and the classifier expects RGB values
scaled to `0..1`. ESP-DL's `ImagePreprocessor` receives uint8 channel values, so
the matching parameters are:

```text
mean = {0, 0, 0}
std  = {255, 255, 255}
```

The original firmware used ImageNet mean/std and scored 16/40. Changing only std
to `1` saturated the int8 input and scored 10/40. Inspecting the `.espdl`
metadata showed the actual input exponent is `-7`, confirming that std `255`
is the bit-width-correct mapping.

The newer `main@d4440b3` artefact
`233619c93f80888297a0a37c840fe422d70a779a4b421efe3d357dbf17a4ebd3` improves
the device result from 30/40 to 39/40. On the same old 40-image smoke set,
the newer `model-float32.onnx` host run is 40/40 and the newer
`model-int8.onnx` host run is 39/40 (misses `B-19_008`).

The previously tested checked-in artifact had CRC `9b31e4dc` and scored
30/40. A fresh ESP-PPQ quantization from the new float graph with 240 local
training images scored only 36/40 and was not flashed.

## S31 camera probe

A separate `STC_CAMERA_PROBE=ON` build was flashed to the S31 only for this
test. It does not initialize the game UI or change the production game path.

```text
sensor:       SC101IOT, PID 0xda4a
format:       640×480 UYVY
frame:        614400 bytes
model input:  160×160 RGB, model CRC 6e1f6dec
result:       CAMERA_PROBE=PASS class=3 conf=0.903182
```

The actual 160×160 RGB image sent to the model is stored at
`runs/camera-probe/2026-11-13/s31-model-input-160x160.png`; the raw serial
capture is beside it in `serial.log`. The frame contains a gray/obscured
surface rather than a card, so the class result is a pipeline smoke result,
not a valid card classification or an accuracy result.

With a can-pattern card held in view, two independent resets produced:

```text
run A: CAMERA_PROBE=PASS class=3 conf=0.991337
run B: CAMERA_PROBE=PASS class=3 conf=0.997835
```

The captured model inputs are
`runs/camera-probe/2026-11-13/s31-card-input-160x160.png` and
`s31-card-input-160x160-second.png`. Both images visibly contain the can
pattern on the right; the classifier returned `recyclable` both times. This
is a two-frame smoke test on one card, not a field accuracy measurement.

The same camera path was then linked into the game build with
`STC_GAME_LIVE_CAMERA=ON`. With the normal game UI, display and ES8389 audio
initialized, the hardware reported:

```text
stc_camera: SC101IOT stream on, 640x480 UYVY
LIVE_CAMERA_FRAME=0..4
LIVE_CAMERA_SMOKE=PASS frames=5/5
```

The live five-frame path therefore works alongside the normal S31 UI. The
replay self-test still reports `39/40`, so the game controller remains blocked
at `MODEL_SELFTEST`; this integration is not a release-game pass.

## Passing checks

```text
CTest:                    22/22 passed
Python replay/UI tests:   14/14 passed
LVGL native scenarios:    65 passed
```

The LVGL native set includes all stage-one states and verifies that the badge
changes with replay/live/dev flags.

## Dev autonomous five-round run

On 2026-11-13 the S31 and S3 boards completed the full first-stage loop with an
explicit development-only build:

```text
S31: STC_GAME_LIVE_CAMERA=ON, STC_GAME_DEV_FLOW=ON
MODEL_SELFTEST=FAIL correct=39 total=40 DEV_BYPASS=1
MODEL_SRC=S31 ESP-DL (PC only captured serial output)
S3: VIRTUAL_HAND, MAC 90:e5:b1:d5:fd:88
ESP-NOW: channel 6, five OPEN_ALL commands, five DROP messages,
         five CLOSE_ALL commands
DEV_FLOW=COMPLETE score=5 streak=0 rounds=5 correct=2 wrong=3
duration_ms=41309 ended_by_time=0
```

The S31 captured and classified five live frames per round, reached the
learning page after each matching OPEN ACK/DROP sequence, and stopped only at
the summary page. Logs are `runs/camera-probe/2026-11-13/game-dev-flow-s31.log`
and `game-dev-flow-s3.log`. This proves the virtual five-round game path; it is
not a model release pass.

The normal `STC_GAME_DEV_FLOW=OFF` replay target build also completed. The
release game remains blocked on the 40/40 self-test until the model is fixed;
real servos, lid positions and VL6180X drops are still not accepted.

## Touch-bin local game

The 2026-11-13 touch-game build adds continuous SC101IOT preview and replaces
the S3 hand with four operator-selected bin buttons. The S31 boot evidence
reports:

```text
Display initialized with resolution 800x480
MODEL_SELFTEST=FAIL correct=39 total=40 DEV_BYPASS=1
TOUCH_BIN_MODE=ON transport=local_ui hand=disabled
SC101IOT stream on, 640x480 UYVY
LIVE_CAMERA=PREVIEW_READY 640x480_UYVY
```

The boot log is `runs/camera-probe/2026-11-13/touch-bin-boot.log`. The model is
still a development bypass, but the game interaction is now manual: scan,
choose a bin on touch, read learning feedback, then press next. No S3 DROP is
used for scoring in this mode.

The first live scan exposed a timeout race rather than a model failure:

```text
LIVE_SCAN_RESULT valid=5 bin=0 conf=0.9927 agreement=5 infer_ms=1527
VISION_EVENT_APPLY result=-2 phase=1   # already back at PROMPT
```

Five-frame inference varied from 1,451 to 1,573 ms, crossing the generic
1,500 ms scan timeout. The touch-bin profile now gives SCAN 5,000 ms. A fresh
hardware run completed:

```text
LIVE_SCAN_RESULT valid=5 bin=0 conf=0.9963 agreement=5 infer_ms=1558
VISION_EVENT_APPLY result=0 phase=3
TOUCH_STATUS phase=8 truth=0 dropped=0 score=10
TOUCH_STATUS phase=1 round=1 score=10
```

Logs are under `runs/camera-probe/2026-11-14/`. The selected bin and score are
validated by the same game core; UART commands were used only to automate this
test run in parallel with the touchscreen action contract.

The next hardware run exposed `INVALID_TRANSITION`: the controller correctly
moved from `CLASSIFYING` directly to the hidden-answer `BIN_CHOICE` state, but
the UI transition table only accepted `RESULT_OK -> BIN_CHOICE`. The table now
also accepts the real controller paths and the timeout/retry paths
`SCAN_READY -> LEARNING`, `CAPTURING -> SCAN_READY`, and
`CAPTURING -> LEARNING`.

```text
VISION_EVENT_APPLY result=0 phase=3
TOUCH_STATUS phase=5 round_id=2 truth=0 dropped=4 score=0   # BIN_CHOICE
TOUCH_STATUS phase=8 truth=0 dropped=0 score=10             # LEARNING
TOUCH_STATUS phase=1 round=1 score=10                       # next PROMPT
```

No `UI rejected`, `INVALID_TRANSITION`, panic or abort line appeared in
`runs/camera-probe/2026-11-14/touch-transition-fixed.log`.

## GAME / CHAT outer shell

The S31 application now has a two-state outer shell around the game. `GAME`
owns the existing controller; `CHAT` pauses controller ticks and has a
60-second exit timeout. The navigation is:

```text
主頁 -> 分類圖鑑 -> 問綠豆 -> CHAT
CHAT -> 返回遊戲 / PLAY / 60 s timeout -> GAME
```

The previous mode-gate contract and PR #41 display work were reviewed, but
neither was copied wholesale: the old six-state gate directly owned
`stc_game_start()`, while the current game controller already owns that
lifecycle. The new shell delegates to hooks and leaves the game core intact.

Hardware evidence:

```text
SHELL_CHAT_REJECT phase=1          # entering CHAT during an active round
TOUCH_STATUS shell=chat phase=0    # idle -> CHAT
CHAT_BACKEND=UNAVAILABLE error=-6
SHELL_MODE=game                    # manual return
TOUCH_STATUS shell=game phase=0
```

Log: `runs/shell-mode/2026-11-14/chat-guard.log`. This verifies the outer state
machine only. The xiaozhi voice/chat backend is not connected; CHAT currently
shows that explicitly.

## Not accepted in this stage

- The normal game firmware does not initialize the camera; the separate
  camera probe does. No PCA9685, servo, TCA9548A or VL6180X was initialized.
- No real lid movement or real DROP detection is claimed.
- The dev-only S31/S3 virtual five-round loop passed. A release game remains
  blocked until the model passes the S31 40/40 self-test.
