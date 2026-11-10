# Changelog

本檔案記錄所有對讀者有影響的改動。格式依 [Keep a Changelog](https://keepachangelog.com/zh-TW/1.1.0/)，
版本號依 [語意化版本](https://semver.org/lang/zh-TW/)。

**規則（見 `docs/agents/commits.md`）**：每個 commit 都要在此檔的 `[Unreleased]` 加一條。
沒有 changelog 條目的 commit 視為未完成，不要 push。

## [Unreleased]

### Added

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

- 混合資料集目錄命名為 `mixed-card-object`；歷史實拍及 probe 保留原路徑並標明用途。

- 重寫專案入口與 agent 指引，更新 YOLO／ESP-DL 路線、整合現況及分類卡圖片連結。

- Documentation CI 的 `guides` job 新增 `python3 tools/check_mode_gate.py`。
- 模式閘契約 v1.1.0：新增 `STC_IN_CHAT_ENTER`（圖鑑「問綠豆」是 CHAT 唯一入口），`SUMMARY` 長按不再能進 CHAT，閒置自動退出維持 60 s，不變量 I1 同步改寫。
- `AGENTS.md` 第 2 節：分清「repo 內追蹤的資料」與「只在本機、已 gitignore 的資料」，並要求開工前讀 `data/README.md`。
- `AGENTS.md` 第 5 節：文件索引由 `docs/INDEX.md` 改為實際存在的 `docs/README.md`。

### Fixed

- `AGENTS.md` 原本聲稱 `data/waste4_ei_train`、`data/waste4_ei_test`、`data/garbage265` 在 repo 內，實際上三者被 `.gitignore` 排除，clone 下來的人不會見到。
