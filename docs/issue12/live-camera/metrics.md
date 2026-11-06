# SC101IOT live-camera evaluation

- Images: 4
- ONNX: `C:\smart_trash_can\runs\issue12\cls160-grouped\weights\best.onnx`
- ONNX SHA-256: `0447a566cdbf6f45a43245205e5667f592d0f6d86bc1c7067bf7e736e6e8177f`
- Manifest SHA-256: `9ca04fc373d016b173ca1655f0bdfecb592af781c1a452a698b0e5365308c362`
- Low-confidence threshold: 0.7
- Provenance: live camera; excluded from training, calibration, and model selection

| Class | Precision | Recall | F1 | Support |
|---|---:|---:|---:|---:|
| 其他 | 0.000000 | 0.000000 | 0.000000 | 1 |
| 可回收物 | 0.000000 | 0.000000 | 0.000000 | 1 |
| 廚餘 | 1.000000 | 1.000000 | 1.000000 | 1 |
| 有害 | 0.000000 | 0.000000 | 0.000000 | 1 |

Closed-set accuracy: 0.250000
Closed-set macro-F1: 0.250000
Abstained: 2
Coverage: 0.500000
Accepted accuracy: 0.000000

Confusion matrix rows are truth and columns are predicted:

```json
[[0, 1, 0, 0], [1, 0, 0, 0], [0, 0, 1, 0], [1, 0, 0, 0]]
```
