# baselines

`tools/lvglpro_preview/render.py` 每次渲染完會同這裡的 PNG 逐像素比對。
容差 2%，只吸收字形抗鋸齒；版面一改就會超標。

更新方法（確認改動正確之後）：

```sh
tools/lvglpro_preview/render.py --update tools/lvglpro_preview/baselines
```

19 張：12 個遊戲狀態加 7 個綠豆狀態。`diff_<state>.png` 只在有差異時寫入 `build/png/`。
