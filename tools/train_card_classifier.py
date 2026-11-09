#!/usr/bin/env python3
"""Fine-tune YOLO11n-cls on the rendered wordless card scenes.

Matches the project's chosen model family: the wordless candidate is YOLO11n-cls
throughout, so the padded cards are trained the same way rather than with a
separate MobileNet pipeline.

Validation is split by card identity: the reported numbers describe unseen cards
from the same 38 artworks, not real-object or field accuracy.

    python3 tools/train_card_classifier.py --data /tmp/dataset --out runs/card-cls
"""

from __future__ import annotations

import argparse
import csv
import json
import shutil
from pathlib import Path

import numpy as np


def macro_f1(cm: np.ndarray) -> float:
    scores = []
    for c in range(cm.shape[0]):
        tp = cm[c, c]
        fp = cm[:, c].sum() - tp
        fn = cm[c, :].sum() - tp
        precision = tp / (tp + fp) if tp + fp else 0.0
        recall = tp / (tp + fn) if tp + fn else 0.0
        scores.append(2 * precision * recall / (precision + recall) if precision + recall else 0.0)
    return float(np.mean(scores))


def read_history(run_dir: Path) -> list[dict]:
    path = run_dir / "results.csv"
    if not path.exists():
        return []
    rows = []
    with path.open() as handle:
        for row in csv.DictReader(handle):
            rows.append({k.strip(): v.strip() for k, v in row.items()})
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--weights", default="yolo11n-cls.pt")
    parser.add_argument("--epochs", type=int, default=12)
    parser.add_argument("--batch", type=int, default=64)
    parser.add_argument("--size", type=int, default=160)
    parser.add_argument("--device", default=None, help="cuda / cpu / mps; default auto")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--patience", type=int, default=4)
    args = parser.parse_args()

    from ultralytics import YOLO

    args.out.mkdir(parents=True, exist_ok=True)
    model = YOLO(args.weights)
    train_kwargs = dict(
        data=str(args.data),
        epochs=args.epochs,
        imgsz=args.size,
        batch=args.batch,
        workers=2,
        project=str(args.out),
        name="train",
        exist_ok=True,
        seed=args.seed,
        pretrained=True,
        optimizer="AdamW",
        lr0=1e-4,
        patience=args.patience,
        plots=False,
        verbose=True,
    )
    if args.device:
        train_kwargs["device"] = args.device

    model.train(**train_kwargs)
    run_dir = Path(model.trainer.save_dir)

    metrics = model.val(data=str(args.data), split="val", imgsz=args.size, plots=False, verbose=False)
    cm = np.array(metrics.confusion_matrix.matrix, dtype=float)
    classes = list(metrics.names.values()) if isinstance(metrics.names, dict) else list(metrics.names)
    accuracy = float(cm.trace() / cm.sum()) if cm.sum() else 0.0

    best = run_dir / "weights" / "best.pt"
    shutil.copy2(best, args.out / "model.pt") if best.exists() else None

    onnx_path = None
    try:
        exported = YOLO(str(best)).export(format="onnx", imgsz=args.size, opset=17)
        onnx_path = Path(exported)
        shutil.copy2(onnx_path, args.out / "model-float32.onnx")
        onnx_path = args.out / "model-float32.onnx"
    except Exception as exc:
        print(f"onnx export skipped: {exc}")

    int8_path = None
    try:
        exported_int8 = YOLO(str(best)).export(format="onnx", imgsz=args.size, int8=True, data=str(args.data))
        int8_path = Path(exported_int8)
        shutil.copy2(int8_path, args.out / "model-int8.onnx")
        int8_path = args.out / "model-int8.onnx"
    except Exception as exc:
        print(f"int8 export skipped: {exc}")

    report = {
        "model": "yolo11n-cls",
        "weights": args.weights,
        "input": f"{args.size}x{args.size} RGB",
        "classes": classes,
        "epochs": args.epochs,
        "device": str(metrics.speed) if hasattr(metrics, "speed") else "unknown",
        "val_accuracy": round(accuracy, 4),
        "val_macro_f1": round(macro_f1(cm), 4),
        "val_top1": round(float(metrics.top1), 4) if hasattr(metrics, "top1") else None,
        "confusion_matrix": cm.astype(int).tolist(),
        "history": read_history(run_dir),
        "artifacts": {
            "best_pt": "model.pt",
            "onnx_float32": onnx_path.name if onnx_path else None,
            "onnx_int8": int8_path.name if int8_path else None,
        },
        "caveat": (
            "Validation is split by card identity; all classes come from the same 38 wordless "
            "artworks. This measures recognition of unseen cards from this set, not real-object "
            "or field accuracy."
        ),
    }
    (args.out / "metrics.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: report[k] for k in ("val_accuracy", "val_macro_f1", "val_top1")}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
