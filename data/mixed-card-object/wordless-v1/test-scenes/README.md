# 測試圖集（40 張）

讀者：整合與韌體人員。何時讀：接模型、寫重播路徑或做 smoke test 之前。

這 40 張圖用來驗證「模型能不能載入、前處理是否正確、標籤次序有沒有調亂」，不可以用來評估泛化能力。

## 內容

- `*.jpg`：40 張 160×160 場景圖，每類 10 張
- `labels.csv`：`file,class,card_id,expected_index`
- `onnx-check.json`：用 `models/wordless-padded/model-int8.onnx` 跑出的結果

## 標籤次序

`expected_index` 跟模型輸出次序：`food = 0`、`hazardous = 1`、`other = 2`、`recyclable = 3`。與 `contracts/model/classes.json` 的桶位映射是另一層，見 issue #53 §6.1。

## 這批圖從哪來

由 `tools/render_wordless_scenes.py` 以 `--fold 2`、`--seed 20261111` 渲染，卡面取自 `../cards-padded/`。fold 2 的卡在交叉驗證中是「未見卡」，但**最終出貨模型已用全部 158 張卡訓練**，所以這批圖對它不是未見。

## 目前的檢查結果

`onnx-check.json`：40/40 正確，平均信心 0.986，混淆矩陣全對角。

此結果只證明 int8 匯出、前處理（RGB、resize 160、除以 255、NCHW）與標籤次序一致。它不是準確率指標。

## 重新產生

```bash
python3 tools/render_wordless_scenes.py \
  --cards data/mixed-card-object/wordless-v1/cards-padded \
  --manifest data/mixed-card-object/wordless-v1/cards.csv \
  --out /tmp/testsplit --fold 2 --balance-val-per-class 80 \
  --train-per-card 1 --size 160 --seed 20261111
```

再把 `/tmp/testsplit/val/<class>/` 每類取前 10 張複製到本目錄，並重寫 `labels.csv`。
