# wordless-unknown：五類模型（加 `unknown` 出口）

讀者：模型與韌體整合人員。何時讀：取用五類模型、替換現行 `models/wordless-padded/wordless_s31.espdl`、或核對 `classes.json` v3.0.0 時。

## 內容

| 檔案 | 說明 |
|---|---|
| `model-float32.onnx` | 五類 float32，opset 18，輸入 `images [1,3,160,160]`，輸出 `output0 [1,5]` |
| `wordless_s31.espdl` | ESP32-S31 w8a8，1,796,592 bytes，檔頭 `EDL2`；**交付物** |
| `wordless_s31.metrics.json` | 量化設定、5,791 張留起場景（4,698 卡面 ＋ 1,093 空場景）的 macro-F1 與逐類 F1 |
| `wordless_s31.json` | ESP-PPQ 匯出設定（逐層 bit width／observer） |
| `labels.txt` | `food, hazardous, other, recyclable, unknown`，與 `contracts/model/classes.json` v3.0.0 同序 |
| `SHA256SUMS` | 上述檔案與 `labels.txt` 的 sha256 |

`models/wordless-padded/` 的重訓會使這裡的五類模型過時：第五類是在四類 `Gemm` 之上加一行，
所以要**先更新四類，再用新 ONNX 重新導出**。產生方法：

```sh
# 空場景放獨立目錄，唔好放入四類資料集（會令 build.json 同資料夾唔一致）
.venv-train/bin/python tools/render_none_scenes.py --dataset /tmp/unknown-root \
  --train 6200 --val 1093 --holdout 300
# 四類 ONNX + 空場景 → 五類 ONNX（四個已知 row 不變）
.venv-train/bin/python tools/add_unknown_row.py \
  --model models/wordless-padded/model-float32.onnx \
  --data models/wordless-padded/dataset \
  --unknown-dir /tmp/unknown-root/train/unknown \
  --out models/wordless-unknown/model-float32.onnx
# 校準與測試要五類 ImageFolder；用 symlink 併一個出來，唔改動四類資料集
mkdir -p /tmp/c5 /tmp/v5
for c in food hazardous other recyclable; do
  ln -s "$PWD/models/wordless-padded/dataset/train/$c" /tmp/c5/$c
  ln -s "$PWD/models/wordless-padded/dataset/val/$c" /tmp/v5/$c
done
ln -s /tmp/unknown-root/train/unknown /tmp/c5/unknown
ln -s /tmp/unknown-root/val/unknown /tmp/v5/unknown

.venv-train/bin/python tools/quantize_espdl.py \
  --onnx models/wordless-unknown/model-float32.onnx \
  --calib /tmp/c5 --calib-images 1200 --test /tmp/v5 \
  --out models/wordless-unknown/wordless_s31.espdl --target esp32s31 \
  --bias-correct --equalization --save-native --force
```

## 與前一代的分別

四類模型對「完全冇卡」的畫面會硬判成某一類：2026-11-14 舊基座，300 張程序化空場景中，中位信心 0.970、283 張仍過 0.55 門檻。空場景同真卡的分數貼埋一齊，任何門檻都分唔開。

第五類 `unknown` 排最後（`recyclable` < `unknown`），前四個 index 與桶位完全不變（`1,2,3,0`）；`unknown` 對應 `STC_BIN_COUNT`：不開桶、`stc_label_t.abstain = true`、由 `stc_game` 重試一次。

第五行係直接改 ONNX `Gemm`（4 行擴到 5 行），四個已知 row 同其他 initializer 原封不動；唔係重訓。詳見[實驗記錄](../../docs/experiments/mixed-card-object/unknown-v1/README.md)。

## 量度（host，非實機；2026-11-19 由新四類基座重新導出）

5,791 張留起場景 = 4,698 卡面（158 張卡全部見過）＋ 1,093 空場景：

| 量度 | float32 | `.espdl`（w8a8，equalisation ＋ bias correction） |
|---|---|---|
| 全部正確 | 5,786/5,791（0.9991） | 5,755/5,791（0.9939 macro-F1） |
| 卡面場景被判 `unknown` | 3/4,698 | 28/4,698 |
| 空場景判 `unknown` | 1,093/1,093 | 1,090/1,093 |

逐類 F1（`.espdl`）：food 0.9968、hazardous 0.9936、other 0.9919、recyclable 0.9921、unknown 0.9950。

**空場景 holdout 300 張（只量 false-accept，永不訓練）**：`.espdl` 296/300 判 `unknown`、
3 張過 0.55 門檻（1.0 %）；float32 299/300、1 張誤放（0.3 %）。

### 擷取包絡（模擬，issue #67）

同一批 1,216 張模擬擷取（38 張印刷卡 × 8 個 regime × 4 張，seed `20261119`）：

| regime | float32 | `.espdl` |
|---|---:|---:|
| nominal | 0.993 | 0.974 |
| tight | 0.961 | 0.921 |
| tight_pearl | 0.980 | 0.987 |
| held | 0.987 | 0.980 |
| isp | 0.993 | 0.961 |
| low_light | 0.993 | 0.980 |
| motion | 1.000 | 0.941 |
| jpeg60 | 1.000 | 0.980 |

被點名的八張卡（B-14 至 B-19、B-22、B-28）在 `.espdl` 上每個 regime 4/4，只有 B-16 在 `tight` 是 3/4。
int8 的殘餘錯誤多數落在 `unknown`（要求重放卡）而唔係錯桶：`tight` 的 12 個錯之中 10 個係 `unknown`。

**上面係場景切分，158 張卡全部見過；按卡片身份切分（fold 0）的數字屬上一代模型，未在新分佈重跑。**

## 未解決

- 數碼卡背（0.908）與數碼封面（0.626）仍判 `hazardous` 並過 0.55 門檻。它們是卡形狀的印刷物，特徵上與真卡太近。
- 只在 host 執行；S31 未載入、未量延遲、RAM、相機前處理一致性。
- 真實鏡頭整體 false-accept 未量，只有 1 張 S31 實拍空鏡頭（判 `unknown` 0.999）。
