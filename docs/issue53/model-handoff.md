# YOLO／ESP-DL 模型實作交接

讀者：模型與 S31 整合工程師。何時讀：載入、重播、評估或部署模型前。

## 已提供產物

使用者決定後續電腦視覺採 YOLO 與 `.espdl` 路線。現有模型為 YOLO11n-cls 混合卡片與物件實驗、S31 目標 w8a8 int8：[模型檔](../../models/mixed-card-object/mixed_s31.espdl)、[標籤](../../models/mixed-card-object/labels.txt)、[hash 記錄](../../models/mixed-card-object/SHA256SUMS)。這是已提供的目標產物，不要求重訓或另交 `.tflite` 才能開始整合。

模型 SHA-256：`2edd8594b3ca4c3fbb1e4a471162f6768957e55fbac02482a596019ef3978557`；使用前以 `shasum -a 256 models/mixed-card-object/mixed_s31.espdl` 自行核對。標籤順序 food、hazardous、other、recyclable 對應桶位 1、2、3、0。輸入與前處理以 `contracts/model/preprocess.json` 和具體實測為準；舊 EI 六類模型數值不可套用至此模型。

## 已有評估與限制

[實驗記錄](../experiments/mixed-card-object/README.md) 指明此前 ESP-PPQ CPU 執行量化 graph 的 host 評估；`tools/quantize_espdl.py` 的 `TorchExecutor(graph=graph)` 使用量化過程返回的 graph。這不單憑證明該工具可直接載入現有 `.espdl` 重播。原始訓練來源並非全部隨 repo 提供；需查明可重現後端、版本及模型／輸入 parity，不能靜默用另一個模型替代。

此 `.espdl` 未經 S31 實機推論、端到端及延遲驗證。資料來源同域、未知物件拒判和素材權利仍有實驗記錄所述限制；不能據主機結果保證物件正確分類或安全開蓋。field-probe 不用於訓練、校準、閾值或選模，亦不應聲稱為完全獨立來源域。

## 實作者檢查

- [ ] 校驗 model、labels 和 preprocess hash，記錄工具鏈、commit 和來源。
- [ ] 識別實際 host backend 的輸入、模型和輸出；區分已保存 graph、重建 graph 與直接執行 `.espdl`。
- [ ] 取得真實合法來源的相機幀及獨立 golden 證據；記錄色彩、裁切、resize、量化與容差。
- [ ] 依現有契約核對四類映射、拒判及錯誤行為；更新前不把現有 `classes.json` 當作模型順序。
- [ ] S31 上量記憶體、推論與整機時間，保存原始 log；缺少實機即標未測。
