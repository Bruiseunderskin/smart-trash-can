# 資料來源與授權

讀者：要重用、再分發或公開呢個資料集的人。何時讀：用之前。

規則：**來源或授權講唔清嘅圖片，一律唔入 dataset。** 呢個目錄只有下面兩類。

## 卡面

| 來源 | 內容 | 授權／權利 | 備註 |
|---|---|---|---|
| `docs/cards/300dpi/PNG` | Set A 9 張、Set B 29 張，共 38 張印刷卡 | 專案自有 | 由原本四頁 PDF 復原；B-11、B-14、B-15 經 AI 修整 |
| `cards/C-01`–`C-120` | 120 張輔助卡面（只教「未見過嘅設計」，唔會印） | 衍生自 Kaggle `sumn2u/garbage-classification-v2`（MIT），經 rembg 去背再貼白卡 | 原檔已入 git 歷史；逐張 `source` 見上一版 `cards.csv`（commit `6966149` 之前） |

## 真實相片（`real-photos/`）

| 來源 | 數量 | 授權 | 對應類別 |
|---|---:|---|---|
| `data/field-captures`（自己拍） | 33 | 專案自有 | other 18、recyclable 15 |
| [TrashNet](https://huggingface.co/datasets/garythung/trashnet) | 437（每類上限 300） | MIT（見 dataset card） | glass／paper／plastic／metal／cardboard → recyclable；trash → other |

逐張來源見 `real-photos/real-photos.csv`。冇 food 同 hazardous 嘅持牌真實相，所以呢兩類只有渲染圖。

## 明確唔用

| 資料 | 原因 |
|---|---|
| 舊 `images/*/physical`（396 張淘寶式商品圖） | 逐張來源同再分發權利未確認；有店舖水印 |
| `data/field-probe`（RealWaste CC-BY-4.0、viola77data Apache-2.0） | 授權清楚，但係獨立測試集，入訓練就毀掉佢嘅用途 |
| `data/garbage265`、`data/waste4_*` | 本機已經冇；舊 Edge Impulse 路線 |

## 再分發

卡片圖案同 TrashNet 相片都可以再分發（MIT／自有），但要保留出處。`cards/C-*` 係 derivative work，
公開時要一併標明 Kaggle 原資料集同 MIT 條款。

[模型與資料說明](README.md) · [專案入口](../../README.md)
