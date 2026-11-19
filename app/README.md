# app — 遊戲與 UI 之間的接線

讀者：整合人員。何時讀：改 snapshot → payload 的對應時。

`stc_app.c` 把 `stc_game` snapshot＋controller view 轉成 `stc_ui` payload。
`stc_game_controller.c` 負責按回合編排 scan、hand、voice 與 render，所有硬件能力由 ports 注入。
分數、模式、硬件一律不在此改。對應表由 `firmware/common/test/test_app_wiring.c` 逐欄驗證。

| phase | UI state |
|---|---|
| `IDLE` | `STC_UI_IDLE` |
| `PROMPT` | `STC_UI_SCAN_READY` |
| `SCAN` | `STC_UI_CAPTURING` |
| `RESULT` | `STC_UI_CLASSIFYING` |
| `OPEN`、`WAIT_DROP`、`SETTLE`、`CLOSE` | `STC_UI_RESULT_OK` |
| `LEARNING` | `STC_UI_LEARNING`：回合回饋，不是圖鑑 |
| `SUMMARY` | `STC_UI_SUMMARY` |

UI fault 由 `view.fault_code` 覆蓋成 `STC_UI_ERROR`。UI 不直接開蓋、改分數或切模式。

階段一 native UI 已由 `ui/lvgl/preview` 驗證，18 張 framebuffer 與依旗標
生成的 replay／live／dev 角標保存在 `ui/lvgl/preview/render/`。這是 desktop
render evidence；S31 實屏另由 COM5 serial log 驗證。

`stc_shell_mode.c` 提供只有 `GAME`／`CHAT` 的外層狀態機。CHAT 期間不推進
game tick，60 秒無操作自動返回 GAME；CHAT 後端未接時會回報
`STC_ERR_UNSUPPORTED`，UI 不得假裝已有對話能力。
