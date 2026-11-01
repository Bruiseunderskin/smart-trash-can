把訓練好的 int8 模型放這裡，檔名固定為 `waste4_int8.tflite`。

要求：
- 輸入 int8（full integer quantization）
- 輸入尺寸 160×160 或 224×224
- 檔案 < 3 MB

未有自己的模型時，可先用任一 ImageNet 級 int8 分類模型熱身（例如 MobileNetV2 int8），
量到的 ms 只作參考，不能當作自訓模型的結果。
