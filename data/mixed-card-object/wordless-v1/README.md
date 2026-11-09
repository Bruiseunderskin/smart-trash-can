# 無額外卡片文字的 mixed-card-object 版本

讀者：模型開發及資料審查人員。何時讀：重現或評估此版本前。這是現有 mixed-card-object 資料集的新增版本，不取代或宣稱補齊原資料。

`cards/` 含 Set A 九張與 Set B 29 張 300 DPI 白底無卡片名稱 PNG；保留物件包裝本身文字，B-11、B-14、B-15 為使用者接納的 AI 修整圖。`cards.csv` 記錄分類、身份、split 和 SHA-256。模型四類：`recyclable → 桶 0`、`food → 桶 1`、`hazardous → 桶 2`、`other → 桶 3`。

`print-300dpi/` 是同一版卡面的 A4 試印稿（兩套正面與共用卡背，卡面 60 × 60 mm），卡面與 `cards/` 逐張 MD5 相同，見其 README。

`simulated-cards.zip` 含訓練用 2,400 張合成模擬卡片圖（30 張卡每張 80 變體）及 8 張數碼卡驗證圖。圖片不添加卡片名稱；物件包裝的原有字樣保留。`manifest.csv` 記錄每張輸出 hash、來源卡及種子；`physical-train.csv` 指回混合資料集中實際使用的 100 張實物訓練圖，不重複保存。資料來源與可再分發權利限制仍見上一層 README；原來 1,117 張實物訓練圖仍缺失。

本次訓練將 38 張卡按卡面身份分為 30 張訓練、8 張驗證，再只對訓練卡做合成增強。但初始混合模型早已見過舊版卡片身份；8 張數碼卡並非從模型視角完全未見。模擬卡圖片上的測量只描述合成情境，不代表實體卡或現場表現。歷史 `field-probe` 和 `field-captures` 均未用於本次訓練、校準或模型選擇。

## 生成

在 repo root 使用含 Pillow 的 Python：

```sh
python tools/build_wordless_card_dataset.py --cards data/mixed-card-object/wordless-v1 --out /tmp/stc-wordless-dataset --count 80 --seed 20261109 --physical-data data/mixed-card-object --physical-repeats 5
```

輸出必須是不存在的新目錄。此命令以 `inventory.csv` 中現存的 100 張實物訓練圖各使用五次，並產生 `physical-train.csv`；不可用驗證集或 field-probe 作校準。照片本身授權及個人資料使用限制須先核對。
