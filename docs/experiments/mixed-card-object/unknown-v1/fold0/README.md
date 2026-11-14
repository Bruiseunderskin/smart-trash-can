# 未見卡（fold 0）逐圖結果

讀者：模型驗證者。何時讀：核對第五類模型對「未見過的卡」的表現時。

1,360 張場景，來自 `--fold 0` 的 34 張未見卡（10 張印刷 ＋ 24 張輔助），renderer 與參數同 `models/wordless-padded/README.md` 的 fold 協定；seed 20261112。

| 檔案 | 內容 |
|---|---|
| `shipped-4class.csv` | shipped 四類模型（`models/wordless-padded/model-float32.onnx`）：1,358/1,360；只反映「卡見過」 |
| `shipped-5class.csv` | shipped 五類模型（`models/wordless-unknown/model-float32.onnx`）：1,358/1,360 |
| `float32-4class-retrained.csv` | 同協議重訓、未見過 fold 0：1,066/1,360 = 0.784 |
| `float32-5class.csv` | 同前加 1,000 張 `unknown` 一併重訓：1,085/1,360 = 0.798 |
| `int8-5class.csv`／`int8-5class.json` | 上者的 S31 w8a8 量化圖（ESP-PPQ CPU 模擬）：1,066/1,360 = 0.784 |
| `holdout-5class.csv` | 同一重訓模型打 300 張未訓練空場景：300/300 判 `unknown` |

重現：

```bash
# fold 0 驗證場景（34 張未見卡）
.venv-train/bin/python tools/render_wordless_scenes.py \
  --cards models/wordless-padded/cards-padded \
  --manifest models/wordless-padded/cards.csv --out runs/fold0 \
  --fold 0 --train-per-card 1 --val-per-card 40 \
  --bg-dir models/wordless-padded/real-photos --size 160 --seed 20261112
# 未見卡重訓（folds 1–4）
.venv-train/bin/python tools/render_wordless_scenes.py \
  --cards models/wordless-padded/cards-padded \
  --manifest models/wordless-padded/cards.csv --out runs/fold0t \
  --fold 0 --balance-train-per-class 800 --balance-val-per-class 40 \
  --bg-dir models/wordless-padded/real-photos \
  --extra-train models/wordless-padded/real-photos --extra-repeat 6 \
  --size 160 --seed 20261112
.venv-train/bin/python tools/render_none_scenes.py --dataset runs/fold0t \
  --train 1000 --val 160 --holdout 300 --size 160
.venv-train/bin/python tools/train_card_classifier.py --data runs/fold0t \
  --out runs/fold0t-model --weights yolo11n-cls.pt --epochs 10 --batch 64 \
  --patience 5 --size 160 --seed 7 --device mps
.venv-train/bin/python tools/evaluate_cards.py --model runs/fold0t-model/model-float32.onnx \
  --dir runs/fold0/val --out float32-5class.csv
```

四類對照組用同一份資料但排除 `unknown/` 目錄（`runs/fold0t-4cls`）。這些 `runs/` 目錄已被 `.gitignore` 排除。
