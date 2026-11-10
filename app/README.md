# app — 遊戲與 UI 之間的接線

讀者：整合人員。何時讀：改 snapshot → payload 的對應時。

`stc_app.c` 只做一件事：把 `stc_game` 的 `stc_snapshot_t` 轉成 `stc_ui` 的 `stc_ui_payload_t`。
分數、模式、硬件一律不在此改。對應表由 `firmware/common/test/test_app_wiring.c` 逐欄驗證。

| phase | UI state |
|---|---|
| `IDLE`、`PROMPT` | `STC_UI_IDLE` |
| `SCAN` | `STC_UI_CAPTURING` |
| `RESULT`（abstain） | `STC_UI_RESULT_RETRY` |
| `RESULT` | `STC_UI_CLASSIFYING` |
| `OPEN`、`WAIT_DROP`、`SETTLE`、`CLOSE` | `STC_UI_RESULT_OK` |
| `LEARNING` | `STC_UI_LEARNING` |
| `SUMMARY` | `STC_UI_SUMMARY` |

SDL 截圖未做：host 上沒有 LVGL／SDL，截圖會是假圖，所以本目錄不提供 `preview/render/*.png`。
