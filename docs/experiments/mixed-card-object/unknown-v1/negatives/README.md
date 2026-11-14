# 非卡負面輸入

讀者：模型驗證者。何時讀：核對五類模型對「唔係卡」的輸入怎樣反應時。

四張圖，全部**不參與訓練、校準或選模**，只作事後檢查：

| 檔案 | 來源 | 說明 |
|---|---|---|
| `empty-camera-s31.jpg` | `docs/issue/issue12/live-camera/20261102T221521_996982_001_unlabeled.jpg` | 唯一一張真實 S31 空鏡頭（枱面、布、牆） |
| `blank-white.png` | 評估時生成的 709×709 純白圖 | 合成探測，不代表真實畫面 |
| `shared-cover.png` | `docs/experiments/mixed-card-object/wordless-v1/negative-controls/cover.png` | 印刷套裝封面 |
| `shared-back.png` | 同上 `back.png` | 印刷套裝卡背 |

結果見上層 [README](../README.md)：空鏡頭與純白圖判 `unknown`；封面（0.923）與卡背（0.572）仍判 `hazardous` 並過 0.55 門檻，屬已知未解問題。
