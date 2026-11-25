# 800×480 UI handoff

讀者：設計師、Remotion／動畫 AI、要重建畫面的工程人員。何時讀：需要實機 UI 的像素、文字、座標或操作流程時。

- `screens/`：26 張由 LVGL 9.5 framebuffer 直接輸出的 800×480 PNG。
- `layouts/`：同一批畫面在 LVGL 執行時的 object tree，座標是 absolute pixels，不是估算。
- `motion/`：指定時間點的正常動畫 frame；baseline PNG 本身使用 reduced motion。
- `figma-import/`：每張圖一個可直接拖入 Figma 的 SVG，另有 `screen-atlas.svg`。
- `manifest.json`：完整 machine-readable source of truth，包括 tokens、screen metadata、hash、flow。
- `remotion-manifest.json`：Remotion composition、assets、timings 與操作流程建議。
- `remotion-prompt.md`：可直接貼給 Remotion AI 的完整限制與工作指示。
- `index.html`：本地視覺索引；直接開檔即可。
- `flow.mmd`：Mermaid 流程圖。
- `screen-index.csv`：表格化清單，方便其他工具快速索引。

## 精確度

PNG 是 800×480、rotation 0、RGB565 framebuffer 展開後輸出；layout JSON 由同一個 running LVGL object tree 讀取。頁面在真機是 instant cut，沒有全局 slide／fade。實體 LCD 仍可能因 panel gamma、色域及視角與 framebuffer 有差異。

## Remotion 建議

1. 以 `remotion-manifest.json` 的 `flows` 建立 scene sequence。
2. 每個 state 用 `screens/{state}.png` 作 full-frame asset，保持 800×480、contain、不要重新裁切。
3. 用 `layouts/{state}.json` 的 button bounds 放觸控波紋；不要顯示 desktop mouse cursor。
4. 倒數與進度只可動 `derived.progressBars` 對應的矩形；其他版面保持不動。
5. 轉頁用 hard cut；需要動畫時用 `motion/` frame 或局部 opacity 動畫，不要整頁 fade。
6. 綠豆在所有遊戲頁固定在 `(688,112)`、64×64；不要為個別 scene 重排。

## 重建

```sh
tools/lvglpro_preview/build.sh
python tools/lvglpro_preview/render.py --compare tools/lvglpro_preview/baselines
python tools/lvglpro_preview/export_handoff.py
```

最後一步會重新產生整個資料夾。只可在已核對 baseline 的 UI source 上執行。
