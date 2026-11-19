# wordless-padded：印刷卡分類模型與唯一資料集

讀者：模型、韌體與卡片設計人員。何時讀：取用分類模型、重建訓練資料，或追查舊資料集問題前。

呢個目錄同時係**模型產物**同**訓練資料唯一 source of truth**。舊路徑（`data/mixed-card-object/`、
`models/mixed-card-object/`）已移除，見 [`data/README.md`](../../data/README.md)。

## 內容

| 路徑 | 說明 |
|---|---|
| `cards.csv` | 158 張卡的身份：`card_id`、`class`、`kind`（`printed`／`auxiliary`）、`fold`、SHA-256、來源 |
| `cards/` | 158 張 709×709 卡面。`printed` 38 張與 `docs/cards/300dpi` 逐張 byte-identical |
| `cards-padded/` | 剪影 sprite：由白紙底裁出，圓角外透明，再上**珠光紙**（半透墨、閃粉、大範圍珠光）。**唔入 git**，可重建 |
| `real-photos/` | 470 張持牌真實相片：`data/field-captures` 33 張（自有）＋ TrashNet 437 張（MIT） |
| `real-photos/real-photos.csv` | 逐張來源、授權、SHA-256 |
| `DATA-LICENSES.md` | 為甚麼只有呢啲圖片、邊啲明確唔用 |
| `dataset/` | 渲染場景：158 張卡各 200 張，加真實相片（**唔入 git**，可重建） |
| `dataset.csv` | 逐張場景的 split、card_id、seed、SHA-256 |
| `labels.txt` | 輸出次序 `food, hazardous, other, recyclable`（＝模型 index＝`contracts/model/classes.json`） |
| `wordless_s31.espdl` | ESP32-S31 int8 w8a8 量化模型 |

重建全部資料：

```sh
uv venv .venv-train --python 3.11
uv pip install --python .venv-train/bin/python -r tools/requirements-train-macos-arm64.lock.txt
.venv-train/bin/python tools/fetch_real_photos.py --force   # 需要網絡，只做一次
.venv-train/bin/python tools/build_wordless_padded.py --force
```

`dataset/` 唔入 git：31,600 張場景可以逐張重建，`dataset.csv` 記錄每張的 seed 同 hash。

## 資料怎樣組成

1. **卡面 158 張**：38 張印刷卡（Set A／B）＋ 120 張輔助卡面（Set C）。Set C 由 Hermes 產生，
   專門用嚟教「未見過嘅卡設計」，**唔會印**。
2. **每張卡 200 張場景**：卡佔畫面 **0.45–1.15**（可以貼邊、可以出框），隨機角度、透視、光線、
   眩光、色偏、動態模糊、陰影、感測器噪聲；一半場景再經手機 ISP（銳化、飽和、對比）。背景係真實相片。
   2026-11-19 之前只到 0.52–0.78，令模型依賴「背景可見」，卡一填滿畫面就塌入 `other`／`recyclable`，
   見 [`docs/experiments/setb-capture-envelope.md`](../../docs/experiments/setb-capture-envelope.md)。
3. **加真實相片**：470 張持牌相片直接入訓練集（每張一次）。冇 food／hazardous 嘅持牌真實相，所以
   呢兩類只有渲染圖。

珠光紙效果燒在 sprite 上，唔係場景噪聲：卡係實物，材質屬於卡本身。成品參考見
`docs/cards/300dpi` 對應的實印卡（60×60 mm、珠光紙、單面印刷、激光切割）。因為烘死在 sprite 的反光
會變成「卡片身份」線索（實物反光隨燈光變），renderer 另外在每個場景加一層隨機方向的寬反光。

## 修正咗乜

| 問題 | 處理 |
|---|---|
| 舊 `cards-padded` 加綠色外框、圖案放大到內框 78%，同實際印刷版唔同 | 改為印刷版面剪影，只裁走白紙底 |
| 舊版冇材質，白底卡太「乾淨」 | sprite 加珠光紙效果 |
| 舊「實物相」係 396 張帶水印嘅淘寶商品圖，來源同授權未確認 | 全部剔除；改用 `data/field-captures`（自有）同 TrashNet（MIT） |
| 舊細卡場景（卡只佔畫面三成、白紙底） | 唔再用；改由 `tools/render_wordless_scenes.py` 產生 |
| 類別偏斜（recyclable 佔 16/38） | 加入 Set C 後 46/158，每張卡渲染數相同，偏差由 2.1:1 降到 1.24:1 |

Set C 本身**唔係**污染：佢係為未見卡泛化而設。之前把它同無授權商品圖一齊移除係判斷錯誤，
現已還原，並在 `cards.csv` 用 `kind` 分開。

## 資料規則

- 卡面只可以來自 `docs/cards/300dpi`（`printed`）或已記錄來源的 `auxiliary` 卡面。要加卡就同時改
  卡檔、`cards.csv` 同 `DATA-LICENSES.md`。
- 真實相片只可以用授權清楚的來源；逐張記入 `real-photos.csv`。`data/field-probe` **永不**入訓練。
- 類別固定：`food` → 桶 1、`hazardous` → 桶 2、`other` → 桶 3、`recyclable` → 桶 0。模型 index 唔等於桶位。
- 部署模型按場景切分（同一張卡可同時出現在 train 同 val），只量「已知卡」。未見卡要用 `--fold` 按卡身份切分。

## 訓練設定

YOLO11n-cls、160×160、AdamW、lr0 1e-4、batch 64、seed 20261111、MPS。

```sh
.venv-train/bin/python tools/train_card_classifier.py \
  --data models/wordless-padded/dataset --out runs/wordless-padded-envelope \
  --epochs 12 --batch 64 --size 160 --patience 4 --device mps --seed 20261111 \
  --scale 0.25 --translate 0.05 --erasing 0.1
```

`--scale/--translate/--erasing` 唔可以再留 0：renderer 雖然已把卡放大到 1.15 倍畫面，但訓練期的
RandomResizedCrop 預設係 1.0（即不縮放），放大樣本會被抵銷。

## 結果

### 已知卡、未見場景（部署情境）

4,698 張 val 場景（158 張卡全在訓練集，場景唔重疊；分佈為卡佔畫面 0.45–1.15、珠光反光、
手機 ISP、感測器噪聲）：

| 指標 | float32 | int8 `.espdl` |
|---|---:|---:|
| 正確 | 4,696/4,698 | 4,692/4,698 |
| top-1 | 0.9996 | 0.9987 |
| macro-F1 | 0.9995 | 0.9977 |

訓練 12 epoch、`T = 0.5`、ECE 0.0012 → 0.0004（同一批驗證集，偏樂觀）。
量化：`tools/quantize_espdl.py`、w8a8、`target=esp32s31`、1,200 張 train 影像校準（0–1 RGB，
與導出圖輸入一致）、`--bias-correct --equalization`、`--save-native`。冇 equalisation 的話
同一份權重 val macro-F1 只有 0.9963，`tight_pearl` 由 0.993 跌到 0.928。

### 擷取包絡（模擬，不需實卡）

`tools/evaluate_capture_stress.py`，38 張印刷卡 × 8 個 regime × 每格 4 張 = 1,216 張，seed
`20261119`。同一批影像亦用來跑上一代模型，所以係同影像 A/B：

| regime | float32 | `.espdl` |
|---|---:|---:|
| nominal | 0.993 | 0.980 |
| tight（卡填滿畫面，數碼卡面） | 1.000 | 0.967 |
| tight_pearl（同上，珠光 sprite） | 1.000 | 0.993 |
| held（卡 80–95 %，實拍背景） | 1.000 | 1.000 |
| isp（再加手機銳化／飽和） | 1.000 | 0.974 |
| low_light | 1.000 | 1.000 |
| motion | 1.000 | 0.961 |
| jpeg60 | 1.000 | 1.000 |

被隊友點名的八張卡（B-14 至 B-19、B-22、B-28）在新 `.espdl` 上每個 regime 都是 4/4，
只有 B-19 在 `tight` 是 3/4。舊模型在同一批：`tight_pearl` 0.711、B-17 1/4、B-19 1/4。
完整 before／after 見 [`docs/experiments/setb-capture-envelope.md`](../../docs/experiments/setb-capture-envelope.md)。

## 未見卡（fold 0）

**以下數字屬上一代模型（2026-11-11 分佈，卡只佔畫面 52–78 %），未在新分佈重跑。**
同協議重訓（未見 fold 0 的 34 張卡、1,360 張場景）當時得 top-1 0.739、macro-F1 0.7378、
ECE 0.1942 → 0.0404（T = 3.55），錯誤分散四類而唔係單一塌陷。

```
| 真實 \ 預測 | food | hazardous | other | recyclable | 正確 |
| food（320） | 252 | 14 | 18 | 36 | 252 |
| hazardous（320） | 40 | 238 | 18 | 24 | 238 |
| other（320） | 56 | 34 | 211 | 19 | 211 |
| recyclable（400） | 24 | 72 | 0 | 304 | 304 |
```

**部署含意**：`wordless_s31.espdl` 用全部 158 張卡訓練，0.9996 只覆蓋「已知卡」。加新卡設計之後
要重跑 fold 驗證，唔可以沿用任何一個數字。重跑方法：

```sh
.venv-train/bin/python tools/render_wordless_scenes.py --cards models/wordless-padded/cards-padded \
  --manifest models/wordless-padded/cards.csv --out /tmp/fold0-dataset --fold 0 --train-per-card 200
.venv-train/bin/python tools/train_card_classifier.py --data /tmp/fold0-dataset --out runs/fold0 \
  --epochs 12 --batch 64 --size 160 --device mps --seed 20261111 --scale 0.25 --translate 0.05 --erasing 0.1
```

## 限制

- 擷取包絡嘅八個 regime 全部係模擬，唔係印刷卡實拍，亦唔係現場準確率。實物表現仍然未測。
- 未有印刷卡實拍：珠光紙實物在相機、手持、反光下嘅表現冇數字。`tight` regime 用**數碼卡面**
  （無珠光紙紋理），係全表最難、亦最唔似實物嘅一格。
- `hazardous` 同 `food` 只有 37 張卡、冇持牌真實相，仍然係最弱嘅兩類；int8 的殘餘錯誤集中在
  B-17（魚骨）同 B-19（電池）。
- int8 對量化設定敏感：同一份權重，無 `--equalization` 時 `tight_pearl` 0.928、`held` 0.934。
  換量化工具或換 calibration 集之前要重跑 `tools/evaluate_capture_stress.py`。
- 未在 S31 實板載入：未量 RAM／PSRAM、未量延遲 p50／p95。`.espdl` 的數字全部係 host PPQ 圖模擬。
- 未見卡（fold 0）嘅泛化數字屬上一代模型，未在新分佈重跑。
