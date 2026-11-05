# STC game/chat shell validation

Target: ESP32-S31-Korvo-1 V1.1, `esp32s31`, ESP-IDF 6.1 on Windows `COM5`.
Test date: 2026-11-05.

## Requested behavior

- Keep XiaoZhi locked in GAME: no WiFi/protocol connection and no voice input.
- Show an intentionally blank GAME page until the team provides game logic.
- Switch through two touch buttons on screen.
- In CHAT, release XiaoZhi and start its native voice path.
- Keep the game UI replacement isolated from networking and XiaoZhi.

## Results

| Check | Result | Evidence |
| --- | --- | --- |
| Outer mode FSM | PASS | Host test covers `GAME -> CHAT_STARTING -> CHAT_READY -> GAME` and invalid/duplicate transitions. |
| Touch shell creation | PASS (software) | Boot logs `STC_SHELL_UI ready game_button=1 chat_button=1`; LVGL top-layer objects are created without runtime assertion. |
| Physical touch press | **PENDING** | UI and GT1151 initialize, but no human physically tapped the screen during this automated run. |
| GAME cloud lock | PASS | Boot logs `STC_NETWORK_GATE state=GAME action=block cloud_connections=0`; no WiFi/OTA/MQTT initialization occurs before CHAT. |
| First CHAT entry | PASS | Serial request reached `CHAT_STARTING`, then `CHAT_READY`, then `State: connecting -> listening`. |
| CHAT re-entry | PASS | After 60 s timeout returned to GAME, CHAT started again and reached `CHAT_READY` without a protocol-not-initialized race. |
| Return to GAME | PASS | CHAT request reached ready, then `STC_NETWORK_GATE state=GAME action=stop` and `APP_STATE CHAT -> GAME`. |
| Human voice answer | **PENDING** | Network and native XiaoZhi Listening are active, but no person completed wake-word/ASR/LLM/TTS QA. |
| Game UI | INTENTIONAL PLACEHOLDER | `stc_game_placeholder.cc` displays a dark empty page and replacement hint only. |

## State machine

`stc_app_fsm.c` is independent of LVGL and `kDeviceState`:

```text
GAME -> CHAT_STARTING -> CHAT_READY -> GAME
```

The touch callback writes only a requested mode. The board task applies voice,
network, LED, and UI changes in this order. CHAT does not call
`stc_voice_start_dialog()` until the upstream network activation has returned
to `kDeviceStateIdle`; this prevents starting Listening before protocol
initialization.

## UI replacement contract

- The persistent bottom bar and touch callbacks are in `stc_shell_ui.cc`.
- The whole GAME page body is built by `stc_game_placeholder_build()` in
  `stc_game_placeholder.cc`.
- The game team can replace only that builder/module when their UI arrives.
- The shell adds children to `lv_layer_top()`, so the buttons remain visible
  over XiaoZhi's own screen while GAME covers the XiaoZhi UI.

## Automated checks

- `python -m unittest discover -s tools/tests -v`: **26/26 PASS**.
- New app FSM test: pass.
- ESP-IDF 6.1 canonical S31 build: pass.
- Final application image: `0x30c4f0` bytes, 23% free.
- Final `xiaozhi.bin` SHA-256:
  `ed858f8383158f749e28d1186b494380f4b075ec8ba232cba4e95ce73207097d`.
- Flash and hash verification on `COM5`: pass.

## Evidence

- [`evidence/serial-shell-ui.log`](./evidence/serial-shell-ui.log): shell object
  creation, GAME network lock, contract versions.
- [`evidence/serial-shell-fsm.log`](./evidence/serial-shell-fsm.log): first CHAT,
  60 s timeout, second CHAT, and return to GAME.
- [`evidence/serial-final-boot.log`](./evidence/serial-final-boot.log): earlier
  final boot of the previous game-flow build.

The files named `serial-cfg.log`, `serial-chat-gate.log`,
`serial-five-rounds-voice.log`, and `serial-autonomous-5-rounds.log` are retained
as historical evidence from the previous automatic five-round build. They do
not describe the current blank GAME page.

## Remaining hardware checks

- Physically tap both screen buttons in GAME and CHAT.
- Confirm visually that GAME fully covers XiaoZhi, while CHAT shows the native
  XiaoZhi page with the bottom mode bar still available.
- Perform one real spoken wake-word question and listen to the answer.
