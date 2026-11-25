# 可直接交給 Remotion AI 的工作指示

你要用 Remotion 製作「分類小隊」S31 實機 UI 的模擬操作流程動畫。唯一視覺 source of truth 是本資料夾，不要自行重畫、改字、改色或移動元件。

## 必讀

1. 先讀 `remotion-manifest.json`、`manifest.json` 及相關 `layouts/<state>.json`。
2. 畫面固定 800×480、rotation 0、60 fps；用 `screens/<state>.png` 作 full-frame asset。
3. 以 `flows` 建立 sequence。主要流程用 `live_round_happy_path`；五回合用 `five_round_loop`。
4. 轉頁保持 hard cut。LVGL 真機沒有全頁 fade／slide。
5. 觸控只畫局部 ripple，位置用 layout 的 `derived.buttons`；不要顯示 desktop mouse cursor。
6. 進度條只可按 `derived.progressBars` 的 bounds 動畫，不可移動其他元素。
7. 綠豆固定在 `(688,112)`、64×64；只有 `scan_ready` 有長按退出。
8. `motion/` frame 是正常動畫取樣，只可用在掃描效果與時間條；其餘畫面保持 baseline。
9. 不要加入真實硬件畫面、3D 垃圾桶或 UI 內不存在的按鈕。
10. 輸出時保留 800×480 composition；如需 16:9 展示可整張等比放大，不可重新排版。

## 建議元件

```tsx
const shot = (state: string) => staticFile(`screens/${state}.png`);
<Img src={shot(state)} style={{ width: 800, height: 480 }} />
```

每個 scene 可先用 hard cut 顯示 PNG，再用 `interpolate` 局部遮蓋 progress bar；需要掃描動態時，切換 `motion/capturing_250ms.png` 至 `motion/capturing_1000ms.png`。完成後請自行核對：所有畫面尺寸、文字、四桶位置及綠豆 anchor 與 handoff 一致。
