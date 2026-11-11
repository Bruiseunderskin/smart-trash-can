# Grouped-split test evaluation

- Images: 1480
- ONNX: `C:\smart_trash_can\runs\issue12\cls160-grouped\weights\best.onnx`
- ONNX SHA-256: `0447a566cdbf6f45a43245205e5667f592d0f6d86bc1c7067bf7e736e6e8177f`
- Input: [1, 3, 160, 160]
- Preprocessing: Ultralytics classify_transforms: resize, center crop, RGB tensor, ImageNet normalization
- Low-confidence threshold: 0.7

| Class | Precision | Recall | F1 | Support |
|---|---:|---:|---:|---:|
| 其他 | 0.819588 | 0.852547 | 0.835742 | 373 |
| 可回收物 | 0.798995 | 0.839050 | 0.818533 | 379 |
| 廚餘 | 0.951351 | 0.931217 | 0.941176 | 378 |
| 有害 | 0.898148 | 0.831429 | 0.863501 | 350 |

Closed-set accuracy: 0.864189
Closed-set macro-F1: 0.864738

Abstained: 172
Coverage: 0.883784
Accepted accuracy: 0.909786

Confusion matrix rows are truth and columns are predicted:

```json
[[318, 36, 6, 13], [35, 318, 8, 18], [12, 12, 352, 2], [23, 32, 4, 291]]
```
