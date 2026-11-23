# Changelog

本檔案記錄所有對讀者有影響的改動。格式依 [Keep a Changelog](https://keepachangelog.com/zh-TW/1.1.0/)，
版本號依 [語意化版本](https://semver.org/lang/zh-TW/)。

**規則（見 `docs/agents/commits.md`）**：每個 commit 都要在此檔的 `[Unreleased]` 加一條。
沒有 changelog 條目的 commit 視為未完成，不要 push。

## [Unreleased]

### Changed

- S31 xiaozhi board：移除聊天 payload 已刪除的 `hint` 欄位，並把內嵌模型由舊 `wordless-unknown` 同步到 `wordless-padded`（SHA `5fb3ac6e…`、CRC `0x1c55ab7a`）。實板進入綠豆後成功連接 `api.tenclass.net` MQTT 並完成 ASR／LLM／TTS 對話。
- S31 standalone game：新模型與 v3 UI 已完成接駁；首次開始顯示三步 Intro，result/open 階段顯示新結果頁，`問綠豆` 進入 9 頁聊天或離線頁，修正聊天次按鈕誤綁 primary action。S3 `DROP bin` 仍是實際投放與計分輸入，ESP-NOW 協議不變。
- `models/wordless-padded/`：重訓 5 類（含 unknown）模型，加入 RealWaste／viola77／medicine 真實相；`wordless_s31.espdl` 1,797,936 bytes（SHA `5fb3ac6e…`、CRC `0x1c55ab7a`），17 張 field probe 16/17（舊 12/17），medicine held-out int8 160/171（舊 32/171）。Firmware `MODEL` 改指 padded 並同步 SHA/CRC。
- `models/wordless-padded/`：同步 `main@07e59daa`（PR #73）的模型產物——`wordless_s31.espdl` 1,792,992 bytes（w8a8、KL 校準 1,200 張、bias correction、equalization）、`model-int8.onnx`、`model-float32.onnx`、`wordless_s31.metrics.json`、`SHA256SUMS`、`cards.csv`、卡面與真實相片。已知卡未見場景 4,696/4,698（macro-F1 0.9977）；未見卡 fold 0 的 0.739 屬上一代，未在新分佈重跑。未在 S31 實機驗證。

### Added

- `docs/chat/luodou-system-prompt.md`：綠豆 system prompt v3（1,640 字），含指令層級、防泄漏、防操控、兒童安全與紅隊測試須知，供隊友直接貼入 xiaozhi 設定。
- `tools/lvglpro_preview`：預覽由 32-bit XRGB8888 改為 RGB565，同 S31 framebuffer 一致；baselines 重新產生。
- `ui/lvglpro`：第一頁改為標題＋「開始遊戲」／「問綠豆」；新增 `intro`（3 步）；`scan_ready` 加觀景框；拍攝／辨識頁全屏相機、移除返回鍵與 AI 角標；錯誤頁只留「重試」。
- `ui/lvglpro`：問綠豆 9 頁——新增 `ask_no_answer`（答唔到）與 `ask_goodbye`（再傾）；整套 chat 改用分類小隊 topbar、卡片 16 radius、outer 24、control height 64，primary 在左。
- `ui/lvglpro`：結果／選桶頁四格對回 166×248、y=216、icon 88；選中桶發亮、其餘半透；選桶頁唔再可點，交 ToF／S3 ESP-NOW 回報。
- `tools/lvglpro_preview`：baselines 更新為 22 個畫面；`intro`、`ask_no_answer`、`ask_goodbye` 已入 render 清單。

- `ui/lvglpro`：回合結果頁改為回合出口——3 秒自動繼續（100 ms tick 的收縮進度條），蓋門未關時不計時。計時器由 `stc_ui_create()` 建立、`container_deleted` 釋放，全程不解構中的 timer。畫面冇主按鈕：回合自動前進。
- `ui/lvglpro`：四桶卡片對回 Figma `state/08a-await_bin`（110:848）與 `state/08b-lid_closed`（124:214）：標籤 40 px、文案「可回收」、開蓋態白底 2 px 實色邊、閂蓋態未選 3 px soft＋文字 60%、選中 8 px 實色，並移除「AI 選」角標。
- `ui/lvglpro`：刪除「AI 判斷：… · 信心值 …」一行，以及「3 秒後自動繼續」「投入卡牌後自動繼續」兩句；倒數只由頂部 8 px 綠色進度條表示。
- 綠豆（問答）接上遊戲 UI：新增 `STC_ASK` 狀態與 `stc_ui_show_ask()`，把既有 `stc_chat_ui` 的七個畫面（ready／listening／thinking／answering／offline／asr_retry／countdown）掛入 `stc_ui`；首頁與總分頁的「問綠豆」終於有畫面。`ready` 畫面冇「開始講」：該步交由 xiaozhi 語音接手，只留「返回主頁」。刪除每頁的提示句（例如「直接講出你想問嘅環保問題。」）。
- 首頁綠豆頭像上移 5 px，與標題字墨中心對齊（實測 177 → 172）。
- `ui/lvglpro`：`scan_ready`／`result_retry`／`bin_choice` 的標題與副標不再重疊；`learning` 的「正確類別」移入 32 px 邊距內；`bin_choice` 四桶文字統一為 40 px「可回收」。
- `ui/lvglpro`：蓋已閂、倒數進行中的結果頁標題由「正在開啟桶蓋」改為「本回合完成」；綠豆對話氣泡改為貼住文字，最高 180 px。
- `ui/lvglpro`：頂欄狀態依 Figma：`idle` 顯示「準備好」，唔再喺未開局時顯示得分；得分只喺總分頁出現。
- `ui/lvglpro`：綠豆頂欄依 Figma chat2（237:258）：24,24,752×56 圓角條、左「問綠豆」、右連線狀態（已連線／聽緊／諗緊／未連線／再講一次／準備返回）。
- `ui/lvglpro`：`scan_ready` 唔再要撳「開始掃描」——3 秒倒數（頂部 8 px 綠條）後自動送出 `STC_ACTION_SCAN`；每回合所需撳掣由 1 降到 0。
- `ui/lvglpro`：`idle` 對齊 Figma `state/idle`（5:36）——56 px 標題、320×144 相機卡、368×144 分數面板（頂部 8 px accent）、752×64 全寬「開始挑戰」；移除「問綠豆」按鈕。
- `ui/lvglpro`：`result_ok` 對齊 Figma 5:79——112 px 類別名（類別色）、頂欄「連對 N 次」＋「完成」、112 px 相機縮圖、96 px 類別圖示、`目前分數`＋80 px 分數、類別提示、底部「等待開蓋」pill；四桶畫面保留在 `bin_choice`。
- 字型：新出 `stc_lp_font_tc_bold_112`（類別名），並以現有 UI 字串重生成 bold/medium 32 系列（222 字）。
- 圖示：四類 glyph 改用 Figma SVG 重新 rasterise 的 96 px ARGB 資產（`stc_lp_img_cat_*_96`），結果頁 1:1 顯示，唔再係 48 px 放大兩倍的模糊邊。
- 新增 `docs/agents/greenbean-system-prompt.md`：綠豆 system prompt，含注入防範與 8 個測試案例。
- 綠豆接駁：`stc_game_controller` 新增可選 port `request_chat`，`STC_UI_ACTION_ASK` 會交去 xiaozhi 外殼（`stc_shell_ui_request_chat()` → `STC_SHELL_CHAT_STARTING`）；冇 chat 的 build 保持 NULL，行為不變。
- 建置修正：`firmware/esp32s31/game` 加入 `stc_chat_ui.c`（先前只有 xiaozhi 板有，S31 game 連結失敗）；`ui/lvglpro` 預覽 CI 獨立成 `lvglpro-preview.yml`，唔再連帶觸發 `ui/lvgl` 的 native-render。
- 修正：`STC_ASK` 只可以由 `idle` 進入，令總分頁「問綠豆」實際無反應；現在 `result_ok`／`summary` 亦可入 `STC_ASK`。由 host 模擬（`tools/lvglpro_preview/sim.c`）發現。
- 新增 `tools/lvglpro_preview/sim.c`：無硬件的行為模擬，驗證 3 秒自動掃描、結果頁自動前進、以及綠豆可由結果／總分頁進入；CI 會跑。
- 新增 `docs/agents/flash.md`：燒錄步驟與開機應見到的畫面。
- 新增 ESP-IDF 6.1 安裝／VS Code 切換與 esp-emu 模擬步驟（`docs/agents/flash.md` §5–6）；實測 S31 影像 boot 到 `app_main`，之後因模擬機板冇 GT1151 觸控而 abort。
- esp-emu 加 GT1151 觸控 regfile 後，S31 影像可 boot 到 `stc_ui_port: 800x480 UI ready`（exit-on 命中，exit 0）；下一個 blocker 是 ES8389 codec 的 I2C open。
- 診斷版（跳過 speaker，不入 repo）在 esp-emu 跑完模型自測：`correct=39 total=40`，B-19 一張 `expected=1 predicted=3`；`MODEL_SELFTEST=FAIL` 但 `MODEL_START_GATE=IGNORED`（閘門未生效）；`LIVE_CAMERA=FAIL`（模擬冇相機）、ESP-NOW `controller ready`；正式影像仍因 ES8389 缺席而在音訊 init abort。
- 新增 `tools/lvglpro_preview/gen_category_icons.py`：由 Figma SVG 重生成 96 px 圖示（sharp-cli 6.1.0），支援 `--check`。
- `ui/lvglpro`：重新生成 `stc_lp_font_tc_bold_32`（218 個字符，含「離」「×」），綠豆離線狀態可用 Figma 原文「暫時離線」。
- 新增 `tools/lvglpro_preview/`：用釘死的 LVGL v9.5.0 在本機渲染 19 個畫面（12 遊戲 + 7 綠豆），可 `--compare` 對 baseline 做 2% 容差的像素比對；有差異時在 `build/png/` 留下放大四倍的 `diff_<state>.png` 熱圖。CI 加 `lvglpro-preview` job 重跑同一套。<!-- changelog-fixed -->
- Added a two-state outer application shell, `GAME` and `CHAT`. GAME owns the current touch game; CHAT pauses game ticks, has a 60-second timeout, and returns to the same game state. The current local voice backend reports `STC_ERR_UNSUPPORTED` for CHAT and the UI states this explicitly.
- Added GAME/CHAT touch navigation: `分類圖鑑`, `問綠豆`, and the CHAT `返回遊戲` screen. Added `STC_UI_CHAT` as an appended UI state without renumbering existing values.
- Added the local `STC_GAME_TOUCH_BIN=ON` game mode: continuous SC101IOT preview, four on-screen bin buttons, human DROP selection, hidden-answer learning feedback, and no ESP-NOW dependency. The selected category is still validated and scored only by the game controller.
- Added RGB888→native RGB565 preview conversion and native UI coverage for the appended `BIN_CHOICE` state and `SELECT_BIN` action without renumbering existing UI contract values.
- Added `STC_GAME_DEV_FLOW=ON` as an explicit stage-one development bypass and five-round autopilot: only this build accepts the current 39/40 model, logs `DEV_BYPASS=1`, shows `DEV 39/40`, waits for the S3 heartbeat, and advances scan/learning automatically. The default remains OFF with the strict 40/40 release gate.
- Added dynamic S31 UI debug badges for replay, live camera, virtual hand and dev-flow states without changing the existing v2.1.0 state/action values.
- Added an opt-in S31 SC101IOT camera probe (`STC_CAMERA_PROBE=ON`) that captures a 640×480 UYVY frame, converts it to the model's 160×160 RGB input, runs ESP-DL, and can dump the exact model input over UART.
- Added the `STC_GAME_LIVE_CAMERA=ON` game path: five live camera frames are classified on S31 and aggregated by the existing controller while replay remains the default fallback.
- Imported the `main@d4440b3` wordless-padded S31 artifact (`wordless_s31.espdl`, SHA-256 `233619c9...`, CRC `6e1f6dec`) and added its frozen replay smoke result. The S31 device is now 39/40, with the remaining `B-19_003` miss still blocking the 40/40 release gate.
- ESP32-S31 standalone stage-one game firmware: Korvo-1 display/touch, local ES8389 PCM, embedded deterministic replay pack, real ESP-DL inference, game controller, and fixed `REPLAY · VIRTUAL HAND` labels.
- ESP32-S3 `virtual_hand` firmware: channel-6 ESP-NOW discovery, 250 ms heartbeat, command ACK, virtual DROP pattern/delay, and USB serial overrides.
- Shared hand protocol v2 and reliability layer: 16-byte CCITT-FALSE frames, 300 ms ACK timeout with two retries, DROP retries, deduplication, offline detection, full game-round correlation, and retry delivery when a DROP arrives before the game accepts it.
- Deterministic replay tooling and tests: seed `20261111`, 40 embedded scenes, five rounds covering all four classes, five-frame aggregation, and build-time pack generation.
- Stage-one controller, hand-link, replay-pack, and native UI tests for `SCAN_READY`, learning feedback, contrast, touch targets, glyphs, and overflow.
- `docs/game-implementation-plan.md`：以 PR #61 的現況為基礎，列出遊戲核心缺口、UI／模型整合界線、分階段實作及上板驗收。
- `docs/research/espnow-s31-feasibility.md`：依 ESP-IDF v6.1 官方文檔與 source 記錄 ESP-NOW 的 channel、callback、ACK、加密與供電限制。
- `firmware/espnow_link_spike/`：ESP32-S31 與 ESP32-S3 的 ESP-NOW 傳輸實驗；重用 16-byte `stc_lidmsg`、固定信道、自動發現、心跳與 ACK，暫不驅動伺服機／TOF。
- `app/stc_app.c`：`stc_snapshot_t` → `stc_ui_payload_t` 映射，`test_app_wiring` 驗 4 個狀態與非法 phase。
- `firmware/esp32s31/bringup/main/t2_camera.c`：esp-video／V4L2 實作（UYVY 640×480、30 幀、fps、`RESULT T2` 行），缺 `esp_video_init.h` 時回 `code=2`。
- `firmware/common/stc_lid_link.c/.h`：CDC 協定（300 ms ACK 重試 2 次、500 ms 離線判定），附 `test_lid_retry`、`test_node_offline`。
- `firmware/esp32s3/node/`：S3 節點專案（PCA9685 `0x40`、TCA9548A `0x70`、TOF050C、USB Serial/JTAG CDC），`sdkconfig.defaults` 關 Wi-Fi 與藍牙。
- `docs/integration-runtime.md`：啟動次序、失敗碼對照、CDC 鏈路、電源預算（設計值，標明未測）。
- CI：`firmware.yml` 新增 **Host C tests** job（cmake ＋ ctest），並把 `stc_node` 加入 esp32s3 編譯矩陣。
- `firmware/common/stc_vision_map.c/.h`：模型 index → 桶位固定表 `{1,2,3,0}`，啟動時逐行核對 `labels.txt`，不符回 `STC_ERR_CONTRACT`。
- `firmware/common/stc_preprocess.c` ＋ `stc_preprocess.h`：UYVY→RGB（BT.601）→ 中央正方形裁切 → bilinear 160，附 golden fixture 與 sha256 測試。
- `firmware/common/stc_frame_source.h`、`stc_frame_replay.c`、`stc_frame_esp.c`：幀來源介面；ESP 路徑在 I6 前一律 `STC_ERR_UNSUPPORTED`。
- `firmware/common/stc_mode.c`：六狀態模式閘，回合中 `IN_SET_LONG`／`IN_PLAY` 一律拒絕，CHAT 60 s 閒置自動回 GAME。
- `firmware/common/stc_game.c`：回合狀態機、5 回合計分（+10／−5／floor 0）、abstain 重掃一次、學習頁 6 s。
- `firmware/common/stc_lid.h`、`stc_lid_loopback.c`、`stc_lidmsg.h/.c`：16 bytes CDC 幀（CRC-16/CCITT-FALSE）與 host 回環實作。
- `tools/evaluate_replay.py`：對 `data/field-probe` 逐圖輸出 truth／pred／conf，刻意不印總分。
- `firmware/common/test/`：`test_label_map`、`test_preprocess_golden`、`test_mode_gate`、`test_game`、`test_lid`、`test_e2e` 共 8 個 ctest，全部通過。
- 契約 `stc_ui.h` 升 **v2.0.0**：對齊 `ui/lvgl` 的 `stc_ui_create/apply` 與 action callback 模型，新增 `stc_ui_payload_t`、`stc_ui_state_t`、`stc_ui_action_t`（依 2026-11-08 決定 D1）。
- 契約 `classes.json` 升 **v2.0.0**：類別次序改為模型次序 `food, hazardous, other, recyclable`，`key` 由 `hazard` 改 `hazardous`，加 `class_order_frozen`；桶位 0–3 不變（依決定 D2）。
- 新增 `firmware/common/` 與 `firmware/common/test/`：CMake ＋ ctest，`test_contract_ui`（列舉次序／payload 佈局）與 `test_classes_order`（`classes.json` 對 `models/wordless-padded/labels.txt`）。
- 新增 `tools/tests/test_contract_ui.py`：契約與 `ui/lvgl/include/stc_ui.h` 逐名同序比對。
- `docs/plan-v3-tof050c.md` §1：改址方案由 SHDN 改為 **TCA9548A（0x70）**，四粒 TOF 固定 0x29（依決定 D6）。
- 新增 S31 量化模型 `models/wordless-padded/wordless_s31.espdl`（w8a8、esp32s31、1,812,848 bytes），測試 155/163、macro-F1 0.9556。
- 未見卡 5 折交叉驗證：158 張卡面下平均 top-1 0.8251（標準差 0.0632）；新 int8 模型覆蓋同一路徑 `models/wordless-padded/model-int8.onnx`。
- 加入 300 DPI A4 試印稿（`docs/print-300dpi/`）：Set A／Set B 正面與共用卡背，卡面與 `cards/` 逐張相同。
- 加框無字卡 int8 模型（`models/wordless-padded/`）及 yolo11n-cls 訓練記錄；場景切分 682 張全對，未見卡 0.558。
- 加框無字卡面：`data/mixed-card-object/wordless-v1/cards-padded/`（38 張，709×709）。移除舊卡底部名稱橫幅、圖案放大至內框 78%，外圍保留綠色圓角框與白邊；由 `tools/rebuild_wordless_cards.py` 產生。
- 新增無額外卡片文字的 38 張卡面、合成變體及 YOLO／ESP-DL 候選模型，並提供逐圖結果與混淆矩陣。

- 加入混合卡片及實物模型可尋回的訓練、驗證資料與來源清單，並標明缺失和授權限制。

- 補充整合決策地圖、YOLO／ESP-DL 模型交接、測試證據範本、CDC 向量及執行時責任表。

- 補充整機整合介面對照、測試矩陣、I0 清單及硬件驗證交接表。
- 整機整合需求、實施計劃、衝突記錄及驗收清單。

- 由 PR #41 抽出六句粵語台詞（`firmware/assets/voice/`，16 kHz 單聲道 raw PCM）與台詞表，供 GAME 模式播報使用。
- `tools/generate_stc_voice_assets.py`：由 WAV 或 Edge TTS 產生經驗證的 raw PCM，附單元測試；格式不符即報錯。
- `docs/issue25/`：PR #41 的搶救記錄，列明抽了什麼、刻意不抽什麼（自製狀態機與對話畫面），以及未接手的部分。
- 新增實物與模擬卡片混合分類的 int8 實驗記錄，分別列出兩種輸入的主機結果及尚未完成的實機驗證。

- `tools/check_mode_gate.py`：交叉核對 `contracts/stc_mode.h` 與 `docs/mode-gate.md` 的閒置超時與契約版本，
  兩者不一致時 CI 直接失敗（今天發生的 60 s／90 s 混用就是這個檢查要攔的）。
- `docs/agents/workflow.md`：六道閘的工作流程，逐步說明每個錯誤由哪一步攔住。
- `.github/pull_request_template.md`：PR 描述要有對應 issue、沒有改的項目與驗證清單。
- `docs/agents/`：agent 專用文件的入口，收納 commit 規則與 skills 對照表，讓 `AGENTS.md` 保持簡短（漸進式披露）。
- `CHANGELOG.md`：本檔案，由今次開始作為所有改動的單一記錄。
- `AGENTS.md` 第 7 節：工作分支可直接 commit，但每個 commit 必須附 changelog 條目；合併到 `main` 仍須 PR ＋ CI 綠燈。
- `AGENTS.md` 第 8 節：skills 清單與漸進式披露的規則。

### Changed

- Game contract is v1.3.0 and UI contract is v2.1.0; scoring and rounds remain owned only by `stc_game`, while UI callbacks emit bounded action intents.
- Learning now shows scanned bin, actual bin, correct bin, outcome, round delta, total score, and one full-width next-round button; it advances only on user action.
- ESP-NOW is the stage-one hand transport instead of USB CDC; both boards use channel 6 without encryption and discover peers automatically.
- The S31 game partition is 12 MB so the ESP-DL model, replay pack, UI assets, and PCM clips leave build headroom on the 16 MB flash.
- 混合資料集目錄命名為 `mixed-card-object`；歷史實拍及 probe 保留原路徑並標明用途。

- 重寫專案入口與 agent 指引，更新 YOLO／ESP-DL 路線、整合現況及分類卡圖片連結。

- Documentation CI 的 `guides` job 新增 `python3 tools/check_mode_gate.py`。
- 模式閘契約 v1.1.0：新增 `STC_IN_CHAT_ENTER`（圖鑑「問綠豆」是 CHAT 唯一入口），`SUMMARY` 長按不再能進 CHAT，閒置自動退出維持 60 s，不變量 I1 同步改寫。
- `AGENTS.md` 第 2 節：分清「repo 內追蹤的資料」與「只在本機、已 gitignore 的資料」，並要求開工前讀 `data/README.md`。
- `AGENTS.md` 第 5 節：文件索引由 `docs/INDEX.md` 改為實際存在的 `docs/README.md`。

### Fixed

- Fixed `INVALID_TRANSITION` in the live touch game by allowing the actual phase path `CLASSIFYING -> BIN_CHOICE`, plus prompt-timeout, scan-retry, and abstain paths into learning/scan-ready. The native regression now exercises those direct transitions rather than taking a replay-only shortcut through `RESULT_OK`.
- Fixed the live touch game's five-frame inference race: device scans measured 1,451-1,573 ms while the generic 1.5 s SCAN timeout could expire first and reject the result as a stale round. `STC_GAME_TOUCH_BIN` now uses a 5 s scan budget while the normal game contract remains unchanged.
- Game startup, PROMPT/SCAN/WAIT_DROP timeout handling, the 180-second limit, duration, consecutive-correct streak, duplicate DROP, and stale round events.
- ESP-NOW OPEN_ALL ACK and DROP now reach the controller with the originating monotonic round ID; `lid_open` changes only after a matching successful ACK.
- S3 retries the same command without resetting a pending virtual DROP, and echoes the OPEN command's sequence on DROP.
- S31 target build now generates the replay pack before embedding it, resolves the ESP-DL binary symbol correctly, and adapts the shared UI contract to the LVGL payload/action types.
- Learning-page glyph coverage now includes the Cantonese timeout/correct/wrong wording, with all five round outcomes covered by the native interaction test.
- S3 no longer reinstalls the USB Serial/JTAG driver when the configured console already owns it.
- `AGENTS.md` 原本聲稱 `data/waste4_ei_train`、`data/waste4_ei_test`、`data/garbage265` 在 repo 內，實際上三者被 `.gitignore` 排除，clone 下來的人不會見到。
