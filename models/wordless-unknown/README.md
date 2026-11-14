# wordless-unknown：五類模型（加 `unknown` 出口）

讀者：模型與韌體整合人員。何時讀：取用五類模型、替換現行 `models/wordless-padded/wordless_s31.espdl`、或核對 `classes.json` v3.0.0 時。

## 內容

| 檔案 | 說明 |
|---|---|
| `model-float32.onnx` | 五類 float32，opset 18，輸入 `images [1,3,160,160]`，輸出 `output0 [1,5]` |
| `wordless_s31.espdl` | ESP32-S31 w8a8，1,796,240 bytes，檔頭 `EDL2`；**交付物** |
| `metrics.json` | 量化設定、1,500 張平衡測試集的 macro-F1 與逐類 F1 |
| `labels.txt` | `food, hazardous, other, recyclable, unknown`，與 `contracts/model/classes.json` v3.0.0 同序 |
| `SHA256SUMS` | 上述三個檔與 `labels.txt` 的 sha256 |

## 與前一代的分別

四類模型對「完全冇卡」的畫面會硬判成某一類：300 張程序化空場景中，中位信心 0.970、283 張仍過 0.55 門檻。空場景同真卡的分數貼埋一齊，任何門檻都分唔開。

第五類 `unknown` 排最後（`recyclable` < `unknown`），前四個 index 與桶位完全不變（`1,2,3,0`）；`unknown` 對應 `STC_BIN_COUNT`：不開桶、`stc_label_t.abstain = true`、由 `stc_game` 重試一次。

第五行係直接改 ONNX `Gemm`（4 行擴到 5 行），四個已知 row 同其他 initializer 原封不動；唔係重訓。詳見[實驗記錄](../../docs/experiments/mixed-card-object/unknown-v1/README.md)。

## 量度（host，非實機）

| 量度（5,831 張留起場景） | float32 | int8（ESP-PPQ 圖模擬） |
|---|---|---|
| 卡面場景正確 | 4,736/4,738 | 4,690/4,738 |
| 卡面場景被判 `unknown` | 0 | 40 |
| 空場景拒判 | 1,092/1,093 | 1,093/1,093 |
| 空場景 false-accept | 1 | 0 |

`.espdl` 在 1,500 張平衡測試集 macro-F1 0.9874（food 0.993、hazardous 0.998、other 0.985、recyclable 0.985、unknown 0.976）。前一代四類 `.espdl` 在同一批留起場景係 4,730/4,738；新模型係 4,690 raw，其中 40 張改判 `unknown`，接受後準確率 0.998。

## 未解決

- 數碼卡背（0.908）與數碼封面（0.626）仍判 `hazardous` 並過 0.55 門檻。它們是卡形狀的印刷物，特徵上與真卡太近。
- 只在 host 執行；S31 未載入、未量延遲、RAM、相機前處理一致性。
- 真實鏡頭整體 false-accept 未量，只有 1 張 S31 實拍空鏡頭（判 `unknown` 0.999）。
