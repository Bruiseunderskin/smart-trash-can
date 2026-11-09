# 加框無字卡面

讀者：模型訓練與印刷人員。何時讀：使用或重新產生 `cards-padded/` 時。

38 張 709×709 卡面，由 `tools/rebuild_wordless_cards.py` 從 `../cards/` 重製：

- 無任何文字，底部名稱橫幅已移除
- 圖案裁到 bbox 後放大至填滿內框（motif 1.0）
- 綠色圓角外框 9.5%、外圓角 13%；框內白邊 1.5%、內圓角 11.5%，圖案四角跟隨內框裁圓
- 外圍 2.5% 透明出血

重新產生：

```bash
python3 tools/rebuild_wordless_cards.py \
  data/mixed-card-object/wordless-v1/cards \
  data/mixed-card-object/wordless-v1/cards-padded --motif 1.0
```

卡面身分（class／train 或 val）記在 `../cards.csv`；訓練結果見 `../padded-training.md`。
