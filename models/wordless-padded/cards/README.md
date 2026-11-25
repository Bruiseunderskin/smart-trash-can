# 卡面來源

讀者：模型、韌體與卡片設計人員。何時讀：新增或替換訓練卡面、追查卡面來源，或準備重建資料集前。

呢個目錄有 158 張 709×709 PNG 卡面，係 `wordless-padded` 模型嘅唯一卡面來源：

- Set A／B 共 38 張印刷卡，對應 `docs/cards/300dpi`，兩邊逐張 byte-identical。
- Set C 共 120 張輔助卡面，只用作訓練未見卡設計，**唔會印刷**。
- 卡身份、類別、fold 及 SHA-256 記錄在 [`../cards.csv`](../cards.csv)。
- 來源與再分發條款見 [`../DATA-LICENSES.md`](../DATA-LICENSES.md)。

唔好直接改名、重新壓縮或覆蓋卡面；`cards.csv`、`SHA256SUMS`、renderer 與已訓練模型都依賴目前檔案。要加卡或換卡，請同時更新 manifest、授權記錄及重建／重訓流程。

[模型與資料說明](../README.md)
