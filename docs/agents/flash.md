# 燒錄與驗收（給隊友）

讀者：拿板子要即刻燒來試的人。
何時讀：拿到 S31 Korvo-1、要驗證這一版 UI 與綠豆接駁時。

## 1. 燒 S31 遊戲機

```sh
cd firmware/xiaozhi-esp32
idf.py set-target esp32s31
idf.py build flash monitor
```

`idf.py` 用 ESP-IDF v6.1（CI 用同一版）。第一次 build 會拉 managed components，需要網絡。

## 2. 開機應該見到

1. 頂欄「分類小隊｜準備好」，中間 56 px 標題、左邊相機卡、右邊「目前分數」。
2. 按「開始挑戰」→ 掃描頁。
3. 掃描頁**冇掣**：頂部綠色條 3 秒縮短，自動開始掃描。
4. 結果頁：112 px 類別名、頂欄「連對 N 次」＋「完成」、相機縮圖、類別圖示、分數、提示、底部「等待開蓋」pill。
5. 蓋閂上後頂部綠條倒數，完結自動入下一回合。
6. 總分頁按「問綠豆」→ 切去 xiaozhi 語音（離線時頂欄顯示「暫時離線」）。

## 3. 已知未驗

- 自動掃描未有實機驗證：controller 只在 `STC_PH_PROMPT` 接受 `STC_ACTION_SCAN`；冇卡時會走模型拒判，畫面停喺重試。
- 綠豆 prompt 只在文件層（`docs/agents/greenbean-system-prompt.md`），xiaozhi 的 LLM 端要自己貼入 system message。
- 四類 glyph 是 96 px 資產；`bin_choice` 用 80 px 顯示。

## 4. 出事時

| 現象 | 查 |
|---|---|
| 開機黑屏 | `idf.py monitor` 看 `stc_ui_port` / `StcShellUi` log |
| 字變豆腐格 | 字型子集缺字；重跑 `tools/lvglpro_preview/gen_category_icons.py` 以外，字型要用 lv_font_conv 重出（見 CHANGELOG 2026-11-21 條目） |
| 問綠豆冇反應 | 確認 build 咗 xiaozhi 板 `esp32-s31-korvo-1-stc`；`stc_shell_ui_request_chat()` 只在此板存在 |
| CI 紅 | `gh pr checks 74`，第二欄唔係 `pass` 即紅 |
