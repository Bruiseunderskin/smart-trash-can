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
2. **每張卡 200 張場景**：隨機角度、透視、光線、眩光、色偏、動態模糊、陰影，背景係真實相片。
3. **加真實相片**：470 張持牌相片直接入訓練集（每張一次）。冇 food／hazardous 嘅持牌真實相，所以
   呢兩類只有渲染圖。

珠光紙效果燒在 sprite 上，唔係場景噪聲：卡係實物，材質屬於卡本身。成品參考見
`docs/cards/300dpi` 對應的實印卡（60×60 mm、珠光紙、單面印刷、激光切割）。

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
  --data models/wordless-padded/dataset --out runs/wordless-padded \
  --epochs 12 --batch 64 --size 160 --patience 4 --device mps --seed 20261111
```

## 結果

### 已知卡、未見場景（部署情境）

4,738 張 val 場景（158 張卡全在訓練集，場景唔重疊）：

| 指標 | float32 | int8 `.espdl` |
|---|---:|---:|
| 正確 | 4,736/4,738 | 4,730/4,738 |
| top-1 | 0.9996 | 0.9983 |
| macro-F1 | 0.9996 | 0.9983 |

訓練 10 epoch（早停），`T = 0.5`，ECE 0.0004 → 0.0007（同一批驗證集，偏樂觀）。
int8 的 8 個錯誤：food→other 1、hazardous→other 2／→recyclable 1、other→recyclable 2、recyclable→other 2。
量化：`tools/quantize_espdl.py`、w8a8、`target=esp32s31`、240 張 train 影像校準、`--save-native`。

### 未見卡（fold 0：34 張 held-out 卡面）

同一支 renderer、同一批背景，把 fold 0 的 34 張卡（10 張印刷 ＋ 24 張輔助）整張抽出，1,360 張場景：

| 指標 | 數值 |
|---|---:|
| top-1 | **0.739** |
| macro-F1 | **0.7378** |
| ECE（原始 → 校準，T = 3.55） | 0.1942 → 0.0404 |

| 真實 \ 預測 | food | hazardous | other | recyclable | 正確 |
|---|---:|---:|---:|---:|---:|
| food（320） | 252 | 14 | 18 | 36 | 252 |
| hazardous（320） | 40 | 238 | 18 | 24 | 238 |
| other（320） | 56 | 34 | 211 | 19 | 211 |
| recyclable（400） | 24 | 72 | 0 | 304 | 304 |

對照上一版（38 張卡、無珠光紙、無真實相片、每卡 200 張以下）：0.5465，而且 hazardous 3/150 全塌去 recyclable。
今次錯誤分散在四類，唔再單一塌陷。完整記錄見 `unseen-fold0.metrics.json`。

**部署含意**：`wordless_s31.espdl` 用全部 158 張卡訓練，1.000 只覆蓋「已知卡」。加新卡設計之後要重跑
fold 驗證，唔可以沿用呢個數字。

## 限制

- 驗證集同訓練集同源（同一批卡面渲染），只證明流程通同量化冇明顯掉點。
- `hazardous` 同 `food` 只有 37 張卡、冇持牌真實相，仍然係最弱嘅兩類。
- 未在 S31 實板載入：未量 RAM／PSRAM、未量延遲 p50／p95。
- 未有印刷卡實拍：珠光紙實物在相機、手持、反光下嘅表現冇數字。
