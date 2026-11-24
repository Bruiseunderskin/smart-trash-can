# ui/lvglpro 預覽（可重跑）

讀者：改 `ui/lvglpro/src/stc_ui.c` 或 `stc_chat_ui.c` 的人、審 PR 的人。
何時讀：要親眼看某個畫面、要確認改動有沒有移動版面、或 CI 的 `lvglpro-preview` 紅了。

這個目錄把 800×480 的 LVGL 畫面渲染成 PNG，不需要 SDL、不需要開發板。
LVGL 版本釘死在 v9.5.0（`85aa60d18b3d5e5588d7b247abf90198f07c8a63`），與 CI 及韌體一致。

## 用法

```sh
tools/lvglpro_preview/build.sh                       # 第一次會 clone LVGL 到 build/lvgl
tools/lvglpro_preview/render.py                      # 26 張 PNG 出到 build/png/
tools/lvglpro_preview/render.py --compare tools/lvglpro_preview/baselines
tools/lvglpro_preview/render.py --update tools/lvglpro_preview/baselines
```

`--compare` 有任何畫面像素差超過 2% 就以非 0 結束，並在 `build/png/diff_<state>.png` 留下放大四倍的熱圖。
2% 的容差只吸收字形抗鋸齒；版面或顏色一改就會超標。

## 涵蓋的 26 個畫面

| 類別 | 狀態 |
|---|---|
| 遊戲 | `idle`、`intro`、`scan_ready`、`scan_logout_confirm`、`capturing`、`classifying`、`result_ok`、`result_ok_2s`、`result_ok_lid`、`result_retry`、`bin_choice`、`error`、`learning`、`summary` |
| 綠豆 | `ask_ready`、`ask_listening`、`ask_thinking`、`ask_answering`、`ask_answering_long`、`ask_offline`、`ask_asr_retry`、`ask_countdown`、`ask_no_answer`、`ask_goodbye`、`ask_connecting`、`ask_provisioning` |

`result_ok_2s` 會在渲染前推進 2000 ms，所以它同時驗證 100 ms tick 的倒數條。
每個畫面都會跑 `stc_ui_check_layout()`；回報 `layout=FAIL` 即失敗。

## 限制

- 以 RGB565（16-bit）渲染，framebuffer 格式同裝置一致；PNG 只係把 RGB565 展開成 8-bit 顯示。
- 不模擬觸控、相機、舵機或模型；只證明畫面。
- 相機區在沒有影像時顯示 placeholder，屬預期。
