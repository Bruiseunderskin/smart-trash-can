# 第五類 `unknown`（冇卡）：量度、做法與界線

讀者：模型開發者、S31 整合者、審查者。何時讀：使用 `models/wordless-unknown/`、判斷「冇卡都判可回收物」是否已解決、或重做這條實驗時。

**症狀已解決（空場景不再被放行），但未在 S31 實機驗證，真實鏡頭的整體 false-accept 也未量。** 下面數字全部係 host 執行，場景係程序化合成。

## 症狀與診斷

用四類 `models/wordless-padded/model-float32.onnx` 打 300 張程序化空場景：

| 輸入 | 張數 | 中位信心 | 0.55 放行 |
|---|---:|---:|---:|
| 空場景（`unknown` holdout） | 300 | 0.970 | **283/300** |

四類 softmax 一定要揀一個，所以「冇卡」只能靠門檻拒絕。空場景的信心分佈同真卡一樣貼近 1.0，任何門檻都分唔開：調高只會開始拒判真卡。逐圖結果見 [`old-model-empty-scenes.csv`](old-model-empty-scenes.csv)。

## 做法

1. `tools/render_card_scenes.py` 加 `render_empty()`／`make_surface()`：程序化表面（白枱、水泥、木紋、垃圾桶內壁、布、牆、金屬、地磚），加傾斜 ±20°、剪切 ±0.15、brightness 0.35–1.60（10% 暗場景）、色溫、陰影漸層、0–2 個眩光熱點、模糊／運動模糊、雜訊 σ0–12、JPEG q40–95、10% 鏡頭被遮。
2. `tools/render_none_scenes.py` 生成 `unknown`：train 6,200、val 1,093（跟其他四類的最少者對齊）、holdout 300（只量 false-accept，永不訓練）。repo 的相片池全部係物件特寫，裁任何一塊都仍然拍到垃圾，所以表面一律程序生成；holdout 同 train／val 的分別係 RNG 流與表面參數。
3. `tools/add_unknown_row.py`：直接改 ONNX。取 final `Gemm` 之前那層 1,280 維特徵，用 logistic probe 擬合「冇卡」方向，再把 `Gemm` 的 weight／bias 由 4 行擴到 5 行；四個已知 row 同所有其他 initializer 原封不動，所以四個 logit 完全不變。
4. 唔重訓整個 head。Ultralytics 換 5 行 head 等於由零重學；在舊資料集上實測真實物件由 240/292 跌到 194/292，連凍結 BN 都因統計漂移再跌 20 張。呢條路已放棄，相關工具亦已移除。

**凍結四個 row 只保證佢哋嘅 logit 不變，唔保證真物件一定贏過第五行。** 第五行可以贏，所以 raw 同「接受後」兩個數一定要一齊報（見下表）。

## 結果（host）

| 量度（5,831 張留起場景） | float32 | int8（ESP-PPQ 圖模擬） |
|---|---|---|
| 卡面場景正確 | 4,736/4,738 | 4,690/4,738 |
| 卡面場景被判 `unknown`（拒判） | 0 | 40 |
| 空場景拒判 | 1,092/1,093 | 1,093/1,093 |
| 空場景 false-accept | 1（0.09%） | 0 |
| 接受後準確率（全部） | 4,736/4,739 = 0.999 | 4,690/4,698 = 0.998 |

`.espdl`（w8a8、esp32s31、1,796,240 bytes）在 1,500 張平衡測試集上 macro-F1 **0.9874**；逐類 F1：food 0.993、hazardous 0.998、other 0.985、recyclable 0.985、unknown 0.976。

同一 `.espdl` 的混淆矩陣（5,831 張，行真值、列預測）：`food` 1,088 對、5 判 `unknown`；`hazardous` 1,127 全對；`other` 1,114 對、1 判 `hazardous`、2 判 `recyclable`、28 判 `unknown`；`recyclable` 1,361 對、5 判 `other`、7 判 `unknown`；`unknown` 1,093 全部正確。

`models/wordless-padded/real-photos/`（470 張持牌相片，本身在訓練集內，唔係留起集）float32：接受 453 張、431 張正確（接受後 0.951）、17 張拒判。只可作參考，唔係準確率。

## 非卡物件負面檢查（float32）

| 輸入 | 結果 | 0.55 是否放行 |
|---|---|---|
| 真實 S31 空鏡頭 `empty-camera-s31.jpg` | `unknown` 0.999 | 否 |
| 純白合成圖 | `unknown` 1.000 | 否 |
| 數碼卡背 `shared-back.png` | `hazardous` 0.908 | **是** |
| 數碼封面 `shared-cover.png` | `hazardous` 0.626 | **是** |

封面／卡背仍然誤放：佢哋係卡形狀的印刷物，特徵上同真卡太近，線性 probe 分唔開。兩張一直只作負面檢查、冇入訓練，所以保持獨立；要修就係另一個資料決定。

## 重現

```bash
# 1. 四類資料集（上游 build 腳本，會重建 cards-padded 與 31,600 張場景）
.venv-train/bin/python tools/build_wordless_padded.py --force
# 2. 加 unknown 場景（train／val／holdout）
.venv-train/bin/python tools/render_none_scenes.py \
  --dataset models/wordless-padded/dataset --train 6200 --val 1093 --holdout 300 --size 160
# 3. 第五行（直接改 ONNX，唔需要 torch 權重）
.venv-train/bin/python tools/add_unknown_row.py \
  --model models/wordless-padded/model-float32.onnx \
  --data models/wordless-padded/dataset \
  --out models/wordless-unknown/model-float32.onnx \
  --holdout models/wordless-padded/dataset/holdout
# 4. 量化（1,500 張平衡測試集）
.venv-train/bin/python tools/quantize_espdl.py \
  --onnx models/wordless-unknown/model-float32.onnx \
  --calib models/wordless-padded/dataset/train --test runs/unknown-v1/test5 \
  --out models/wordless-unknown/wordless_s31.espdl --calib-images 240 --target esp32s31
# 5. 量度
.venv-train/bin/python tools/evaluate_cards.py --model models/wordless-unknown/model-float32.onnx \
  --dir models/wordless-padded/dataset/val --out val-scenes-float32.csv
.venv-train/bin/python tools/evaluate_none_scenes.py --model models/wordless-unknown/model-float32.onnx \
  --dir models/wordless-padded/dataset/holdout --out holdout-float32.csv
```

`models/wordless-padded/dataset/`、`cards-padded/` 同 `runs/` 都係生成物，已被 `.gitignore` 排除。

## 界線

- `unknown` 場景全部程序生成；唯一真實空鏡頭只有 1 張（S31 實拍），係獨立檢查，不可當統計結論。
- 卡面場景與訓練同源（同一批 158 張卡、同一批 seed），只描述這批合成卡的變體。
- int8 數字來自 ESP-PPQ 圖的 CPU 模擬，唔係板上執行；S31 未載入，延遲／RAM 未測。
- `stc_vision` 在 S31 仍未實作，`stc_label_t.abstain` 有 game 層支援但未有 producer。
- 逐圖檔案：`val-scenes-float32.csv`、`val-scenes-int8.csv`、`real-photos-float32.csv`、`holdout-float32.csv`、`negatives-float32.csv`、`old-model-empty-scenes.csv`。
