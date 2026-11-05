# STC P1 validation

Target: ESP32-S31-Korvo-1 V1.1, `esp32s31`, ESP-IDF 6.1 on Windows `COM5`.
Test date: 2026-11-05.

The implementation uses the P1 stub vision/bin modules and a real
`stc_game`/`stc_voice` state machine. Stub labels are marked with
`src=STC_SRC_STUB`; no claim below treats a stub result as model accuracy.

## Result summary

| ID | Result | Evidence |
| --- | --- | --- |
| A | PASS | Five autonomous rounds completed; labels `0,1,2,3,0`; scores `10,5,15,10,20`; settlement reached without reset or hang. |
| B | PASS (software) | First local line queued-to-start `40 ms`; ten test lines played serially; interruption stopped after `204 ms` and `busy_after=0`. Acoustic quality is not human-rated. |
| C | PASS | `stc chat` during round 0 was rejected; `stc play` in CHAT logged `GAME_START rejected: CHAT active`; CHAT exit started no game until GAME was selected. |
| D | PASS | Whole GAME boot/five-round log contains the network gate block and no WiFi manager, HTTP/OTA, or MQTT connection. |
| E | **PENDING** | CHAT network activation and listening were exercised, but no human spoke a wake word and completed a question/answer. |
| F | PASS | CHAT returned to GAME at `88.687 s` after becoming active at `28.387 s`, about `60.3 s` without interaction. |
| G | PASS | Startup prints all four frozen contract versions; board `static_assert` rejects any header version drift at compile time. |

## A - five-round smoke

Command:

```text
tools/capture_stc_acceptance.py --port COM5 --seconds 155 \
  --output firmware/xiaozhi-esp32/main/boards/espressif/esp32-s31-korvo-1-stc/evidence/serial-cfg.log
```

Observed sequence:

```text
STUB_LABEL round=0 bin=0 conf=0.99 src=1
STUB_LABEL round=1 bin=1 conf=0.99 src=1
STUB_LABEL round=2 bin=2 conf=0.99 src=1
STUB_LABEL round=3 bin=3 conf=0.99 src=1
STUB_LABEL round=4 bin=0 conf=0.99 src=1
GAME_SETTLEMENT rounds=5 score=20
```

The random incorrect-drop path and timeout path are covered by the host test;
the live run exercised correct, incorrect, and correct outcomes and completed
all five rounds.

## B - local voice queue and interruption

The initial game line was observed at:

```text
STC_VOICE_LINE_START line=0 samples=52608 channels=2 queued_ms=40
```

`queued_ms` is measured from `stc_voice_say()` entry until the playback task
starts the line. It is not an acoustic timestamp. A notified playback task is
used instead of a polling delay, so the queue does not add a fixed 20 ms sleep.

The `stc voice` diagnostic enqueued ten lines, waited for each busy queue to
drain, then interrupted an active line:

```text
VOICE_SELFTEST sequential=10 result=complete
STC_VOICE_LINE_DONE line=0 start_ms=204 interrupted=1
VOICE_SELFTEST interrupt=1 busy_after=0
```

Only one playback task exists, so lines cannot overlap. The queue has eight
slots; rapid test submissions correctly reported `queue_full` and were retried
until all ten lines had played. PCM is mono 16 kHz/16-bit and is expanded to
stereo only inside the codec output interface.

## C - mode exclusion

During the first capture round:

```text
STC_CMD chat rejected: game not in SETTLEMENT
```

During a later active CHAT session:

```text
STC_CMD play
GAME_START rejected: CHAT active
```

Selecting GAME terminates the CHAT network gate before starting a new game.
The short gate log is in `evidence/serial-chat-gate.log`.

## D - GAME cloud isolation

Fresh boot:

```text
STC_NETWORK_GATE state=GAME action=block cloud_connections=0
```

The full five-round GAME log was checked for `WifiManager`, `HttpClient`, and
`MQTT` connection messages. None occurred before CHAT was explicitly selected.
CHAT then logged network activation, and returning to GAME logged:

```text
STC_NETWORK_GATE state=GAME action=stop
```

This proves both the initial zero-connection path and teardown on CHAT exit.

## E - CHAT human voice (pending)

CHAT entered correctly after the first settlement:

```text
STC_NETWORK_GATE state=CHAT action=start
State: activating -> idle
State: idle -> connecting
State: connecting -> listening
```

The same flow succeeded on a second CHAT entry after GAME had stopped the WiFi
station. There was no human listener/speaker test in this run, so real wake
word, ASR, LLM, and TTS acceptance remains **PENDING**. A successful network
state transition is not substituted for that evidence.

## F - CHAT idle timeout

The first CHAT entry reached active listening at approximately `28.387 s`.
No voice or reply kept the session alive, and the state machine emitted:

```text
STC_VOICE: CHAT idle timeout; returning to GAME
STC_NETWORK_GATE state=GAME action=stop
```

at `88.687 s`, a measured idle period of about `60.3 s`. A second game then ran
normally, proving that returning to GAME did not leave a stuck CHAT state.

## G - contract version

Startup emits:

```text
CONTRACT_VERSION voice=1.1.0 vision=1.0.0 bin=1.2.0 game=1.0.0 impl_vision=stub-1
```

The four contract headers expose major/minor/patch macros. The board uses
`static_assert` against the frozen versions, so a header contract change fails
the build instead of silently producing a firmware with mismatched interfaces.
The vision contract remains a stub implementation during P1.

## Automated and build checks

- `python -m unittest discover -s tools/tests -v`: **25/25 PASS**.
- Host `stc_game` test covers +10/-5/0, active-round PLAY rejection, five-round
  progression, timeout, settlement, and CHAT eligibility.
- Voice asset tests cover WAV validation and raw PCM conversion.
- Canonical ESP-IDF 6.1 S31 build: **PASS**.
- Final application image: `0x30c760` bytes, 23% free in the app partition.
- Final `xiaozhi.bin` SHA-256:
  `01b5a30a2497e781e44e193d23c66f5dc5f9abd28ec9480bc407b4665dde60a4`.
- Flash and hash verification on `COM5`: **PASS**.
- RGB indicator code runs for each phase, but visual color/current acceptance
  was not recorded by a human; use serial logs as the authoritative P1 evidence.

## Remaining limitations

- Human CHAT speech (E) is still required.
- Acoustic intelligibility, clicks, and speaker loudness were not human-rated.
- UI is intentionally not implemented for P1; phase/mode evidence is serial and
  LED only.
- Board-local PCM assets are generated with Edge TTS and must be regenerated if
  the phrase text changes.

## Evidence files

- [`evidence/serial-cfg.log`](./evidence/serial-cfg.log): two five-round games,
  C, D, F, first CHAT entry, timeout, second game, and re-entry.
- [`evidence/serial-chat-gate.log`](./evidence/serial-chat-gate.log): CHAT
  prevents a new game, then GAME teardown and restart.
- [`evidence/serial-five-rounds-voice.log`](./evidence/serial-five-rounds-voice.log):
  ten-line serial playback and interruption test.
- [`evidence/serial-final-boot.log`](./evidence/serial-final-boot.log): final
  image boot, contract versions, GAME network gate, and first playback timing.
- [`timings.csv`](./timings.csv): extracted queue, game, and timeout timings.
