# 2026-11-24 — S31 → real S3 hand integration

Reader: integrator checking whether the real hand path actually opened lids and used TOF.

## Setup

- S31: running `Esp32S31Korvo1Stc`; status showed `espnow=1 hand_online=1`.
- S3: ESP32-S3, MAC `90:e5:b1:d5:fd:88`, 32 MB octal flash, native USB Serial/JTAG on `COM8`.
- S3 firmware: `stc_game_hand`, active worktree `codex/s31-new-model-ui` at `fdaf58c` plus uncommitted diagnostics, built with ESP-IDF 6.0.2.
- Secondary USB Serial/JTAG console enabled for this diagnostic build.

## Bug found before capture

The active worktree had removed boot-time `stc_servo_lids_init()` and used lazy init on first PWM move. `s_channels_stopped` defaulted to `false`, so the first close/open called LEDC before initialization:

```text
E ledc: ledc_set_duty(1105): LEDC is not initialized
E SERVO_LIDS: set_pulse_us(59): set duty failed
```

The S3 still sent the command ACK, so S31 showed `lid_open=1` even though no servo pulse was applied.

Fix applied for this run: initialize `s_channels_stopped = true` so the first move runs `stc_servo_lids_init()`.

## Result

Raw log: `s31-to-real-hand.log`.

Observed sequence:

```text
TOF4: INIT ready=4/4
CONTROLLER_FOUND mac=1c:29:04:d0:58:a0
RX_CLOSE round=0 seq=45
SERVO_LIDS: CLOSE bin=0..3
RX_OPEN round=66 seq=46
SERVO_LIDS: OPEN bin=0..3
ALL_LIDS_OPEN round=66
BASELINE sensor=0..3
DETECTING round=66
TX_DROP round=66 seq=46 bin=1
DROP_ACK round=66 seq=46
SERVO_LIDS: CLOSE bin=0..3
ALL_LIDS_CLOSE round=66
```

The test proves:

- S31 reached the real S3 over ESP-NOW.
- All four TOF sensors initialized and produced baselines.
- S3 executed all four servo open/close calls.
- A TOF candidate produced a DROP and S31 accepted it.

Not yet proven:

- Visual confirmation that every lid reaches its mechanical endpoint.
- Controlled drop accuracy per bin with a real card; the captured DROP fired with return-rate change at the existing range.
- Correctness of ACK timing. The current S3 sends ACK before the servo action is applied, so `lid_open=1` is not a position feedback signal.

## Reproduction

Build and flash the active S3 `game_hand` build, then monitor its UART0 or enabled USB secondary console. On S31, use `stc status`; trigger a game round and confirm the markers above. Do not treat `TX_ACK` alone as proof of motion.
## Follow-up — staggered outputs and owner confirmation

The first integrated run exposed a transient four-servo power/contact symptom after an earlier all-at-once test. The real-hand firmware now staggers OPEN and CLOSE calls by 120 ms:

```text
OPEN bin=0
OPEN bin=1   (+120 ms)
OPEN bin=2   (+120 ms)
OPEN bin=3   (+120 ms)
```

Raw log: `s31-to-real-hand-staggered.log`.

The ESP32-S3 booted with `TOF4: INIT ready=4/4`, received S31 OPEN, wrote all four PWM outputs, captured all four TOF baselines, and closed all four outputs after the round. The hardware owner confirmed on 2026-11-24 that this build is fully working.

This still does not replace a controlled per-bin drop-accuracy matrix.
