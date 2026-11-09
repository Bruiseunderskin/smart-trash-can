# 加框無字卡：訓練記錄

讀者：接手模型或卡片設計的人。何時讀：查詢加框（padding）無字卡面的訓練結果與限制。

## 卡面

`cards-padded/` 38 張，709×709，由 `tools/rebuild_wordless_cards.py` 從 `cards/` 重製：

- 移除卡面底部名稱橫幅（該批卡本身已無字，工具仍會偵測殘留橫幅）
- 圖案裁到 bbox 後放大至填滿內框
- 綠色圓角外框 9.5%、外圓角 13%；框內白邊 1.5%、內圓角 11.5%（內緣有 fillet，圖案四角同步裁圓）
- 外圍 2.5% 透明出血

## 資料

`tools/render_wordless_scenes.py` 由加框卡生成 160×160 場景（隨機背景、角度、縮放、光線、模糊、陰影）。兩種切分：

| 模式 | 用法 | train | val |
|---|---|---:|---:|
| 卡片身分切分（`cards.csv` 的 split） | 量「未見過的卡」 | 6,000 | 120 |
| 場景切分（`--scene-split 0.15`，38 張卡全用） | 量「部署時的已知卡」 | 3,878 | 682 |

## 結果（YOLO11n-cls，160×160）

| 切分 | val 準確率 | macro-F1 | 備註 |
|---|---:|---:|---|
| 場景切分（已知卡） | 1.000 | 1.000 | 682 張全對；early stop 於第 7 epoch，best 第 3 |
| 卡片身分切分（未見卡，v4） | 0.558 | — | 120 張；8 張 held-out 卡 |
| 卡片身分切分（未見卡，v7／v8） | **0.6875** | **0.6287** | 96 張；班級平衡＋真實照片背景＋實物相 |

**判讀**：合成場景由同一批卡面渲染，所以「已知卡」滿分只證明流程通、可部署在固定 38 張卡的示範。未見卡由 0.558 提升到 0.688，靠的是兩件事：班級平衡（原本 14/30 訓練卡是 recyclable，模型傾向全判該類）與真實照片背景＋實物相加入訓練。

未平衡而只加真實照片（v6）反而更差：0.5521／macro-F1 0.4759，24 張 hazardous 中有 20 張判成 recyclable。

v7／v8 剩餘錯誤集中在 hazardous → other（24 張中 5 張）；該類只有 5 張訓練卡、26 張實物相，是下一步的瓶頸。

早期版本（MobileNetV3-small、同一資料、卡片身分切分）為 0.625／macro-F1 0.5679；弱點同樣在 `hazardous`（24 張只得 13 張對，誤判去 `other` 6、`recyclable` 5）。

## 執行

Kaggle GPU kernel：`wedsamuel1230/stc-wordless-padded-cards`（`kaggle/train_wordless_padded.py`）。
本機或 CI 亦可：

```bash
# 未見卡協議（v7／v8）
python3 tools/render_wordless_scenes.py --cards data/mixed-card-object/wordless-v1/cards-padded \
  --manifest data/mixed-card-object/wordless-v1/cards.csv --out /tmp/ds \
  --balance-train-per-class 1200 --balance-val-per-class 24 \
  --bg-dir data/mixed-card-object/images/train/physical \
  --extra-train data/mixed-card-object/images/train/physical --extra-repeat 8
python3 tools/train_card_classifier.py --data /tmp/ds --out runs/wordless-padded --epochs 20 --batch 64 --patience 7
```

產物：`model.pt`（best）、`model-float32.onnx`、`model-int8.onnx`、`metrics.json`。

## 限制

- 驗證集與訓練集同源（同 38 張卡面渲染），不代表實物或新卡表現。
- 未在 S31 實板載入，未量 RAM／PSRAM／推論延遲。
- 卡片身分切分只有 8 張 held-out 卡，每個數字約 ±4%，比較版本時要看差異是否大於此。
