# YOLO／ESP-DL 模型實作交接

讀者：模型與 S31 整合工程師。何時讀：載入、重播、評估或部署模型前。

## 已提供產物

使用者決定後續電腦視覺採 YOLO 與 `.espdl` 路線。**2026-11-11 起現行模型係**
[`models/wordless-padded/`](../../../models/wordless-padded/README.md)：YOLO11n-cls 160×160、S31 目標 w8a8 int8，
[模型檔](../../../models/wordless-padded/wordless_s31.espdl)、[標籤](../../../models/wordless-padded/labels.txt)、
[hash 記錄](../../../models/wordless-padded/SHA256SUMS)。同目錄亦係訓練資料唯一來源（38 張印刷白底卡）。

可追溯流程：Train (YOLO) → ONNX (opset 17) → `espdl_quantize_onnx()` → `.espdl`、`.json` → S31 部署。
**尚未在 S31 實機驗證。**

模型 SHA-256 見 `models/wordless-padded/SHA256SUMS`；使用前以 `shasum -a 256 models/wordless-padded/wordless_s31.espdl`
自行核對。標籤順序 food、hazardous、other、recyclable 對應桶位 1、2、3、0。輸入與前處理以
`contracts/model/preprocess.json` 和具體實測為準；舊 EI 六類模型數值不可套用至此模型。

舊 `models/mixed-card-object/`（`mixed_s31.espdl`、`wordless-v1`）已刪除，只留 git 歷史；
其歷史評估見[舊實驗記錄](../../experiments/mixed-card-object/README.md)。

## 五類模型（`unknown`，2026-11-14）

現行四類模型對「完全冇卡」的畫面會硬判成某一類（300 張程序化空場景中位信心 0.970、283 張仍過 0.55 門檻），已提供[五類產物](../../../models/wordless-unknown/README.md)：`labels.txt` 為 `food, hazardous, other, recyclable, unknown`，輸出 `[1,5]`，`unknown`（index 4）對應 `bin = STC_BIN_COUNT`、`abstain = true`。契約已升 `classes.json` **v3.0.0**、`preprocess.json` **1.1.0**，韌體 `STC_MODEL_CLASSES` 改 5。

仍然未在 S31 實機載入或量測；數碼卡背／封面仍誤判為 `hazardous` 並放行。整合前先核對 `labels.txt` 與 `contracts/model/classes.json` 同序，`test_classes_order` 會拒絕少過五行。

## 已有評估與限制

[實驗記錄](../../experiments/mixed-card-object/README.md) 指明此前 ESP-PPQ CPU 執行量化 graph 的 host 評估；`tools/quantize_espdl.py` 的 `TorchExecutor(graph=graph)` 使用量化過程返回的 graph。這不單憑證明該工具可直接載入現有 `.espdl` 重播。原始訓練來源並非全部隨 repo 提供；需查明可重現後端、版本及模型／輸入 parity，不能靜默用另一個模型替代。

此 `.espdl` 未經 S31 實機推論、端到端及延遲驗證。資料來源同域、未知物件拒判和素材權利仍有實驗記錄所述限制；不能據主機結果保證物件正確分類或安全開蓋。field-probe 不用於訓練、校準、閾值或選模，亦不應聲稱為完全獨立來源域。

## 實作者檢查

- [ ] 校驗 model、labels 和 preprocess hash，記錄工具鏈、commit 和來源。
- [ ] 識別實際 host backend 的輸入、模型和輸出；區分已保存 graph、重建 graph 與直接執行 `.espdl`。
- [ ] 取得真實合法來源的相機幀及獨立 golden 證據；記錄色彩、裁切、resize、量化與容差。
- [ ] 依現有契約核對四類映射、拒判及錯誤行為；更新前不把現有 `classes.json` 當作模型順序。
- [ ] S31 上量記憶體、推論與整機時間，保存原始 log；缺少實機即標未測。
