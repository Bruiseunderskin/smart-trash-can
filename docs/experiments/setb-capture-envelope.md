# Set B 誤判的根因：擷取包絡，不是先驗

讀者：模型、韌體、卡片設計與驗收人員。何時讀：有人報「某幾張卡經常判錯」、要改模型輸入分佈、或要重跑 issue #67 之前。

## 症狀

隊友實測 Set B：廚餘卡（B-14 至 B-18）、藥物卡（B-22）、鏡片濕紙巾鋁箔包（B-28）、乾電池，經常被判成**可回收**。

## 已排除的假設

| 假設 | 結果 |
|---|---|
| 判決層先驗偏置 | PR #71 掃 `tau` 只得 +2.5 點，B-22 由 0.50 升到 0.53。不是主因。 |
| ONNX 前處理錯（ImageNet 正規化） | PR #72 已修工具；修好之後同一批 120 張由 0.625 變 1.000。是工具錯，不是模型能力。但同一錯誤仍留在 int8 校準路徑（見下）。 |
| 訓練分佈內準確率不足 | 舊分佈 4,736/4,738。分佈內沒有問題，所以量測方式本身看不到實拍。 |

## 根因（量測）

模型只在「卡佔畫面 52–78%、背景可見、JPEG 82–94」的 render 上可靠。把同一批 38 張印刷卡逐步推向實拍，準確率是階梯式下跌：

| 輸入 | float32 | int8 |
|---|---:|---:|
| render（卡 52–78 %，有背景） | 1.000 | 1.000 |
| 卡 75 % ／ 85 % 珍珠 sprite，白底 | 1.000 | 0.947 |
| 卡填滿畫面（珍珠 sprite，白底） | 0.684 | **0.526** |
| 卡填滿畫面（印刷面，白底） | 0.974 | 0.737 |
| 卡填滿畫面 + 銳化 | 0.895 | 0.706 |
| 卡填滿畫面 + JPEG q70 | 1.000 | 0.763 |

`hazardous → recyclable` 在填滿畫面的情況下出現 18 次（每類 42 張），`hazardous` int8 是 0/42。錯誤的信心中位數 0.88–0.99，所以**拒判救不了**：模型是自信地錯。

兩個獨立缺陷：

1. **訓練分佈沒有「卡貼邊」的樣本。** `render_card_scenes.render()` 的 scale 只到 0.78，而 `train_card_classifier.py` 的 `--scale/--translate/--erasing` 預設全 0，即 Ultralytics 的 RandomResizedCrop 不會放大。模型因此把「背景存在」當成特徵；小朋友把卡舉到鏡頭前就沒有背景。
2. **int8 校準餵錯輸入空間。** `quantize_espdl.py` 與 `evaluate_espdl_native.py` 用 `classify_transforms`，它做 `/255` 之後**再加 ImageNet 正規化**；導出圖由 `Conv` 開始、要的是 0–1 RGB。校準決定每層 activation scale，餵錯空間等於用錯的動態範圍量化。

## 修法（不需要實卡）

1. renderer：scale 由 `0.52–0.78` 改為 `0.45–1.15`（卡可貼邊、可出框，模擬 4:3 轉正方形的中央裁切）；每場景加珠光反光（0–16 % 亮度、隨機方向），因為真實珠光紙的反光是隨視角變，而 `pearl_paper()` 是把一個反光烘進 sprite、變成卡片身份的線索；加手機 ISP（unsharp 50–150 %、飽和 1.0–1.25、對比 1.0–1.12，p=0.5）與 sensor noise（σ 1–5）。
2. 訓練時開 `--scale 0.25 --translate 0.05 --erasing 0.1`。
3. int8 校準改用 `tools/stc_preprocess.py`（center-crop-square → bilinear → 0–1 RGB），與部署一致。
4. 回歸閘：`tools/evaluate_capture_stress.py` 八個 regime，任何一個低於門檻即 exit 1。

### 缺陷 2 的隔離量測

同一個 `model-float32.onnx`、同一批 114 張 `tight_pearl` 影像，只改量化校準的輸入空間：

| 校準／評分輸入 | macro-F1 | food | hazardous | other | recyclable |
|---|---:|---:|---:|---:|---:|
| 0–1 RGB（部署空間） | **0.681** | 0.727 | 0.444 | 0.656 | 0.897 |
| ImageNet 正規化（舊工具） | 0.493 | 0.250 | 0.385 | 0.531 | 0.807 |

所以 int8 在實拍上比 float32 差得多，部分不是模型本身，而是校準餵錯輸入空間。上表仍是 `main` 上那個舊 float32 圖，即模型本身的缺點未修；兩件事要分開看。

## 驗收方式

`tools/evaluate_capture_stress.py` 對 38 張印刷卡生成八個 regime（`nominal`／`tight`／`tight_pearl`／`held`／`isp`／`low_light`／`motion`／`jpeg60`），同一批 1,216 張影像，seed 固定 `20261119`，每張卡每個 regime 4 張；四個模型跑同一批影像。**這是模擬，不是印刷卡實拍，也不是現場準確率。**

## 修正前（`main` @ PR #72，1,216 張／模型）

| regime | float32 | int8 |
|---|---:|---:|
| nominal（新分佈，屬分佈外） | 0.974 | 0.934 |
| tight | 0.875 | 0.678 |
| tight_pearl | 0.711 | 0.618 |
| held | 1.000 | 0.980 |
| isp | 0.934 | 0.888 |
| low_light | 0.961 | 0.901 |
| motion | 0.947 | 0.816 |
| jpeg60 | 0.974 | 0.914 |

## 修正後（同一批 1,216 張，seed 20261119）

| regime | 舊 float32 | 舊 int8（.onnx） | 新 float32 | 新 `.espdl` |
|---|---:|---:|---:|---:|
| nominal | 0.974 | 0.934 | 0.993 | 0.980 |
| tight | 0.875 | 0.678 | **1.000** | 0.967 |
| tight_pearl | 0.711 | 0.618 | **1.000** | **0.993** |
| held | 1.000 | 0.980 | 1.000 | 1.000 |
| isp | 0.934 | 0.888 | **1.000** | 0.974 |
| low_light | 0.961 | 0.901 | **1.000** | 1.000 |
| motion | 0.947 | 0.816 | **1.000** | 0.961 |
| jpeg60 | 0.974 | 0.914 | **1.000** | 1.000 |

八張被點名的卡（B-14 至 B-19、B-22、B-28）在新 `.espdl` 上：**每一 regime 都是 4/4，只有 B-19 在 `tight` 是 3/4**。舊 int8 在同一批是 B-17 1/4（`tight_pearl`）、B-19 1/4（`held`）、B-22 3/4（`held`）。

已知卡、未見場景（4,698 張 val，新分佈）：

| | 正確 | macro-F1 | ECE |
|---|---:|---:|---:|
| float32 | 4,696/4,698 | 0.9995 | 0.0012 → 0.0004（T=0.5） |
| `.espdl` w8a8 | 4,692/4,698 | 0.9977（6 個錯） | 未量（host PPQ 圖模擬） |

### int8 為甚麼之前咁差，同埋點樣修

三個獨立因素，逐個量：

1. **校準餵錯輸入空間**（ImageNet 正規化 vs 0–1）。同一圖、同一批 114 張 `tight_pearl`：macro-F1 0.493 → 0.681。
2. **校準影像太少**（240）：改 1,200 後 val macro-F1 0.9952 → 0.9963，但 stress 無明顯改善。
3. **量化本身**：加 ESP-PPQ 的 cross-layer equalisation ＋ bias correction，val macro-F1 **0.9977**，`tight_pearl` 由 0.928 → 0.993、`held` 0.934 → 1.000、`tight` 0.750 → 0.967。呢一步係把 int8 拉到達標的關鍵。

加大校準集（240 → 1,200）同把擷取包絡影像加入校準都試過：兩者都無 equalisation 咁有效，後者反而令 stress 變差（`tight` 0.750 → 0.697），所以最終校準只用訓練集 render。

## 重跑步驟

```sh
# 1. 重建資料（約 22 分鐘）：卡面 → 珠光 sprite → 31,600 個場景 ＋ 470 張持牌實拍
.venv-train/bin/python tools/build_wordless_padded.py --force

# 2. 重訓（scale／translate／erasing 一定要開，否則 renderer 的放大樣本會被訓練期裁切抵銷）
.venv-train/bin/python tools/train_card_classifier.py \
  --data models/wordless-padded/dataset --out runs/wordless-padded-envelope \
  --epochs 12 --batch 64 --size 160 --patience 4 --device mps --seed 20261111 \
  --scale 0.25 --translate 0.05 --erasing 0.1

# 3. 量化：校準用 0–1 RGB（--normalise 預設 div255），1,200 張，開 equalisation ＋ bias correction
#    （本次校準池是 dataset/train 每類 400 張的切片，1,600 張取 1,200；直接指向 dataset/train 等效但抽樣不同）
.venv-train/bin/python tools/quantize_espdl.py \
  --onnx models/wordless-padded/model-float32.onnx \
  --calib models/wordless-padded/dataset/train --test models/wordless-padded/dataset/val \
  --out models/wordless-padded/wordless_s31.espdl \
  --calib-images 1200 --target esp32s31 --bias-correct --equalization --save-native --force

# 4. 回歸閘：八個 regime 任何一個低於 0.95 即 exit 1
.venv-train/bin/python tools/evaluate_capture_stress.py \
  --cards models/wordless-padded/cards --sprites models/wordless-padded/cards-padded \
  --cards-csv models/wordless-padded/cards.csv --backgrounds models/wordless-padded/real-photos \
  --models models/wordless-padded/model-float32.onnx,models/wordless-padded/model-int8.onnx \
  --n 4 --out /tmp/setb/stress --fail-under 0.95
```

## 與五類模型（`models/wordless-unknown/`）的關係

韌體契約是五類（`contracts/model/classes.json` v3.0.0、`STC_MODEL_CLASSES 5`），第五類 `unknown` 由
`tools/add_unknown_row.py` 在**四類 `Gemm` 之上**加一行，四個已知 row 原封不動。所以四類模型是根：
四類塌陷，五類一齊塌。實測 `models/wordless-unknown/model-float32.onnx` 在 `tight` 0.855、`tight_pearl`
0.724，同舊四類同源同病。

重訓四類之後，五類要**用新導出的四類 ONNX 重新產生**：`render_none_scenes.py` 補回 `train/unknown`，
再跑 `add_unknown_row.py`，然後重新量化。舊 `models/wordless-unknown/` 的數字在新四類之上不再成立。

## 限制

- 沒有印刷卡實拍，所有數字都是模擬擷取。珠光紙仍是一個模型（半透墨、閃粉、寬反光），換一種紙就會偏移。
- 珠光 sprite 由卡面 hash 決定，每張卡固定一個反光；實物反光隨燈光變。renderer 已加每場景反光去抵銷這個線索，但未在實物上驗證。
- `tight_pearl` 用 `cards-padded/` 的 709 px sprite 直接縮到 160，與相機由 640×480 起算的光學不同。
