# Changelog

本檔案記錄所有對讀者有影響的改動。格式依 [Keep a Changelog](https://keepachangelog.com/zh-TW/1.1.0/)，
版本號依 [語意化版本](https://semver.org/lang/zh-TW/)。

**規則（見 `docs/agents/commits.md`）**：每個 commit 都要在此檔的 `[Unreleased]` 加一條。
沒有 changelog 條目的 commit 視為未完成，不要 push。

## [Unreleased]

### Added

- `tools/check_mode_gate.py`：交叉核對 `contracts/stc_mode.h` 與 `docs/mode-gate.md` 的閒置超時與契約版本，
  兩者不一致時 CI 直接失敗（今天發生的 60 s／90 s 混用就是這個檢查要攔的）。
- `docs/agents/workflow.md`：六道閘的工作流程，逐步說明每個錯誤由哪一步攔住。
- `.github/pull_request_template.md`：PR 描述要有對應 issue、沒有改的項目與驗證清單。
- `docs/agents/`：agent 專用文件的入口，收納 commit 規則與 skills 對照表，讓 `AGENTS.md` 保持簡短（漸進式披露）。
- `CHANGELOG.md`：本檔案，由今次開始作為所有改動的單一記錄。
- `AGENTS.md` 第 7 節：工作分支可直接 commit，但每個 commit 必須附 changelog 條目；合併到 `main` 仍須 PR ＋ CI 綠燈。
- `AGENTS.md` 第 8 節：skills 清單與漸進式披露的規則。

### Changed

- Documentation CI 的 `guides` job 新增 `python3 tools/check_mode_gate.py`。
- 模式閘契約 v1.1.0：新增 `STC_IN_CHAT_ENTER`（圖鑑「問綠豆」是 CHAT 唯一入口），`SUMMARY` 長按不再能進 CHAT，閒置自動退出維持 60 s，不變量 I1 同步改寫。
- `AGENTS.md` 第 2 節：分清「repo 內追蹤的資料」與「只在本機、已 gitignore 的資料」，並要求開工前讀 `data/README.md`。
- `AGENTS.md` 第 5 節：文件索引由 `docs/INDEX.md` 改為實際存在的 `docs/README.md`。

### Fixed

- `AGENTS.md` 原本聲稱 `data/waste4_ei_train`、`data/waste4_ei_test`、`data/garbage265` 在 repo 內，實際上三者被 `.gitignore` 排除，clone 下來的人不會見到。
