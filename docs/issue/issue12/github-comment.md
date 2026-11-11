## 驗證完成報告（本地證據）

已依此 issue 完成資料分組、重訓、獨立測試、int8 量化、S31 實板驗證，
以及 SC101IOT 現場圖片 smoke evaluation。以下證據目前保存在本地工作樹，
**尚未 commit、push 或建立 PR**，所以沒有可用的 GitHub artifact 連結。

### 本次做了什麼

1. 以 ModelScope `garbage265` revision
   `123b8a59c0af5c2ceedae010d9d7450bd6c1e016` 建立分組資料集。
   來源沒有拍攝場次 metadata，因此全部如實記錄為 `session_group=null`。
2. 使用 64-bit pHash Hamming distance ≤ 8 及 normalized 32×32 grayscale
   RMS distance ≤ 0.08 的雙門檻建立物件組，並將同組圖片固定在同一 split。
3. 以 YOLO11n-cls、160×160、20 epochs、CPU、seed 42 重訓四類模型。
   test 未參與訓練、早停、校準或閾值選擇。
4. 匯出 ONNX、完成 1,480 張 PyTorch/ONNX parity，再量化成
   `esp32s31` w8a8 ESP-DL 模型。
5. 在 ESP32-S31-Korvo-1 完成模型 self-test、12 圖 host/target parity、
   推論時間與記憶體量測。
6. 以 SC101IOT 經 COM5 串口取得現場 JPEG，完成四類各 1 張的獨立現場評估。

### 資料與模型結果

- 原始圖片：9,872 張，合併為 9,138 個物件組。
- 完全重複額外條目：145；pHash 近重複組：583；多圖物件組：672。
- 隔離標籤衝突：1 組 / 2 張。
- 最終資料：9,870 張 / 9,137 組；train/validation/test 為
  6,905 / 1,485 / 1,480；新 split 組洩漏為 0。
- Validation top-1：`0.862`。
- 獨立 test：accuracy `0.864189`、macro-F1 `0.864738`。
- Test low-confidence threshold 0.70：abstain 172、coverage `0.883784`、
  accepted accuracy `0.909786`。
- PyTorch/ONNX top-1 agreement：`1,480/1,480 = 1.0`。
- int8 ESP-DL host macro-F1：`0.833208`；大小 1,794,864 bytes。
- 量化模型相對 float 有明顯精度下降，不能視為等價替代。

### S31 實板結果

- ESP-IDF v6.1、ESP-DL 3.3.14、SC101IOT 1280×720 UYVY。
- Model self-test：PASS。
- 20 次純模型推論：mean `83,842 µs`、p50 `83,837 µs`、
  p95 `83,878 µs`。
- 模型載入約增加 internal RAM `41,552 B`、PSRAM `1,811,708 B`。
- 12 張對照圖的 QINT8 輸入 CRC：`12/12` 與 host 相同。
- S31 與 ESP-PPQ host：類別 `12/12` 一致，confidence mean absolute delta
  `2.67e-7`。

因此 target 的 preprocessing、量化與 runtime 數值路徑已對齊；問題不是
S31 執行路徑，而是量化模型本身的精度下降。

### SC101IOT 現場評估

四類各使用 1 張未參與訓練、校準或選模的實拍圖片：

| Truth | Prediction | Confidence | Handling |
|---|---|---:|---|
| 其他（紙巾） | 可回收物 | 0.815399 | 錯誤，被接受 |
| 可回收物（紙盒） | 其他 | 0.875398 | 錯誤，被接受 |
| 廚餘（橙子） | 廚餘 | 0.580522 | 正確，但 abstain |
| 有害（藥品包裝） | 其他 | 0.488199 | 錯誤，abstain |

現場 closed-set accuracy `0.25`、macro-F1 `0.25`、abstain `2`、
coverage `0.50`、accepted accuracy `0`。

這是每類 1 張的流程驗收，不足以估計整體效能，但已足以顯示目前模型
在室內、手持、近距離、真實相機視角下有嚴重 domain shift；而且 0.70
confidence threshold 無法攔住兩張高信心錯誤。

### 本地證據

- `docs/issue12/report.md`
- `docs/issue12/test-metrics.md`
- `docs/issue12/live-camera/metrics.md`
- `docs/issue12/s31-host-parity.json`
- `docs/issue12/pytorch-onnx-parity.json`
- `docs/issue12/quantized-test-metrics.json`
- `docs/issue12/live-camera/captures.json`

自動化測試：`24/24` 通過；`xiaozhi-esp32/` 內容 diff 為 0。

### 結論與建議

以 issue #12 要求的驗證流程而言，四項均已有結果與證據，可以視為驗證完成；
但**不能把目前的 4 圖結果當成產品精度通過，也不建議直接量產或整合為最終
分類器**。

建議另開 follow-up，依序處理：

1. 建立更大的 locked field set，建議至少每類 30 張、涵蓋至少 10 個獨立
   物件/場景；release decision 需要更大的獨立集合。
2. 現場集只用於評估，禁止回流訓練、校準或選模；同時記錄物件、背景、
   光照、距離與角度。
3. 在相機端加入固定 ROI/物件偵測與裁切，使輸入分布接近訓練資料，並驗證
   camera → ROI → classifier 的端到端時延。
4. 針對現場 domain shift 補充訓練資料並重新訓練，不能只靠調低 confidence
   threshold；目前高信心錯誤已證明單靠 threshold 不足。
5. 重新評估 quantization 設定或可接受精度門檻，因為 int8 macro-F1
   相對 float 明顯下降。
6. 在模型改善前，產品行為應採「低信心 abstain + 二次確認」；不能只依
   top-1 class 自動開蓋。
