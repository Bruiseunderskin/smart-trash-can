# Changelog

本檔案記錄所有對讀者有影響的改動。格式依 [Keep a Changelog](https://keepachangelog.com/zh-TW/1.1.0/)，
版本號依 [語意化版本](https://semver.org/lang/zh-TW/)。

**規則（見 `docs/agents/commits.md`）**：每個 commit 都要在此檔的 `[Unreleased]` 加一條。
沒有 changelog 條目的 commit 視為未完成，不要 push。

## [Unreleased]

### Added

- `models/wordless-padded/`：158 張卡面（38 印刷 ＋ 120 輔助）、珠光紙 sprite、每張 200 張場景、470 張持牌真實相片與 SHA-256 清單，成為訓練資料唯一來源。
- `firmware/esp32s3/node/`：S3 節點專案。2026-11 硬件改版後為 GPIO 直出舵機 PWM、4 × TOF050C（VL6180X）用 XSHUT 改址、S31 ↔ 節點走 ESP-NOW。
- 加入 300 DPI 試印卡（`docs/cards/300dpi`）：Set A／Set B 正面與共用卡背，卡面與 `cards/` 逐張相同。
- `app/stc_app.c`：`stc_snapshot_t` → `stc_ui_payload_t` 映射，`test_app_wiring` 驗 4 個狀態與非法 phase。
- `firmware/esp32s31/bringup/main/t2_camera.c`：esp-video／V4L2 實作（UYVY 640×480、30 幀、fps、`RESULT T2` 行），缺 `esp_video_init.h` 時回 `code=2`。
- `firmware/common/stc_lid_link.c/.h`：傳輸無關的鏈路協定（300 ms ACK 重試 2 次、500 ms 離線判定），傳輸由呼叫者注入；ESP-NOW 實作在 `stc_lid_link_now.c`，附 `test_lid_retry`、`test_node_offline`。
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
- `models/wordless-padded/`：158 張卡面（38 印刷 ＋ 120 輔助）、珠光紙 sprite、每張 200 張場景、470 張持牌真實相片與 SHA-256 清單，成為訓練資料唯一來源；由 `tools/build_wordless_padded.py` 重建。
- `tools/build_wordless_padded.py`：由 `docs/cards/300dpi`、`cards/` 同 `real-photos/` 組出資料集，卡面 hash 逐張核對。
- `tools/fetch_real_photos.py`：收集持牌真實相片（`data/field-captures` 自有 ＋ TrashNet MIT），逐張寫入 `real-photos.csv`。
- `models/wordless-padded/DATA-LICENSES.md`：卡面同真實相片的來源、授權，以及明確停用的資料。
- `tools/rebuild_wordless_cards.py`：新增 `cutout()` 同 `pearl_paper()`——把印刷卡由白紙底裁出，再上珠光紙效果。
- `tools/render_wordless_scenes.py`：新增 `--manifest-out`，逐張場景輸出 card_id、seed 同 SHA-256。
- `docs/ui/figma-vs-lvgl.md`：九個狀態的 Figma／design JSON 與實際 800×480 LVGL framebuffer 逐屏對照、差異排名、未能比對項目。

### Changed

- 混合資料集目錄命名為 `mixed-card-object`；歷史實拍及 probe 保留原路徑並標明用途。

- 重寫專案入口與 agent 指引，更新 YOLO／ESP-DL 路線、整合現況及分類卡圖片連結。

- Documentation CI 的 `guides` job 新增 `python3 tools/check_mode_gate.py`。
- 模式閘契約 v1.1.0：新增 `STC_IN_CHAT_ENTER`（圖鑑「問綠豆」是 CHAT 唯一入口），`SUMMARY` 長按不再能進 CHAT，閒置自動退出維持 60 s，不變量 I1 同步改寫。
- `firmware/esp32s3/node/`：舵機改用 GPIO 直出 PWM（LEDC 50 Hz，腳位可在 menuconfig 改），刪除 PCA9685 驅動。
- `firmware/esp32s3/node/`：4 粒 TOF050C（VL6180X）改用 XSHUT 逐支改址 0x30–0x33，刪除 TCA9548A 驅動。
- `firmware/esp32s3/node/`：S31 ↔ 節點由 USB CDC 改為 **ESP-NOW**（S31 是 host），16 bytes `stc_lidmsg` 幀與 CRC 不變。
- 模型改為珠光紙材質、158 張卡面（38 印刷 ＋ 120 輔助）、每卡 200 張場景 ＋ 470 張持牌真實相片；未見卡準確率由 0.5465 升到 0.739。
- `AGENTS.md` 第 2 節：分清「repo 內追蹤的資料」與「只在本機、已 gitignore 的資料」，並要求開工前讀 `data/README.md`。
- `AGENTS.md` 第 5 節：文件索引由 `docs/INDEX.md` 改為實際存在的 `docs/README.md`。
- 卡片 sprite 改為印刷版面剪影，不再加綠色外框或把圖案放大到內框 78%（舊 `cards-padded` 同印刷卡對不上）。
- 場景背景改用 `data/field-captures` 自拍相，不再用帶水印的商品圖。
- `render_wordless_scenes.py`：`--scene-split` 現在同 `--balance-*-per-class` 一齊生效（之前 scene-split 會靜默忽略班級平衡）。
- 資料集改為每張卡 200 張場景（158 張卡）、再加入 470 張持牌真實相片；場景目錄 `models/wordless-padded/dataset/` 唔再入 git，`dataset.csv` 保留逐張 seed 同 hash。
- Set C 120 張輔助卡面還原：佢係為未見卡泛化而設，之前同無授權商品圖一齊移除係判斷錯誤；`cards.csv` 用 `kind` 分開 `printed` 同 `auxiliary`。
- `tools/check_docs.py`：衍生資產目錄（`models/wordless-padded/{cards,cards-padded,real-photos,dataset}`）唔再要求 README.md，圖文由上一層說明。
- 重新訓練並量化 `models/wordless-padded/wordless_s31.espdl`；`AGENTS.md` 第 2、3 節同步改成新路徑同新數字。

### Removed

- `data/mixed-card-object/`：舊 8,360 張細卡場景、396 張商品圖、158 張卡面清單同 inventory／calibration CSV。
- `models/mixed-card-object/`：舊混合模型與舊無字候選（`mixed_s31.espdl` 等）。

### Fixed

- `AGENTS.md` 原本聲稱 `data/waste4_ei_train`、`data/waste4_ei_test`、`data/garbage265` 在 repo 內，實際上三者被 `.gitignore` 排除，clone 下來的人不會見到。
