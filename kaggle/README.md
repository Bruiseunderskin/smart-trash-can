# Kaggle 訓練 kernel

讀者：要在 Kaggle GPU 上重跑加框無字卡訓練的人。何時讀：重跑或改動訓練設定時。

- `train_wordless_padded.py`：kernel 進入點。clone `feat/wordless-padded`，用 repo 內的 renderer 產生場景，再訓 YOLO11n-cls。
- `kernel-metadata.json`：kernel slug `wedsamuel1230/stc-wordless-padded-cards`，GPU、私人。

推送與讀取結果：

```bash
export KAGGLE_API_TOKEN=...            # 由 hermes 的環境檔讀取，勿寫入 repo
kaggle kernels push -p kaggle/
kaggle kernels status wedsamuel1230/stc-wordless-padded-cards
kaggle kernels logs wedsamuel1230/stc-wordless-padded-cards
```

輸出下載（`kaggle kernels output`）在此帳號常被 429 限流；kernel 因此額外用 base64 把 int8 模型串流到 log，再由 log 還原。
