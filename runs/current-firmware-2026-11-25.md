# Current S31 and S3 firmware provenance - 2026-11-25

Reader: firmware integrator or reviewer asking which source and binary are
currently on the two physical boards.

The tracked source for both targets is at repository commit
`7dbedaa8300b2dd60707574bf9cc063af2282a9a` on
`codex/s31-new-model-ui`. Build directories are intentionally not committed;
the hashes below identify the exact flashed artifacts.

## S31 - Korvo-1 board

| Item | Value |
|---|---|
| Role | display, camera, local Edge TTS PCM, SFX, game state, ESP-NOW controller |
| Target | `esp32s31` |
| Board source | `firmware/xiaozhi-esp32/main/boards/espressif/esp32-s31-korvo-1-stc/` |
| UI source | `ui/lvglpro/src/stc_ui.c`, `ui/lvglpro/src/stc_chat_ui.c` |
| Voice/SFX | `firmware/assets/voice/`, `firmware/assets/sfx/` |
| Last source changes | board + voice `9ad6f2d`; UI + SFX `e41340a` |
| IDF | ESP-IDF 6.1 |
| Flash port | `COM5` |
| App binary | `firmware/xiaozhi-esp32/build/xiaozhi.bin` |
| App bytes | 9,511,600 |
| App SHA-256 | `0e33672f34c360940fc43da75b13bb313d11f42e1a2ec4711d71eab5b983d4f2` |
| Assets binary | `firmware/xiaozhi-esp32/build/generated_assets.bin` |
| Assets bytes | 4,036,762 |
| Assets SHA-256 | `81f9622be5bf06517830b0d44ee0f253b71097935449c7bd5e0144c17eeb1416` |

Current S31 behaviour includes 16 kHz mono Edge TTS PCM, 16 kHz Ogg Opus SFX,
unified 綠豆 anchor `(688,112)`, and long-press logout only on scan-ready.

## S3 - real hand

| Item | Value |
|---|---|
| Role | four servo lids, four VL6180X TOF sensors, ESP-NOW hand |
| Target | `esp32s3` |
| Source | `firmware/esp32s3/game_hand/` |
| Last source change | `d70b01e` |
| IDF | ESP-IDF 6.0.2 |
| Flash port | `COM8` |
| Binary | `firmware/esp32s3/game_hand/build/stc_game_hand.bin` |
| Bytes | 833,056 |
| SHA-256 | `de124562e1bca5dca38a372d0b4ff08dd5e73ea45bd41c566b4924eb0364a21f` |

Current S3 behaviour includes 120 ms staggered servo OPEN/CLOSE, lazy LEDC
reinitialisation, TOF address retries, paused TOF/PWM mode for CHAT, and the
USB Serial/JTAG secondary console for diagnostics.

## Reproducibility rule

If either board is rebuilt, record the new source commit and replacement binary
hash here. Do not claim a physical board is running a source commit without the
matching build artifact hash.