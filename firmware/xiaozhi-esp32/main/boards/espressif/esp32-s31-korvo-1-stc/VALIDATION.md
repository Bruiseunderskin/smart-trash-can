# STC game/chat shell validation

Target: ESP32-S31-Korvo-1 V1.1, `esp32s31`, ESP-IDF 6.1 on Windows `COM5`.
Test date: 2026-11-05, updated 2026-11-06.

## Requested behavior

- Keep XiaoZhi locked in GAME: no WiFi/protocol connection and no voice input.
- Show an intentionally blank GAME page until the team provides game logic.
- Switch through two touch buttons on screen.
- In CHAT, release XiaoZhi and start its native voice path.
- Replace XiaoZhi's native display with a board-owned CHAT page.
- Show a larger/bolder STT/TTS subtitle, network state, and XiaoZhi DeviceState.
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
| Self-owned CHAT UI | PASS | `StcChatDisplay` logs `native_xiaozhi_ui=0`; the shell creates a dedicated CHAT page with `chat_page=1`. |
| Runtime status display | PASS | Live status log moved through `网络连接中`, `网络已连接`, `连接中 · connecting`, `聆听中 · listening`, `回复中 · speaking`, then back to `待命 · idle`. |
| Subtitle event routing | PASS | Real STT/TTS events reached the custom UI as `user`/`assistant` messages, including a multi-sentence assistant reply. |
| Human voice answer | PASS | Real speech produced combined STT/TTS turns and returned audio while the custom status changed `listening -> speaking -> listening`. |
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

## Voice deadlock fix

The first Listening implementation initialized AFE before assets loaded and used
manual-stop mode. The serial trace showed:

```text
MODEL_LOADER: Can not find model in partition table
AfeAudioEngine: Initialized FD AFE, detector: none
AudioService: Ignoring speech model replacement after audio engine initialization
State: connecting -> listening
```

`StartListening()` selected `kListeningModeManualStop`, so speech never sent the
server-side end-of-utterance signal. The fix:

1. Do not initialize WakeNet from `stc_voice_set_mode(CHAT)`; wait until Assets
   load the SR models.
2. Start CHAT through `Application::ToggleChatState()` so the default auto/VAD
   listening mode is used.

The fixed trace shows the model loaded first and then a live conversation:

```text
MODEL_LOADER: Successfully load srmodels
AfeAudioEngine: Model 0: wn9_nihaoxiaozhi_tts
AfeAudioEngine: Initialized FD AFE, detector: WakeNet
State: connecting -> listening
Application: >> 你好，小智。
State: listening -> speaking
Application: << 哈喽～我在呢，今天过得怎么样呀？
```

## UI replacement contract

- The persistent bottom bar and touch callbacks are in `stc_shell_ui.cc`.
- The content viewport ends above the 94 px mode bar; subtitles never share
  that area with the touch buttons.
- `stc_chat_display.cc/.h` bridges XiaoZhi display events without constructing
  the native LCD page.
- The whole GAME page body is built by `stc_game_placeholder_build()` in
  `stc_game_placeholder.cc`.
- The game team can replace only that builder/module when their UI arrives.
- The future CHAT visual redesign can replace `stc_shell_ui.cc` while preserving
  the small public update API in `stc_shell_ui.h`.

## Automated checks

- `python -m unittest discover -s tools/tests -v`: **26/26 PASS**.
- New app FSM test: pass.
- ESP-IDF 6.1 canonical S31 build: pass.
- Final application image: `0x32b690` bytes, 20% free.
- Final `xiaozhi.bin` SHA-256:
  `99bba5aa78990e3faf11af75cf0437afc2488eacd55e2390d3c2fb79f6764aca`.
- Flash and hash verification on `COM5`: pass.

## Evidence

- [`evidence/serial-shell-ui.log`](./evidence/serial-shell-ui.log): shell object
  creation, GAME network lock, contract versions.
- [`evidence/serial-chat-ui-boot.log`](./evidence/serial-chat-ui-boot.log):
  current image boots the custom page and logs `native_xiaozhi_ui=0`.
- [`evidence/serial-chat-ui-runtime.log`](./evidence/serial-chat-ui-runtime.log):
  current custom CHAT page, network/DeviceState updates, live STT/TTS subtitle
  routing, and return to GAME.
- [`evidence/serial-shell-fsm.log`](./evidence/serial-shell-fsm.log): first CHAT,
  60 s timeout, second CHAT, and return to GAME.
- [`evidence/serial-chat-debug-stats.log`](./evidence/serial-chat-debug-stats.log):
  successful live microphone, VAD, ASR, TTS, and second-turn trace.
- [`evidence/serial-chat-debug-before.log`](./evidence/serial-chat-debug-before.log):
  failed trace proving the pre-fix `detector: none` initialization order.
- [`evidence/serial-chat-debug-after.log`](./evidence/serial-chat-debug-after.log):
  intermediate trace proving the model-loading order was fixed while manual-stop
  listening still produced no reply.
- [`evidence/serial-chat-debug-final.log`](./evidence/serial-chat-debug-final.log):
  intermediate trace proving automatic VAD reached `listening`; no human speech
  was entered during this short capture.
- [`evidence/serial-final-boot.log`](./evidence/serial-final-boot.log): earlier
  final boot of the previous game-flow build.

The files named `serial-cfg.log`, `serial-chat-gate.log`,
`serial-five-rounds-voice.log`, and `serial-autonomous-5-rounds.log` are retained
as historical evidence from the previous automatic five-round build. They do
not describe the current blank GAME page.

## Remaining hardware checks

- Confirm final typography spacing and touch ergonomics on the production
  enclosure after the teammate's visual design replaces this placeholder.
