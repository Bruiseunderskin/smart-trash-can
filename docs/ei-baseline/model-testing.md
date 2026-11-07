# Edge Impulse baseline — Waste-Rec (issue #7)

- Project: Waste-Rec, ID `1154067`, account `wedsamuel1230`, target ESP32-S3
- Impulse: Image 160×160 (fit-short, lanczos3, RGB) → Image DSP → Transfer Learning (Images)
- Backbone: MobileNetV2 160×160 0.5; 40 training cycles; lr 0.0005; validation 20%; augmentation on; int8 profiling on
- Dataset: 1,198 training / 788 test; classes food, hazardous, other, recyclable
- Test split uploaded by the user; it was never used for training, calibration, or model selection. It was classified once.

## Validation (int8 model, 20% hold-out from training set)

- Accuracy 83.8%, loss 1.10
- AUC 0.97; weighted precision/recall/F1 0.84
- Per-class F1: food 0.90, hazardous 0.84, other 0.83, recyclable 0.78

## Model testing (788-image test split)

The Studio "Model testing" run used the **Unoptimized (float32)** model version.

- Accuracy **78.17%**
- AUC 0.96
- Weighted precision 0.81, recall 0.81, F1 0.81
- Macro F1 (mean of per-class F1): 0.800

Confusion matrix (row = true label, %, columns include EI's `uncertain` rejection):

| true \ predicted | food | hazardous | other | recyclable | uncertain |
|---|---:|---:|---:|---:|---:|
| food | 94.9 | 0.0 | 1.5 | 1.5 | 2.0 |
| hazardous | 1.5 | 82.6 | 5.6 | 5.1 | 5.1 |
| other | 3.1 | 15.3 | 65.3 | 9.2 | 7.1 |
| recyclable | 6.0 | 6.5 | 8.0 | 69.8 | 9.5 |

Per-class F1: food 0.92, hazardous 0.81, other 0.72, recyclable 0.75.

Machine-readable copy: `confusion-matrix.csv`.

## Notes

- The deployable int8 model was not re-scored on the test split, to keep the single-look rule for the test set. Validation accuracy above refers to the int8 model; model-testing accuracy refers to float32.
- EI's confusion matrix includes an `uncertain` column (rejection), so rows sum to 100% across five columns.
