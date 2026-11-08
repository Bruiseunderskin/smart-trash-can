# issue25 — 語音素材搶救記錄

讀者：語音整合人員與 reviewer。何時讀：決定 PR #41 如何處置、或要接手 `stc_voice` 實作時。

狀態：**素材已抽出，實作未接手**（2026-11-08）。

## 背景

PR #41（`codex/issue25-voice-chat`，83 個檔、+8607）是 Codex 交 issue #25 的語音實作，但基於舊契約（`stc_game.h`／`stc_vision.h` v1.0.0，main 已 v1.2.0），而且自帶一套狀態機（`stc_app_fsm.c`，3 個狀態）與自製對話畫面（`StcChatDisplay`），與 main 的 `contracts/stc_mode.h` v1.1.0（6 狀態）及 `ui/lvgl/src/stc_ui.c` 打對台。分支亦不含 `contracts/stc_mode.h` 與 `contracts/stc_ui.h`。

決定：**不 rebase 整條分支，只抽有價值的部分。**

## 已抽出的部分

| 項目 | 位置 | 說明 |
| --- | --- | --- |
| 六句粵語台詞 | `firmware/assets/voice/` | 16 kHz 單聲道 raw PCM，約 470 KB |
| 台詞表 | `firmware/assets/voice/phrases.json` | key → text + 檔名 |
| 產生工具 | `tools/generate_stc_voice_assets.py` | WAV 或 Edge TTS → 驗證後的 raw PCM |
| 工具測試 | `tools/tests/test_stc_voice_assets.py` | 驗格式與輸出 |
| 播出記錄 | [voice-run-s31.log](voice-run-s31.log) | PR #41 分支建置的序列埠記錄，**只證明六段 PCM 在 S31 播得出來**，不代表本分支的整合狀態 |

## 刻意不抽的部分

| 項目 | 原因 |
| --- | --- |
| `stc_app_fsm.c`／`.h` | 與 `contracts/stc_mode.h` v1.1.0 重複；閘由 `stc_mode.c` 實作（issue #53 I3） |
| `stc_chat_display.cc`／`.h` | 取代小智原生畫面的另一套 UI；畫面由 `ui/lvgl/src/stc_ui.c` 負責，規格見 `docs/ui/info-mode-flow.md` v1.4 |
| `stc_shell_ui.cc`、`stc_game_placeholder.*`、`stc_stubs.*` | 佔位實作，與 issue #53 的 I3／I4 重複 |
| 分支的 `contracts/*` | 舊版本；main 為準 |
| 分支刪除的 `docs/issue12/*.json` 等 | main 已有，不應回退 |

## 未完成

- `stc_voice.cc` 的實作**未抽取**：它依賴分支的 FSM 與板目錄，要在 main 契約上重寫，屬 issue #25 的 M2／M3。
- 台詞拼接（14 段組合）未實作。
- 六段 PCM 未在最終韌體中驗證播放延遲（`contracts/stc_voice.h` 要求起播 < 50 ms）。
