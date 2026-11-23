# ESP32-S31 standalone game

Reader: firmware integrator or hardware owner. Read before flashing stage one.

This project is the S31 "brain" for the first milestone. It uses the Korvo-1
800×480 display, touch panel, local ES8389 speaker, deterministic replay images
or the SC101IOT live camera, and
`models/wordless-padded/wordless_s31.espdl`. S3 is a separate real-hand peer
over ESP-NOW. After a successful scan, S31 opens all four physical lids. S3
detects the selected bin with four VL6180X sensors, closes the lids after the
three-second result window, then returns the real DROP to S31.

## Build

```sh
. "$IDF_PATH/export.sh"
idf.py --preview -C firmware/esp32s31/game \
  -B /root/stc-build/s31-game \
  -D IDF_TARGET=esp32s31 build
```

The build must use ESP-IDF 6.1 and a build directory outside this worktree
when the Windows volume is low on free space.

CMake regenerates `game_replay_pack.bin` from
`data/mixed-card-object/wordless-v1/test-scenes/` on every configure, checks
its SHA-256, then embeds it with the model and PCM clips. The factory app
partition is 12 MB on the board's 16 MB flash.

## Runtime evidence

At boot the model validates its SHA-256/CRC, labels, and replay pack, then runs
all 40 replay images. A normal build permits game use only after
`REPLAY_SELFTEST=PASS correct=40 total=40`. The badge is generated from the
active flags, so release builds distinguish `REPLAY` from `LIVE CAMERA`, and a
dev build adds `DEV 39/40`.

The ESP-DL image preprocessor accepts normalization parameters in the uint8
range. This quantized model has an int8 input exponent of `-7` and was trained
with RGB values scaled to `0..1`, so the device path must use mean `0` and std
`255`. Using ImageNet mean/std or std `1` produces the wrong quantized input.

On 2026-11-12 the S31 Korvo-1 hardware loaded the model imported from
`main@d4440b3` (SHA-256 `233619c9...`, runtime CRC `6e1f6dec`) and reported the
correct labels, replay pack and input exponent. It scored 39/40; the only miss
was `B-19_003` (expected hazardous, predicted recyclable). The game therefore
remained blocked with `MODEL_SELFTEST`. The required 40/40 gate must not be
lowered; the remaining model miss must be resolved before a hardware pass.

The physical PLAY key is GPIO61 active-low. The same scan/continue action is
also available through the touch UI.

For camera hardware bring-up, build the same project with
`-DSTC_CAMERA_PROBE=ON` and
`SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.camera.defaults"`. This path
initializes SC101IOT DVP at 640×480/UYVY, captures one frame, converts it to
160×160 RGB, and runs the ESP-DL classifier without starting the game UI.

Live camera is now the default. The live path captures five frames,
classifies each through the embedded `models/wordless-padded/wordless_s31.espdl`,
and sends the normal five-frame aggregate to the controller. The replay path
remains available with `-DSTC_GAME_LIVE_CAMERA=OFF`. The model self-test gate
still applies.

`-DSTC_GAME_DEV_FLOW=ON` is an explicit development-only escape hatch for the
current 39/40 artifact. It changes the log to
`MODEL_SELFTEST=FAIL ... DEV_BYPASS=1`, shows `DEV 39/40` on screen, waits for
the S3 virtual hand, and automatically starts/scan/continues five rounds. The
default remains `OFF`; this option does not relax or redefine the release gate.

`-DSTC_GAME_TOUCH_BIN=ON` turns the firmware into the local touch game. It
keeps the SC101IOT preview running on the display, disables ESP-NOW and the S3
hand, hides the model answer after scanning, and shows four 200×92 touch
buttons for `可回收物`, `廚餘`, `有害`, and `其他`. The selected button enters
the existing game core as a DROP, then the learning page reveals the scanned
class, selected bin, correct bin and score. In touch mode the dev autopilot is
disabled, so the player controls START, SCAN, bin selection and NEXT. The normal
build keeps this option `OFF`; on-screen bin buttons are hidden while waiting
for the real S3 drop and stale touch-bin events are ignored.

The game UI is wrapped by a two-state outer shell: `GAME` and `CHAT`. The home
page offers `開始遊戲` and `問綠豆`; the first start shows the three-step intro,
then the scan-ready page auto-starts after three seconds. `問綠豆` renders the
v3 chat screens. If the voice backend is unavailable, the chat page explicitly
shows its offline state; `返回主頁` or the 60-second idle timeout returns to the
game. CHAT pauses game ticks and rejects any in-round entry.

The touch profile allows 5 seconds for the five-frame ESP-DL inference. The
first hardware run measured 1,451-1,573 ms; keeping the generic 1.5-second
timeout caused intermittent stale-result rejection even when all five frames
were valid. The ordinary game profile remains at 1.5 seconds.

```sh
idf.py --preview -C firmware/esp32s31/game \
  -B /root/stc-build/s31-game-dev \
  -D IDF_TARGET=esp32s31 \
  -D STC_GAME_LIVE_CAMERA=ON \
  -D STC_GAME_DEV_FLOW=ON \
  -D STC_GAME_TOUCH_BIN=ON \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.camera.defaults" \
  build
```

## Host and target checks

```sh
cmake -S firmware/common/test -B build/common-test -DCMAKE_BUILD_TYPE=Debug
cmake --build build/common-test -j
ctest --test-dir build/common-test --output-on-failure
python3 -m unittest tools.tests.test_game_replay tools.tests.test_stage1_layout -v
```

The normal completion log is `DEV_FLOW=COMPLETE`. The full S31/S3 evidence is
stored under `runs/camera-probe/2026-11-13/`.

## Boundaries

- Camera source: SC101IOT live camera by default; replay with
  `STC_GAME_LIVE_CAMERA=OFF`.
- Visual source: live camera frames in the normal hardware build.
- Hand result: ESP-NOW real S3 with four servos and four VL6180X sensors.
- Audio: six existing 16 kHz mono PCM clips, played locally.
- Cloud/xiaozhi: not initialized. The v3 CHAT screens and offline state are
  implemented, but the standalone voice backend is still unsupported.

[Firmware index](../../README.md) · [Main component](main/README.md)
