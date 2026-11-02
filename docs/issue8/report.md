# Issue #8 — YOLO11n-cls 四類 → ONNX → esp-ppq int8 → esp-dl

**資料洩漏限制：** 原資料有 14 組相同內容跨 train、validation 或 test（11 train/test、2 val/test、1 train/val）；EI 上傳資料另有 5 組 train/test 重複；下列數字是原始觀察，可能偏高，不能證明獨立測試門檻。重建去重資料並重新訓練和評估後才可作正式比較。


## 環境

- Python 3.11（`.venv-train`）
- ultralytics 8.4.197、torch 2.15.0
- torchvision 0.29.2、onnx 1.19.1、onnxruntime 1.31.1、esp-ppq 1.3.14
- macOS Apple Silicon（訓練用 MPS）
- 完整 pip 版本鎖：[`../tools/requirements-train-macos-arm64.lock.txt`](../../tools/requirements-train-macos-arm64.lock.txt)（macOS arm64 / Python 3.11 專用）

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
  - `docs/issue8/confusion_matrix.png`
  - `docs/issue8/confusion_matrix_normalized.png`
- 訓練日誌：[`results.csv`](results.csv)；最佳與 normalized 混淆矩陣：[`confusion_matrix.png`](confusion_matrix.png)、[`confusion_matrix_normalized.png`](confusion_matrix_normalized.png)。
- 權重：`weights/best.pt`（3,193,090 B）、`weights/last.pt`；模型 binaries 不提交。

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
- 原始量化報告當時尚未量測 espdl 在實機上的延遲；#6 的 105 ms 是另一個模型（person_detect int8）在 ESP-NN 下的數字，不可直接比較。

## 後續：排除完全重複的測試圖

固定原本的 `best.pt` / `best.onnx`，未根據測試結果調參。從原始來源以 SHA-256 去重，產生 980 張測試圖；再次核對這 980 張與原訓練及驗證圖的完全相同 SHA-256 數為 0。原模型仍曾在內含 train/val 重複圖的分割上選模；近重複與同場景來源亦未排除。

這 980 張上 PyTorch 與 ONNX 預測一致率 100%，macro-F1 為 0.862707；逐類結果與混淆矩陣見 `../yolo-test-dedup.json`。這是排除已知完全重複後的補充評估，並非重新訓練、ESP-DL 量化評估或真實場景驗收。

## 去重測試集上的重做量化評估

使用相同 ONNX 權重及 `docs/yolo-evaluation-manifest.json` 指定的校準/測試影像：200 張 calibration 只來自 train split；980 張 test 完全排除已知 SHA-256 重複。重算前逐檔核對 manifest hash，200/200 與 980/980 均相符。更新後的 `tools/quantize_espdl.py` 以鎖定的 Python 3.11 環境執行，`classify_transforms` 固定使用 Resize、CenterCrop、ToTensor、Normalize，沒有隨機翻轉；校準和評估使用相同 transform。

| 指標 | 最新 review-fix rerun |
|---|---:|
| calibration / test images | 200 / 980 |
| test macro-F1 | **0.858549** |
| `.espdl` size | **1,796,864 B** |
| `.espdl` SHA-256 | `05361838193d2edf9d32ce616c92d926b08036d53090fa2d6f89a6605b8a168c` |
| ONNX SHA-256 | `1e209726e0fa21ee52c4828b4b308bad2b05eec4a5d8f5cdc4ac18082cca2f70` |

ESP-PPQ TorchExecutor host simulation 的逐類 F1：其他 0.7967、可回收物 0.8473、廚餘 0.9307、有害 0.8595。逐類 confusion matrix 載於 `docs/yolo-espdl-dedup.json`。這是主機模擬，不代表 S31 實機延遲或準確率。

最新 `.espdl` 產物保存在執行者的 workflow artifacts，不提交至 repository，符合本 issue 不提交 `.espdl` 的要求。前兩次記錄的同指標 rerun 曾產生 1,796,880 B / SHA-256 `3f369b0e8572abb49166bcd7455cac9454bb0e3f084b3d252e4880ca8f2c498e`，以及 1,796,768 B / SHA-256 `83bc961bc4dd1b7268393b250a53262b26d45b9b92fb9212d3af58f23289ed92`；今次重算為 1,796,864 B。三次 macro-F1 一致至六位小數，但 binary size/hash 有差異，故以本節最新 hash 標識本次產物，不假設不同執行會逐位相同。

原始 1,000 張測試集的首次量化結果仍保留在上方三方表格，macro-F1 為 0.859411；它與本節去重後的 980 張結果不是同一測試集。近重複、同一場景及 train/validation 的重複選模仍未排除，因此不能把現有測試當成完全獨立的產品驗收。

## 與 Edge Impulse 共用測試集比較

在同一份 980 張 SHA-256 去重 test manifest 上，EI MobileNetV2 0.5 int8 TFLite 以 LiteRT 2.3.0 host 評估的 macro-F1 為 **0.805371**（accuracy 0.807143）；本模型 PyTorch/ONNX macro-F1 為 **0.862707**，ESP-PPQ 模擬為 **0.858549**。同圖 EI 與 YOLO ONNX 的 macro-F1 差為 5.73 個百分點，量化 ESP-PPQ YOLO 與 EI 差為 5.32 個百分點。EI 完整結果及 confusion matrix 見 [`../ei-baseline/shared-dedup-test.json`](../ei-baseline/shared-dedup-test.json)。

因此同一測試集的準確率證據目前偏向 YOLO11n-cls，適合作為 S31 `.espdl` 實板候選；這不代表 ESP-PPQ 主機模擬已在 S31 達到該精度或速度。EI TFLite 原先只驗證 S3 零輸入 p95 858.635 ms；後續 S3 YOLO 實測見下節。兩者都未以實際相機至開蓋流程驗收 3 秒 p95、4.3 吋畫面、斷網流程或連續 10 次。EI host 前處理有 18 張小圖採邊界 edge clamp，詳細限制及建議見 [`../model-comparison.md`](../model-comparison.md)。

## ESP32-S3 專用量化與板端 smoke test

以相同 200 張 calibration 及 980 張去重 test manifest 重新量化為 `esp32s3`。ESP-PPQ 主機模擬 macro-F1 **0.796771**，輸出 1,715,696 B。S31 目標產物與 S3 產物不同；不可互換。

ESP32-S3、240 MHz、ESP-IDF 6.0.2、8 MB PSRAM：ESP-DL 載入及內建 `Model::test()` 通過。四張選定圖各代表一類，單核心輸出全部符合標籤。每張 20 次 Invoke，p95 為 **247.807–247.867 ms**；每次重載輸入的時間不計在內。測試亦不包括拍照、resize、屏幕及開蓋；四張圖不能估計準確率。
