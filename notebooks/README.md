# Notebooks

讀者：要跑評估或重現實驗的人。何時讀：打算用 Kaggle 或 Colab 跑，而不是本機時。

這裡的 notebook 只做「編排」：把 repo 內已有的 CLI 串起來跑，不放模型邏輯。數字的定義一律以 `tools/` 與 `docs/experiments/` 為準。

| Notebook | 用途 | 需要 |
|---|---|---|
| [kaggle-setb-matrix.ipynb](kaggle-setb-matrix.ipynb) | issue #67 的 S1 至 S3：出矩陣、兩個 ONNX 模型推論、掃 `tau` 與拒判門檻、出 `summary.md` | CPU、Internet、numpy／Pillow／onnxruntime |

## 使用前

1. 在 Kaggle 建立 notebook，**Settings 開 Internet**，Accelerator 選 None（CPU 足夠）。
2. 直接執行第一個 cell（clone public repo ＋ `pip install`），再執行其餘 cell。
3. 輸出只保留 `summary.md`、`rows-*.csv`、`sweep-*.json`；矩陣圖片約 830 MB，不要存成 output。

## 界線

- `model-int8.onnx` 是 ESP-PPQ 量化的 ONNX，用 onnxruntime 跑只是 host 代理；S31 上的 `.espdl` 表現仍要上板驗。
- notebook 產生的數字全部屬模擬，不可當實機準確率對外發布。
