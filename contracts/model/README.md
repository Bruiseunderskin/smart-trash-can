# model contract v2.0.0

訓練與推論的唯一接口。任何一項不符，韌體會拒絕載入（`stc_vision_init()` 回 `STC_ERR_CONTRACT`）。

## 必交檔案

| 檔案 | 說明 |
|---|---|
| `model.onnx` | opset ≤ 17，固定輸入 shape，無動態維度 |
| `classes.json` | 類別索引 → 名稱／桶位（見下） |
| `preprocess.json` | 輸入尺寸、色彩、歸一化、resize 方式、contract_version |
| `eval.json` | confusion matrix、per-class precision/recall/F1、測試集大小與 hash |

## classes.json（已凍結，不要改順序）

```json
{"version":"2.0.0","class_order_frozen":true,"classes":[
 {"index":0,"key":"food","zh":"廚餘","bin":1},
 {"index":1,"key":"hazardous","zh":"有害垃圾","bin":2},
 {"index":2,"key":"other","zh":"其他垃圾","bin":3},
 {"index":3,"key":"recyclable","zh":"可回收物","bin":0}]}
```

v2.0.0（2026-11-08 決定 D2）：`index` 改跟模型 `labels.txt` 的次序，`key` 由 `hazard` 改為 `hazardous`（與模型一致），新增 `class_order_frozen`。桶位數值不變。韌體啟動時逐行比對 `labels.txt`，不符即 `STC_ERR_CONTRACT`，不會默認落 `other`。

## 預處理（推論必須逐步相同）

1. 相機輸出 YUV422 (UYVY) 640×480 → PPA 轉 RGB
2. **中心裁切為正方形**（640×480 → 中央 480×480），不用 letterbox
3. bilinear resize → `input_size`（預設 160×160）
4. RGB 通道順序（不是 BGR），uint8 0–255
5. 歸一化在**模型圖內**（除以 255、mean/std 寫進 ONNX），韌體不做額外縮放

> 訓練端如果做 augmentation，只可以作用在「步驟 2 之後的 RGB 圖」，並且評估時要用**未經 augmentation** 的同一條管線，否則 train/infer 不一致。

## 輸出

- `float32[1,4]` softmax（量化版為 int8/uint8 + scale/zero_point，由 `quant.json` 描述）
- 索引對應 `classes.json`
- 低於 `abstain_threshold`（預設 0.55）→ 標記為不確定，遊戲計 0 分並提示重試

## 評估門檻（寫入 eval.json）

| 指標 | 門檻 |
|---|---|
| macro-F1（獨立測試集） | ≥ 0.80 |
| 每類 recall | ≥ 0.70 |
| 裝置 p95 推理延遲 | ≤ 300 ms |
| host/target 類別一致率 | 100%（只可講「一致」，不可講「準確」） |
