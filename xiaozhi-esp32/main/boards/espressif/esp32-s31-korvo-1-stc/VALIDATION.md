# STC text-chat validation

Validated on an ESP32-S31-Korvo-1 over `COM5` with ESP-IDF 6.1 on 2026-11-04.
The active transport was MQTT, not WebSocket.

## Result summary

- A1 UART `ask` / `say`: pass for inputs within the server's text limit.
- A2 C API: the same two functions are exercised through UART and link into the
  firmware successfully.
- A3 phrase table: pass. `right` and `question` were read from the packed asset;
  a missing key returned the expected empty default.
- Idle / Listening / Speaking / three serialized turns: pass in three repetitions.
- TTFB target: pass for successful calls. S P95 `571 ms`; Q P95 `592 ms`.
- Full 30-sample acceptance: **fail**. Only 15/30 produced a response because
  the official server rejects long `listen/detect.text` values.
- True S TTS-only path: **not available**. The official protocol exposes no
  device-to-server TTS-only message, so S remains a constrained-Q fallback.
- Q response length at most 40 Chinese characters: **fail**. Only 2/6 successful
  Cantonese-Q replies were at most 40 Unicode characters.
- Human intelligibility score: **not rated**. No human listener was present, so
  this report does not claim a 1-5 audio quality score.

## Configuration evidence

```text
CONFIG_SEND_WAKE_WORD_DATA=y
# CONFIG_STC_MCP_CAMERA is not set
CONFIG_CAMERA_SC101IOT=y
# CONFIG_CAMERA_OV3660 is not set
CONFIG_FLASH_DEFAULT_ASSETS=y
```

SC101IOT remains initialized locally while MCP camera exposure is disabled:

```text
I (...) sc101iot: Detected Camera sensor PID=0xda4a
I (...) EspVideo: Camera init success
```

No `self.camera.take_photo`, image capture, or image upload appeared in the
post-change 30-sample run.

## Measurement definitions

- TTFB: `submit_to_speaking_ms` from request enqueue (`t0`) until the device
  observes `kDeviceStateSpeaking`, which follows the server `tts start` signal.
- Playback: from the first observed speaking transition until decode/playback
  queues and in-flight output report drained. This is not an I2S sample timestamp.
- P95 uses nearest-rank over successful samples only.
- The issue requested 30 samples, so the table contains exactly five categories
  with six entries each. Failures remain in the denominator and are not dropped
  from the result summary.
- `NR` means intelligibility was not scored by a human listener.

The first implementation appended a long reply constraint and spent 20 seconds
in a timeout after the server returned:

```text
Application: Alert [sad] ERROR: Detect is only for wake words, do not send long texts.
```

The final implementation compresses Q to `40字：` (4 characters) and S to
`照讀：` (3 characters), then rejects requests whose full outbound text exceeds
the observed 16-Unicode-character server limit before opening a network turn.

## 30-sample table

| # | Category | Mode | Input | TTFB ms | Playback ms | Outcome | Audio |
| ---: | --- | --- | --- | ---: | ---: | --- | --- |
| 1 | short S | S | 答對喇 | 393 | 4300 | ok; phrase plus extra | NR |
| 2 | short S | S | 答錯喇 | 571 | 5050 | ok; phrase plus extra | NR |
| 3 | short S | S | 請放入廚餘桶 | 518 | 4045 | ok; phrase plus extra | NR |
| 4 | short S | S | 請放入可回收物桶 | 551 | 8150 | ok; explanation, not read-only | NR |
| 5 | short S | S | 請將卡片放入任何一個桶 | 532 | 4150 | ok; semantic paraphrase | NR |
| 6 | short S | S | 第3題答對 | 554 | 2799 | ok; exact phrase | NR |
| 7 | long S | S | 鋁罐長句 | - | - | `server_text_limit` | NR |
| 8 | long S | S | 紙巾長句 | - | - | `server_text_limit` | NR |
| 9 | long S | S | 玻璃樽長句 | - | - | `server_text_limit` | NR |
| 10 | long S | S | 廚餘長句 | - | - | `server_text_limit` | NR |
| 11 | long S | S | 電池長句 | - | - | `server_text_limit` | NR |
| 12 | long S | S | 答題完成長句 | - | - | `server_text_limit` | NR |
| 13 | Cantonese Q | Q | 呢張卡係咩垃圾 | 542 | 7800 | ok; relevant, 27 chars | NR |
| 14 | Cantonese Q | Q | 點解電池唔可以當普通垃圾 | 549 | 19950 | ok; relevant, 78 chars | NR |
| 15 | Cantonese Q | Q | 廚餘應該放入邊個桶 | 592 | 9100 | ok; relevant, 41 chars | NR |
| 16 | Cantonese Q | Q | 鋁罐同鐵罐要分開回收嗎 | 591 | 9650 | ok; relevant, 39 chars | NR |
| 17 | Cantonese Q | Q | 呢張卡濕咗仲可唔可以回收 | 576 | 11695 | ok; relevant, 52 chars | NR |
| 18 | Cantonese Q | Q | 玻璃碎咗應該點處理 | 476 | 18400 | ok; relevant, 80 chars | NR |
| 19 | mixed | Q | 呢個係 Paper，應該入邊個桶？ | - | - | `server_text_limit` | NR |
| 20 | mixed | S | 第 3 題答對 | 569 | 3000 | ok; exact phrase | NR |
| 21 | mixed | Q | 呢張係 PET 膠樽，可以回收嗎？ | - | - | `server_text_limit` | NR |
| 22 | mixed | Q | 電池係 Hazardous waste 嗎？ | - | - | `server_text_limit` | NR |
| 23 | mixed | S | 請講出 Blue bin 入面應該放咩 | - | - | `server_text_limit` | NR |
| 24 | mixed | Q | 呢張卡 QR code 睇唔清，點算？ | - | - | `server_text_limit` | NR |
| 25 | boundary | Q | empty | - | - | `empty_text` | NR |
| 26 | boundary | Q | Which bin should paper go in? | - | - | `server_text_limit` | NR |
| 27 | boundary | S | 1234567890 | 520 | 5150 | ok; digits converted, extra remark | NR |
| 28 | boundary | Q | ?!,.;:...!!? | 20031 | 0 | server rejected punctuation input | NR |
| 29 | boundary | S | 😀♻️🗑️ | 371 | 7650 | ok; described emoji instead of reading | NR |
| 30 | boundary | Q | 200-char repeated Chinese text | - | - | `server_text_limit` | NR |

Successful-sample statistics:

| Mode | Successful n | Failed n | Median TTFB ms | P95 TTFB ms | Max TTFB ms | Median playback ms | P95 playback ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| S | 9 | 7 | 532 | 571 | 571 | 4300 | 8150 |
| Q | 6 | 8 | 563 | 592 | 592 | 10673 | 19950 |

Of the nine successful S calls, two returned the requested phrase exactly and
six returned the phrase with extra conversational text. The emoji boundary was
described rather than read. This confirms that S is a fallback and not a
faithful TTS-only mode.

## State-machine matrix

Each row was repeated three times against the final firmware. A separate burst
test sent three queued phrases at one-second intervals three times.

| Starting state | Injected requests | Passes | Result |
| --- | ---: | ---: | --- |
| Idle | 3 | 3/3 | Pass |
| Speaking | 3 | 3/3 | Pass; previous TTS aborted |
| Listening | 3 | 3/3 | Pass; previous channel closed then new request sent |
| Three serialized turns | 9 | 3/3 bursts | Pass; ids completed in order, no overlapping playback |

Observed completion sequence for the three state rows:
`1,2,3`, `4,5,6`, `7,8,9`. Three queued bursts completed
`10,11,12`, `13,14,15`, and `16,17,18`.

## Phrase-table runtime check

```text
StcChat: PHRASE key=right value=答對喇
StcChat: PHRASE key=missing value=
StcChat: PHRASE key=question value=呢張卡係咩垃圾
```

The empty value for `missing` used `fallback=nullptr`; a game caller receives
its own non-null fallback instead.

## Latency decomposition

Because successful TTFB is already below 2.5 seconds, no temporary
packet/decode/I2S instrumentation was left in core files. The representative
Q timeline below comes from board/app logs:

| Point | Log time ms | Delta from t0 | Meaning |
| --- | ---: | ---: | --- |
| t0 | 5897 | 0 | request enqueued |
| t1 | 5917 | +20 | `WakeWordInvoke` submit call |
| - | 6067 | +170 | audio channel/listening ready |
| - | 6187 | +290 | server echoed `stt` text |
| t2 proxy | 6367 | +470 | server `tts start` / speaking |
| first sentence | 7217 | +1320 | first subtitle sentence |
| t4 proxy | 24777 | +18880 | playback queues drained |

The transport does not expose `t2` first packet, `t3` decode completion, or `t4`
I2S start through the board-only API. Those values are therefore not presented
as measured facts.

Hypothesis results:

- H1 VAD wait: not dominant; `stt` appeared 290 ms after submission.
- H2 ASR: no ASR delay observed; server echoed the submitted prefixed text.
- H3 LLM/TTS serialization: `stt` to `tts start` was about 180 ms on this run.
- H4 playback buffering: first subtitle arrived 850 ms after `tts start`;
  audibility was not independently measured.
- H5 Opus decode: not isolated because a listener timestamp was unavailable.
- H6 network RTT: five ICMP probes to `47.76.65.170` averaged 23 ms,
  min 18 ms, max 26 ms, 0% loss.

The dominant remaining failure is the server's short-text validation, not
device/network latency.

## Final smoke test

After adding the read-only `phrase` diagnostic, the final firmware still
completed Q and S:

```text
Q: ttfb_ms=375 playback_ms=20550 total_ms=20925 ok=1
S: ttfb_ms=584 playback_ms=5350 total_ms=5934 ok=1
```

## Build and automated checks

- Canonical board build: pass, ESP-IDF 6.1, `esp32s31`.
- Final application image: `0x30b6e0` bytes, 23% free in the app partition.
- Flash and boot on `COM5`: pass.
- Focused build-selection/config/assets tests: 36/36 pass.
- `clang-format --dry-run -Werror`: pass.
- `generated_assets.bin` contains both `phrases.json` and the `open` phrase.
- Full 97-test suite was also attempted. It has 1 pre-existing board-menu
  ordering failure and 11 environment errors from Windows temporary-directory
  cleanup and missing Bash-compatible CI test tooling; these are not in the
  changed board path.

## Remaining limitations

- The official server has no device-to-server TTS-only command in the current
  protocol. S cannot guarantee exact read-only output.
- The server does not honor the four-character `40字：` prompt consistently;
  only 2/6 successful Q replies were at most 40 characters.
- Long inputs cannot use this `listen/detect.text` path. A proper server-side
  text-turn/TTS API is required for arbitrary 200-character input.
- No human listened to the speaker during these samples.
- A separate spoken wake-word regression was not rerun after the final flash;
  the relevant core/audio/protocol files remain unchanged.

## Historical issue #23 P1

The P1 trigger is a long press of the SET button. It passes the question through
the existing `Application::WakeWordInvoke()` path, which emits the unchanged
`listen/detect` payload. Short-press, voice interaction, and the official board
directory are not modified.

### Historical P1 result

Functional result: **pass**. The official server treated the arbitrary text as
the user's message, emitted `stt` text equal to the question, invoked the
language model, and returned a response relevant to classifying a card. It did
not return a fixed wake response.

| Run | Speaker response | First-audio latency |
| ---: | --- | ---: |
| 1 | Asked for more card information after camera failure | 2.60 s |
| 2 | Asked the user to describe or retake the card | 9.15 s |
| 3 | Asked for card text or imagery | 4.97 s |
| 4 | Asked what kind of card it was | 1.69 s |
| 5 | Asked whether it was paper, plastic, or metal | 3.43 s |
| 6 | Asked the user to bring the card closer | 3.00 s |
| 7 | Asked what text or image was on the card | 3.92 s |
| 8 | Asked whether it was paper, plastic, or metal | 4.06 s |
| 9 | Asked the user to retry the photo | 3.03 s |
| 10 | Asked what image or text was on the card | 4.52 s |

Median: 3.68 s. P95: **9.15 s**. Acceptance target: at most 2.5 s.

Latency result: **fail** for this question. Camera-tool failure dominated the
variance in the ten-run measurement. This measurement used the first normal
audio packet after the question.

### Historical SC101IOT camera validation

After changing the variant to SC101IOT only at 640x480 YUV422:

```text
I (...) sc101iot: Detected Camera sensor PID=0xda4a
I (...) EspVideo: Camera init success
I (...) EspVideo: mmap_buffers_[buf.index].length = 614400, frame.width = 640, frame.height = 480
I (...) EspVideo: Explain image size=614400 bytes, compressed size=16759
```

One server-driven `self.camera.take_photo` request captured and uploaded an
image successfully. The image was too dark to classify, and the server asked the
user to retake it. Query-to-first-audio for that run was 9.86 s, so the camera
fix does not by itself meet the 2.5 s target.

### Historical P2 - protocol payload variants

Not run because P1 passed its functional gate. Additional payload variants are
only needed if the official server ignores arbitrary `detect.text` input.

### Historical P3 - fallback

Not reached. P1 was accepted functionally, but the latency acceptance criterion
fails. Use B1 (self-hosted HTTP streaming) or make image classification local
when latency below 2.5 s is required.

### Historical remaining checks

- Spoken wake word through ASR, LLM, and TTS.
- Ten-run latency after removing the camera round trip or classifying locally.
- Camera image quality under production lighting.
