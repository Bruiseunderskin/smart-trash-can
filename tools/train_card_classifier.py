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
    parser.add_argument("--seeds", default="", help="comma separated seeds; each trains a model and the probabilities are averaged")
    parser.add_argument("--patience", type=int, default=4)
    parser.add_argument("--lr0", type=float, default=1e-4)
    parser.add_argument("--degrees", type=float, default=0.0)
    parser.add_argument("--translate", type=float, default=0.0)
    parser.add_argument("--scale", type=float, default=0.0)
    parser.add_argument("--erasing", type=float, default=0.0)
    args = parser.parse_args()

    from ultralytics import YOLO

    args.out.mkdir(parents=True, exist_ok=True)
    seeds = [int(s) for s in args.seeds.split(",")] if args.seeds else [args.seed]
    best_models: list[Path] = []
    for seed in seeds:
        model = YOLO(args.weights)
        train_kwargs = dict(
        data=str(args.data),
        epochs=args.epochs,
        imgsz=args.size,
        batch=args.batch,
        workers=2,
        project=str(args.out),
        name=f"train_s{seed}",
        exist_ok=True,
        seed=seed,
        pretrained=True,
        optimizer="AdamW",
        lr0=args.lr0,
        degrees=args.degrees,
        translate=args.translate,
        scale=args.scale,
        erasing=args.erasing,
        patience=args.patience,
        plots=False,
        verbose=True,
    )
        if args.device:
            train_kwargs["device"] = args.device

        model.train(**train_kwargs)
        run_dir = Path(model.trainer.save_dir)
        best_models.append(run_dir / "weights" / "best.pt")

    run_dir = best_models[0].parent.parent

    # ultralytics has no confusion matrix for classification, so build it from
    # predictions on the validation split
    val_root = args.data / "val"
    classes = sorted(p.name for p in val_root.iterdir() if p.is_dir())
    index = {name: i for i, name in enumerate(classes)}
    samples = [(p, index[d]) for d in classes for p in sorted((val_root / d).iterdir()) if p.is_file()]
    paths = [str(p) for p, _ in samples]

    # average the class probabilities of every seed's best checkpoint
    probs = np.zeros((len(samples), len(classes)), dtype=float)
    best_model = None
    for checkpoint in best_models:
        best_model = YOLO(str(checkpoint))
        names = best_model.names if isinstance(best_model.names, dict) else dict(enumerate(best_model.names))
        for row, result in enumerate(best_model.predict(paths, imgsz=args.size, verbose=False)):
            scores = result.probs.data.cpu().numpy()
            for class_index, class_name in names.items():
                probs[row, index[class_name]] += float(scores[class_index])
    probs /= max(1, len(best_models))

    cm = np.zeros((len(classes), len(classes)), dtype=int)
    for (path, true_index), row_probs in zip(samples, probs):
        cm[true_index, int(row_probs.argmax())] += 1
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
        "device": str(next(best_model.model.parameters()).device) if best_model else "unknown",
        "seeds": seeds,
        "ensemble_size": len(best_models),
        "val_accuracy": round(accuracy, 4),
        "val_macro_f1": round(macro_f1(cm), 4),
        "val_top1": round(accuracy, 4),
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
