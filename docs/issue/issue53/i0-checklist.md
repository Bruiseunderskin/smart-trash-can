# I0 實作者檢查清單

讀者：I0 實作者及審查者。何時讀：建立 I0 分支之前。

- [ ] 更新並核對 `origin/main`、issue、現有 PR 與各 header；不從 #40／#41 建分支。
- [ ] 記錄 `contracts/stc_ui.h`、`ui/lvgl/include/stc_ui.h` 的宣告差異，先釐清重複 `stc_state_t` 名稱及 LVGL 依賴，設計能同時 include 的編譯測試。
- [ ] 確認 UI v2 契約如何對齊每個公開宣告，不改現有 LVGL 實作；確認 header guard 和 include 路徑不會遮蔽另一份 header。
- [ ] 檢查 `labels.txt`、`classes.json`、`stc_types.h` 與 downstream 使用者；按 D2 更新契約版本、模型次序和 bin 值；加入錯序與名稱不符測試。
- [ ] 按 issue 的 D5/D6 修正屏幕及 ToF 方案文件；不把設計值寫成硬件量測。
- [ ] 審查契約 diff：除兩個指定檔外無其他契約變動；若必須擴大，先記錄 blocker 和變更決策。
- [ ] 同 commit 更新 `CHANGELOG.md`，執行文件檢查、模式檢查、工具測試和新增測試，保存完整輸出。
- [ ] 對照 PR diff 填 PR 描述；中間 PR 用 `Refs #53`，避免提前關閉整體 issue。

可與 I0 並行整理真實 fixture 來源及 host 推理後端，但不把未驗證假設當作 I0 已解決。
