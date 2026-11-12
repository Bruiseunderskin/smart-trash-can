# data

原始資料來源。**訓練集唔再喺呢度**：最終唯一可信嘅資料集係
[`models/wordless-padded/`](../models/wordless-padded/README.md)（158 張卡面 → 珠光紙 sprite → 場景）。

## 保留嘅原始資料

| 目錄 | 用途 | 可否入訓練 |
|---|---|---|
| [field-captures](field-captures/README.md) | 自己拍嘅實物相（33 張，其他垃圾／可回收物） | 可以；**專案自有**，已用於場景背景同小量真實相 |
| [field-probe](field-probe/README.md) | 獨立測試集（RealWaste、viola77data） | **不可以**，授權清楚但係獨立評估集 |
| `modelscope_cache/` | 舊 Hermes 訓練輸出同快取（未入 git） | 不適用 |

授權規則：**來源或授權唔清楚嘅相片一律唔用。** 逐項授權見
[`models/wordless-padded/DATA-LICENSES.md`](../models/wordless-padded/DATA-LICENSES.md)。

## 已移除

`data/mixed-card-object/` 已刪除（2026-11-11）。當中有三批唔同嘅資料：

- `images/*/simulated_card/`：舊 38 張卡面嘅細卡場景（卡只佔畫面約三成），分佈同部署唔同
- `images/*/physical/`：396 張淘寶式商品圖，帶水印，來源同授權未確認 → **永久停用**
- `wordless-v1/cards/`：38 張印刷卡 ＋ 120 張 Set C 輔助卡面。卡面已搬去
  [`models/wordless-padded/cards/`](../models/wordless-padded/cards)，其餘清單停用

需要舊版本時由 git 取回：`git show <commit>:data/mixed-card-object/...`。
現行資料規則見 [models/wordless-padded](../models/wordless-padded/README.md)。

[上層指南](../README.md) · [專案入口](../README.md)
