# 真實相片

讀者：模型、資料與驗證人員。何時讀：檢查真實相片來源、類別分佈或訓練資料授權前。

呢個目錄有 470 張持牌真實相片，按模型類別分成：

- [`other/`](other/README.md)：155 張。
- [`recyclable/`](recyclable/README.md)：315 張。

逐張來源、授權及 SHA-256 記錄在 [`real-photos.csv`](real-photos.csv)。目前冇持牌 `food` 或 `hazardous` 真實相，呢兩類只使用渲染資料。相片只可用於已記錄嘅訓練用途；`data/field-probe` 係獨立測試集，唔可以混入。

[模型與資料說明](../README.md) · [資料來源與授權](../DATA-LICENSES.md)
