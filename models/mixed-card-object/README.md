# 混合模型產物

讀者：S31 整合及模型審核人員。何時讀：載入或審查模型前。

`mixed_s31.espdl` 是 ESP32-S31 目標的 w8a8 int8 實驗模型；`labels.txt` 為輸出次序，`SHA256SUMS` 保留校驗值。這是現行 YOLO／ESP-DL 路線的目標產物，模型未在 S31 實機驗證。未知輸入拒絕和權利問題尚未解決，不可作現場交付。

[訓練資料、主機結果及限制](../../docs/experiments/mixed-card-object/README.md)

- [無額外卡片文字的 S31 int8 候選模型](wordless-v1/README.md)：另存的開發候選，未取代現行模型。
