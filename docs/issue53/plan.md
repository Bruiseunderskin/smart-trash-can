# 整機整合 Implementation Plan

讀者：執行者及審查者。何時讀：每階段開工與交接前。

**Goal:** 建立可重播離線分類與遊戲路徑，並準備 S31 相機與 S3 節點建置。
**Architecture:** 遊戲核心持有分數；模式閘控制 GAME/CHAT；影像、UI、桶蓋和時間各以介面接入。硬件和 host 實作用同一契約但證據分開。
**Tech Stack:** C、ESP-IDF、ESP-DL、LVGL/SDL、Python、CMake/CTest。
**Spec:** [PRD](prd.md)；完整需求和原始數值見 [issue 快照](issue-snapshot.md)。

## 全局限制

每個階段從最新 main 核對依賴後獨立分支；每個 commit 更新 CHANGELOG。維持契約與離線邊界，勿修改 gate 令測試通過。I0 前先解決 [衝突記錄](risks.md) 中影響該階段的項目。依賴或真實素材缺失時記 blocker，不虛構驗證。

## 階段與檢查

- [ ] I0：對齊 `contracts/stc_ui.h` 和 `contracts/model/classes.json`、屏幕與 ToF 文件；先寫契約 API/標籤次序檢查，再實作並跑三個 gate。檢查沒有改其他契約。
- [ ] I1：建立 `firmware/common` 測試入口、固定映射、真實來源 golden 資料及逐圖 replay 報告。先確認 host 可執行的模型和 fixture 來源；測試順序錯誤拒絕、四桶映射與 tensor hash。field-probe 保持獨立。
- [ ] I2：實作 frame source、replay 與前處理；ESP source 未實作時回 unsupported。以 mock clock 驗五幀與少於三幀的錯誤；驗證 timeout 清理與 golden tensor。
- [ ] I3：按 `contracts/stc_mode.h`、`docs/mode-gate.md` 及 `contracts/stc_game.h` 實作模式和遊戲；涵蓋全部轉移、回合中拒絕 CHAT、計分下限、逾時與 learning 時間。
- [ ] I4：新增 `app/stc_app.c`，將遊戲 snapshot 映射到現有 UI payload；驗狀態映射，產生並人手查看 category guide、learning、summary 截圖。
- [ ] I5：實作 lid loopback/stub、時間 hooks 和端到端 replay；逐個檢查四類及拒判案例的狀態、畫面、命令與分數。記錄測試素材來源和模型執行後端。
- [ ] I6：根據既有 S31 bring-up 實作相機，編譯 target；留 RESULT 格式和測試步驟給硬件持有人。編譯通過仍屬待上板。
- [ ] I7：實作共用 CDC 序列化、S31 link 及 S3 node；先驗 frame、CRC、序號回繞、重試、去重和離線，再建置兩端。核對 I²C 腳位、mux 互斥、Wi-Fi 設定及獨立舵機電源；記錄未測項目。

每階段記錄基準 commit、命令、結果及限制，依 [驗收清單](acceptance.md) 交接。PR 不應提前關閉 #53；只有整個 issue 的完成條件成立時才使用 closing keyword。
