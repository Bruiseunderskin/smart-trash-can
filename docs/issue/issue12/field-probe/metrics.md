# Issue #12：17 張獨立 field-probe 評估

> **不可作為現場準確率估計。** 每類只有 2–8 張，而且來源不同；本結果只回答同一鎖定模型換資料來源後錯在哪裡。

- ONNX：`runs/issue12/cls160-grouped/weights/best.onnx`
- ONNX SHA-256：`0447a566cdbf6f45a43245205e5667f592d0f6d86bc1c7067bf7e736e6e8177f`
- Manifest SHA-256：`50fb8a988953d2d1b8fcba1346eec60728cf6667dc473962dd05fd49b7f8dba5`
- 固定 low-confidence threshold：`0.70`
- 中心裁切比例：`60%`（以短邊建立中心正方形）
- 紀律：本批圖永久排除於訓練、校準、閾值掃描及模型選擇；模型與 threshold 均未改動。

## Baseline：原始圖片

| Dataset | Images | Accuracy | Macro-F1 | Abstain | Coverage | Accepted accuracy | Mean confidence |
|---|---:|---:|---:|---:|---:|---:|---:|
| 全部 | 17 | 0.352941 | 0.342857 | 6 | 0.647059 | 0.454545 | 0.785569 |
| RealWaste | 14 | 0.357143 | 0.286813 | 6 | 0.571429 | 0.500000 | 0.765557 |
| 電池 | 3 | 0.333333 | 0.125000 | 0 | 1.000000 | 0.333333 | 0.878960 |

分佈不均而且電池只有一個真值類別，所以 Macro-F1 只用來保留完整數字，不應跨來源比較；正確性以逐張結果為準。

| file | bin_hk | prediction | confidence | correct | abstained | source | ambiguous |
|---|---|---|---:|---|---|---|---|
| recyclable/cardboard_7.jpg | 可回收物 | 可回收物 | 0.998892 | yes | no | RealWaste | no |
| recyclable/cardboard_50.jpg | 可回收物 | 其他 | 0.992484 | no | no | RealWaste | no |
| recyclable/glass_3.jpg | 可回收物 | 可回收物 | 0.982913 | yes | no | RealWaste | no |
| recyclable/glass_40.jpg | 可回收物 | 其他 | 0.985659 | no | no | RealWaste | no |
| recyclable/metal_12.jpg | 可回收物 | 有害 | 0.514367 | no | yes | RealWaste | no |
| recyclable/paper_5.jpg | 可回收物 | 有害 | 0.732188 | no | no | RealWaste | no |
| recyclable/paper_60.jpg | 可回收物 | 可回收物 | 0.499165 | yes | yes | RealWaste | no |
| recyclable/plastic_2.jpg | 可回收物 | 其他 | 0.672938 | no | yes | RealWaste | no |
| food/food-organics_77.jpg | 廚餘 | 有害 | 0.828324 | no | no | RealWaste | no |
| food/food-organics_9.jpg | 廚餘 | 可回收物 | 0.538227 | no | yes | RealWaste | no |
| food/vegetation_120.jpg | 廚餘 | 廚餘 | 0.944171 | yes | no | RealWaste | no |
| food/vegetation_4.jpg | 廚餘 | 其他 | 0.589153 | no | yes | RealWaste | yes |
| hazardous/viola-001_bd75a457.jpg | 有害垃圾 | 可回收物 | 0.877960 | no | no | 電池 | no |
| hazardous/viola-003_793ffd6d.jpg | 有害垃圾 | 可回收物 | 0.992851 | no | no | 電池 | no |
| hazardous/viola-005_f42fa779.jpg | 有害垃圾 | 有害 | 0.766069 | yes | no | 電池 | no |
| other/miscellaneous-trash_33.jpg | 其他垃圾 | 其他 | 0.999987 | yes | no | RealWaste | yes |
| other/miscellaneous-trash_6.jpg | 其他垃圾 | 可回收物 | 0.439332 | no | yes | RealWaste | no |

## 背景捷徑檢查

- 電池 3 張 accuracy：`0.333333`；coverage：`1.000000`；平均信心：`0.878960`。
- RealWaste 14 張 accuracy：`0.357143`；coverage：`0.571429`；平均信心：`0.765557`。
- 簡單捷徑訊號：`no`。
- 白底高信心偏差訊號：`yes`；電池高分張數：`3`；其中正確：`1`。
- 判讀：完整背景捷徑條件（電池全部高分正確、RealWaste 同時大幅崩塌）未成立；但電池 3 張全部高分並被接受，只有 1 張正確，而且 coverage 明顯高於 RealWaste。這顯示白底產品照可推高信心而沒有帶來準確率優勢；仍不能單憑這個對照證明背景係唯一原因。

## 中心 60% 裁切對照

| Dataset | Images | Accuracy | Macro-F1 | Abstain | Coverage | Accepted accuracy | Mean confidence |
|---|---:|---:|---:|---:|---:|---:|---:|
| 全部 | 17 | 0.470588 | 0.437302 | 4 | 0.764706 | 0.538462 | 0.825904 |
| RealWaste | 14 | 0.500000 | 0.378846 | 3 | 0.785714 | 0.545455 | 0.820115 |
| 電池 | 3 | 0.333333 | 0.125000 | 1 | 0.666667 | 0.500000 | 0.852918 |

| 對照項 | Baseline accuracy | 裁切 accuracy | Δ | Baseline coverage | 裁切 coverage | Δ |
|---|---:|---:|---:|---:|---:|---:|
| 全部 | 0.352941 | 0.470588 | +0.117647 | 0.647059 | 0.764706 | +0.117647 |
| RealWaste | 0.357143 | 0.500000 | +0.142857 | 0.571429 | 0.785714 | +0.214286 |
| 電池 | 0.333333 | 0.333333 | +0.000000 | 1.000000 | 0.666667 | -0.333333 |

| file | bin_hk | prediction | confidence | correct | abstained | source | ambiguous |
|---|---|---|---:|---|---|---|---|
| recyclable/cardboard_7.jpg | 可回收物 | 可回收物 | 0.829497 | yes | no | RealWaste | no |
| recyclable/cardboard_50.jpg | 可回收物 | 其他 | 0.994295 | no | no | RealWaste | no |
| recyclable/glass_3.jpg | 可回收物 | 可回收物 | 0.997272 | yes | no | RealWaste | no |
| recyclable/glass_40.jpg | 可回收物 | 其他 | 0.989928 | no | no | RealWaste | no |
| recyclable/metal_12.jpg | 可回收物 | 可回收物 | 0.652526 | yes | yes | RealWaste | no |
| recyclable/paper_5.jpg | 可回收物 | 有害 | 0.362330 | no | yes | RealWaste | no |
| recyclable/paper_60.jpg | 可回收物 | 可回收物 | 0.767893 | yes | no | RealWaste | no |
| recyclable/plastic_2.jpg | 可回收物 | 其他 | 0.960447 | no | no | RealWaste | no |
| food/food-organics_77.jpg | 廚餘 | 有害 | 0.810655 | no | no | RealWaste | no |
| food/food-organics_9.jpg | 廚餘 | 其他 | 0.586763 | no | yes | RealWaste | no |
| food/vegetation_120.jpg | 廚餘 | 廚餘 | 0.995694 | yes | no | RealWaste | no |
| food/vegetation_4.jpg | 廚餘 | 可回收物 | 0.756795 | no | no | RealWaste | yes |
| hazardous/viola-001_bd75a457.jpg | 有害垃圾 | 可回收物 | 0.989027 | no | no | 電池 | no |
| hazardous/viola-003_793ffd6d.jpg | 有害垃圾 | 其他 | 0.572121 | no | yes | 電池 | no |
| hazardous/viola-005_f42fa779.jpg | 有害垃圾 | 有害 | 0.997607 | yes | no | 電池 | no |
| other/miscellaneous-trash_33.jpg | 其他垃圾 | 其他 | 0.976742 | yes | no | RealWaste | yes |
| other/miscellaneous-trash_6.jpg | 其他垃圾 | 其他 | 0.800766 | yes | no | RealWaste | no |

判讀：中心裁切帶來部分 RealWaste 改善，但未達明顯改善門檻；物件位置／比例有貢獻，但不足以解釋主要錯誤，語意辨認仍是主要問題。

## 與 SC101IOT 4 張現場圖對照

| Dataset | Images | Accuracy | Macro-F1 | Coverage | Accepted accuracy |
|---|---:|---:|---:|---:|---:|
| SC101IOT live | 4 | 0.250000 | 0.250000 | 0.500000 | 0.000000 |
| Field probe baseline | 17 | 0.352941 | 0.342857 | 0.647059 | 0.454545 |
| Field probe center crop 60% | 17 | 0.470588 | 0.437302 | 0.764706 | 0.538462 |

兩組圖片數、類別分佈和拍攝來源都不同，只可比較同一模型換域後的方向和逐張錯誤，不可把差異當成穩定的現場準確率。

## 爭議標註

- `other/miscellaneous-trash_33.jpg`：乾果殼可爭議屬廚餘或其他；逐張表已標 `ambiguous=yes`，不以這張單獨判模型錯。
- `food/vegetation_4.jpg`：整條青瓜與廚餘殘渣分佈不同；逐張表已標 `ambiguous=yes`，不以這張單獨判模型錯。

嚴格版本仍照 manifest 原標籤計入 accuracy，沒有事後改真值；上述兩張只作人工爭議標記。
Baseline 嚴格 accuracy：`0.352941`；把爭議圖的錯判視為「不確定／不歸咎模型」後的靈敏度 accuracy：`0.411765`。
裁切後嚴格 accuracy：`0.470588`；爭議靈敏度 accuracy：`0.529412`。這兩個靈敏度數字只用於分析，不取代嚴格 manifest 指標。
