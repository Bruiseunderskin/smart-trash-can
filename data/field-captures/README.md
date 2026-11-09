# 現場拍攝資料（field-captures）

> 狀態：歷史資料，保留原路徑及下列用途限制；未用於本次新模型訓練或校準。

自己用手機／相機拍的實物照片，用於補 domain gap（garbage265 網圖 → 真實課室環境）。

## 命名

`<bucket>/<object-id>_s<session>_<nn>_<view>.jpeg`

- `bucket`：`可回收物`／`廚餘`／`有害垃圾`／`其他垃圾`
- `object-id`：`類型_品牌或描述`，例如 `drink-carton_vitasoy-choc`
- `s<session>`：拍攝場次
- `view`：`front`／`back`／`side`／`top`／`bottom`／`held`／`bin`

## 用途限制

- **可以**入訓練集（需按 `contracts/model/preprocess.json` 前處理）
- **不可以**用來調 threshold 或選模型（會洩漏）
- 與 `data/field-probe/`（獨立評估集）分開：probe 集只可評估，不可訓練
