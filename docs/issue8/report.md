# Issue #8 — YOLO11n-cls 四類 → ONNX → esp-ppq int8 → esp-dl

**資料洩漏限制：** 原資料有 14 組相同內容跨 train、validation 或 test（11 train/test、2 val/test、1 train/val）；EI 上傳資料另有 5 組 train/test 重複；下列數字是原始觀察，可能偏高，不能證明獨立測試門檻。重建去重資料並重新訓練和評估後才可作正式比較。


## 環境

- Python 3.11（`.venv-train`）
- ultralytics 8.4.197、torch 2.15.0
- esp-ppq 1.3.14
- macOS Apple Silicon（訓練用 MPS）

## Step 2 — 訓練

實際指令（與 issue 相同，只多了 `device=mps seed=42`，patience 設 40 讓 40 epoch 跑完）：

```bash
.venv-train/bin/yolo classify train data=./data/waste4 model=yolo11n-cls.pt \
  imgsz=160 epochs=40 batch=32 device=mps patience=40 seed=42 \
  project=./runs/waste4 name=cls160
```

Ultralytics 會自動在 project 前加 `runs/classify/`，所以實際輸出路徑是：

```
runs/classify/runs/waste4/cls160/
```

`results.csv` 最後一行：

```
epoch,time,train/loss,metrics/accuracy_top1,metrics/accuracy_top5,val/loss,lr/pg0,lr/pg1,lr/pg2
40,1396.8,0.06819,0.89333,1,0.49823,4.34375e-05,4.34375e-05,4.34375e-05
```

- 最佳 epoch 37：top1 = 0.90、val/loss = 0.45199。
- 混淆矩陣：
  - `runs/classify/runs/waste4/cls160/confusion_matrix.png`
  - `runs/classify/runs/waste4/cls160/confusion_matrix_normalized.png`
- 權重：`weights/best.pt`（3,193,090 B）、`weights/last.pt`。

## Step 3 — ONNX

```bash
.venv-train/bin/yolo export model=runs/classify/runs/waste4/cls160/weights/best.pt \
  format=onnx imgsz=160 opset=13
```

- 檔案：`runs/classify/runs/waste4/cls160/weights/best.onnx`
- 大小：**6,162,676 B**
- 輸入 shape：`[1, 3, 160, 160]`（由 `onnx.load` 讀出）
- onnxslim 已套用，匯出耗時 7.1 s。

## Step 4 — 量化前 parity

50 張分層 test 抽樣：

```bash
.venv-train/bin/python tools/check_onnx_parity.py \
  --pt runs/classify/runs/waste4/cls160/weights/best.pt \
  --onnx runs/classify/runs/waste4/cls160/weights/best.onnx \
  --data data/waste4/test --n 50 --imgsz 160 --device cpu --batch 16 \
  --seed 42 --report docs/yolo-onnx-parity.json
```

- **top-1 一致率 100%**（50/50），mismatches 為空。
- 50 張抽樣的 macro-F1 兩邊同為 0.83936。

全量 1,000 張 test（`--n 0`，report `docs/yolo-test-full.json`）：

- **top-1 一致率 100%**（1,000/1,000）。
- PyTorch / ONNX macro-F1 同為 **0.863353**。

## Step 5 — esp-ppq int8

```bash
.venv-train/bin/python tools/quantize_espdl.py \
  --onnx runs/classify/runs/waste4/cls160/weights/best.onnx \
  --calib data/waste4/train --test data/waste4/test \
  --out runs/classify/runs/waste4/cls160/best.espdl \
  --imgsz 160 --calib-images 200 --target esp32s31 --force
```

- 成功，無不支援算子。
- 量化設定：`w8a8`、200 張 calibration（只取 train split，分層抽樣，seed 42）、`QuantizationSettingFactory.espdl_setting()`。
- 產物：
  - `runs/classify/runs/waste4/cls160/best.espdl` — **1,796,816 B**
  - `runs/classify/runs/waste4/cls160/best.json` — 767,767 B（ESP-DL graph）
  - `runs/classify/runs/waste4/cls160/best.metrics.json` — 682 B

## Step 6 — 三方對比

| 指標 | PyTorch `best.pt` | ONNX `best.onnx` | espdl int8 `best.espdl` |
|---|---:|---:|---:|
| test macro-F1 | 0.863353 | 0.863353 | **0.859411** |
| 檔案大小 | 3,193,090 B | 6,162,676 B | 1,796,816 B（+ graph json 767,767 B） |
| 其他 F1 | 0.80860 | 0.80860 | 0.79678 |
| 可回收物 F1 | 0.83462 | 0.83462 | 0.84444 |
| 廚餘 F1 | 0.92683 | 0.92683 | 0.93177 |
| 有害 F1 | 0.88337 | 0.88337 | 0.86465 |

混淆矩陣（列 = 真值，欄 = 預測；順序：其他、可回收物、廚餘、有害）：

```
PyTorch / ONNX          espdl int8
[[188, 28,  8, 26],     [[198, 22, 11, 19],
 [ 12,217,  6, 15],      [ 19,209, 11, 11],
 [  4, 17,228,  1],      [  2,  8,239,  1],
 [ 11,  8,  0,231]]      [ 28,  6,  2,214]]
```

## 結論

原始量化模擬 macro-F1 為 0.8594；由於重複圖片跨分割，不能視為達到獨立測試門檻。 ONNX 匯出無損（1,000 張一致率 100%），int8 量化只掉 0.0039。最大退化在「其他」（−0.0118）與「有害」（−0.0187），「可回收物」與「廚餘」反而略升。

## 備註

- test set 只在最後評估使用，未用於調參；calibration 只取 train split。
- `.pt` / `.onnx` / `.espdl` 均未 commit（`.gitignore` 已排除 `runs/classify/`）。
- 尚未量測 espdl 在實機上的延遲；#6 的 105 ms 是另一個模型（person_detect int8）在 ESP-NN 下的數字，不可直接比較。
