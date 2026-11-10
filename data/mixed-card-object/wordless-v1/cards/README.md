# 無額外卡片文字的卡面

讀者：模型訓練者。何時讀：使用卡面圖片前。

來源、分類、split、hash 及限制見 [資料版本說明](../README.md) 和 `../cards.csv`；B-11、B-14、B-15 經 AI 修整。

## 兩批卡面

| 批次 | 編號 | 數量 | 來源 |
|---|---|---:|---|
| 原始 | A-01 至 A-09、B-01 至 B-29 | 38 | 使用者提供的卡面 |
| 生成 | C-01 至 C-120 | 120 | Kaggle `sumn2u/garbage-classification-v2`（MIT） |

生成批次由 `tools/build_cards_from_photos.py` 產生：rembg（u2netp）去背、加白邊貼紙輪廓、置中於 709×709 白卡，最後加淡灰圓角邊線。類別對應如下：

- biological → food
- battery → hazardous
- cardboard、glass、metal、paper、plastic → recyclable
- clothes、shoes、trash → other

`cards.csv` 的 `source` 欄記錄每張生成卡的原圖路徑，`fold` 欄是 0 至 4 的卡片身分交叉驗證分組。

重新產生生成批次：

```bash
python3 tools/build_cards_from_photos.py \
  --photos /path/to/standardized_384 \
  --out /tmp/newfaces --per-class 30 --start 1 > /tmp/newfaces.csv
python3 tools/merge_generated_cards.py --csv ../cards.csv --new /tmp/newfaces.csv \
  --cards-dir . --new-cards-dir /tmp/newfaces --folds 5
```
