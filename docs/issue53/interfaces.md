# 介面與資料流交接

讀者：實作者及跨模組審查者。何時讀：修改契約、接線或寫測試前。以最新 `main` 和現行 header 為準；本文件不新增公開 API。

## 資料與責任

| 邊界 | 現有來源 | 輸入／輸出及責任 |
|---|---|---|
| 模式 | `contracts/stc_mode.h`、`docs/mode-gate.md` | `stc_mode_input` 管理狀態，拒絕非法轉移時不可觸發 hook；tick 監察 CHAT 超時。 |
| 影像 | `contracts/stc_vision.h` | `stc_vision_scan(out, detail, timeout_ms)` 開相機、最多五幀、平均機率、關相機；少於三幀或超時回錯誤。 |
| 模型前處理 | `contracts/model/preprocess.json` | UYVY 640×480、center crop、bilinear、RGB 160×160 NHWC；bit-exact 色彩和取樣細節仍要指定。 |
| 遊戲 | `contracts/stc_game.h` | `stc_game_input` 接事件；`tick` 推進狀態；`snapshot` 唯讀；只有遊戲核心改分數和回合。 |
| 桶 | `contracts/stc_bin.h` | 目前公開 `open_all`、`close_all`、monitor 和 poll；傳感事件不是自動等於一次有效得分。 |
| UI | `ui/lvgl/include/stc_ui.h` | `stc_ui_create` 建 UI，`stc_ui_apply` 套 payload；callback 只發意圖，不能直接做硬件動作。 |
| 語音 | `contracts/stc_voice.h` | 區分 GAME 本地素材與 CHAT；codec 實際播放另待硬件。 |

## 固定映射

`food → 1`、`hazardous → 2`、`other → 3`、`recyclable → 0`。這是模型 index 到桶位，並非目前 `classes.json` 的 index。I0 改契約前的版本與標籤必須核對；未知或錯序不降級成 other。

## 接線時不能自行補足的空缺

1. `contracts/stc_mode.h` 和 `ui/lvgl/include/stc_ui.h` 均定義不同的 `stc_state_t`，目前不能在同一 C translation unit 直接包含。I0 的「逐個簽名對齊」不能只複製 header，須提出相容命名／adapter 方案並確認契約修改範圍。
2. 模式 header 沒有 `STC_IN_BOOT_OK`，issue 啟動次序卻要求該輸入。`STC_IN_ROUND_END` 亦沒有參數區分最後回合。必須先確定如何以現有 API 表達，不能偷偷擴大契約。
3. UI payload 的 category、兩個 candidates、score、streak、error 與 image 指標並非都在 `stc_snapshot_t` 中。需確認來源、有效值及生命週期；缺資料不要虛構。
4. UI 必須在 LVGL task 或 lock 下呼叫；image descriptor 和像素由呼叫者持有至替換完成。callback 不可直接 destroy UI；跨 task 消息需有界並處理過期 round。
5. `stc_bin_open_all` 與 issue 的單蓋敘述衝突；在決策前不要把單蓋操作或四蓋操作視為已確認的遊戲行為。
