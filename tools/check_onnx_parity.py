#!/usr/bin/env python3
"""Compare YOLO11 classification PyTorch and ONNX top-1 predictions."""

from __future__ import annotations

import argparse
import json
import random
import sys
from pathlib import Path
from typing import Any

from ultralytics import YOLO


IMAGE_SUFFIXES = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pt", type=Path, required=True, help="PyTorch best.pt checkpoint")
    parser.add_argument("--onnx", type=Path, required=True, help="Exported ONNX model")
    parser.add_argument("--data", type=Path, required=True, help="ImageFolder test split")
    parser.add_argument("--n", type=int, default=50, help="Stratified sample size; use 0 to evaluate the full split")
    parser.add_argument("--imgsz", type=int, default=160)
    parser.add_argument("--batch", type=int, default=16)
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--report", type=Path, help="Optional JSON report path")
    args = parser.parse_args()
    if args.n < 0 or args.batch < 1:
        parser.error("--n must be >= 0 and --batch must be >= 1")
    return args


def gather_images(root: Path) -> tuple[list[str], list[int], list[str]]:
    classes = sorted(path for path in root.iterdir() if path.is_dir())
    if len(classes) < 2:
        raise ValueError(f"expected class subdirectories under {root}")
    paths: list[str] = []
    labels: list[int] = []
    class_names = [path.name for path in classes]
    for label, class_dir in enumerate(classes):
        files = sorted(path for path in class_dir.rglob("*") if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES)
        paths.extend(str(path) for path in files)
        labels.extend([label] * len(files))
    return paths, labels, class_names


def select_examples(paths: list[str], labels: list[int], class_names: list[str], count: int, seed: int) -> tuple[list[str], list[int]]:
    if count == 0 or count >= len(paths):
        return paths, labels
    rng = random.Random(seed)
    by_class = {name: [] for name in range(len(class_names))}
    for index, label in enumerate(labels):
        by_class[label].append(index)
    base, remainder = divmod(count, len(class_names))
    chosen: list[int] = []
    chosen_set: set[int] = set()
    for label in range(len(class_names)):
        indexes = by_class[label]
        quota = base + (1 if label < remainder else 0)
        take = min(quota, len(indexes))
        picked = rng.sample(indexes, take)
        chosen.extend(picked)
        chosen_set.update(picked)
    if len(chosen) < count:
        remaining = [index for index in range(len(paths)) if index not in chosen_set]
        chosen.extend(rng.sample(remaining, count - len(chosen)))
    rng.shuffle(chosen)
    return [paths[i] for i in chosen], [labels[i] for i in chosen]


def top1(results: list[Any]) -> list[int]:
    predictions = []
    for result in results:
        if result.probs is None:
            raise RuntimeError(f"model returned no classification probabilities for {result.path}")
        predictions.append(int(result.probs.top1))
    return predictions


def predict(model: YOLO, paths: list[str], imgsz: int, batch: int, device: str) -> list[int]:
    predictions: list[int] = []
    for start in range(0, len(paths), batch):
        batch_paths = paths[start : start + batch]
        results = model.predict(
            source=batch_paths,
            imgsz=imgsz,
            batch=len(batch_paths),
            device=device,
            verbose=False,
            save=False,
        )
        predictions.extend(top1(results))
    return predictions


def f1_report(truth: list[int], prediction: list[int], class_names: list[str]) -> dict[str, Any]:
    confusion = [[0 for _ in class_names] for _ in class_names]
    for actual, predicted in zip(truth, prediction, strict=True):
        confusion[actual][predicted] += 1
    per_class = {}
    scores = []
    for index, name in enumerate(class_names):
        tp = confusion[index][index]
        fp = sum(confusion[row][index] for row in range(len(class_names))) - tp
        fn = sum(confusion[index]) - tp
        denominator = 2 * tp + fp + fn
        score = (2 * tp / denominator) if denominator else 0.0
        per_class[name] = score
        scores.append(score)
    return {
        "macro_f1": sum(scores) / len(scores),
        "per_class_f1": per_class,
        "confusion_matrix": confusion,
    }


def main() -> int:
    args = parse_args()
    for path in (args.pt, args.onnx, args.data):
        if not path.exists():
            raise FileNotFoundError(path)
    paths, labels, class_names = gather_images(args.data)
    paths, labels = select_examples(paths, labels, class_names, args.n, args.seed)
    if not paths:
        raise ValueError(f"no supported images found in {args.data}")

    pt_model = YOLO(str(args.pt), task="classify")
    onnx_model = YOLO(str(args.onnx), task="classify")
    pt_predictions = predict(pt_model, paths, args.imgsz, args.batch, args.device)
    onnx_predictions = predict(onnx_model, paths, args.imgsz, args.batch, args.device)
    if len(pt_predictions) != len(paths) or len(onnx_predictions) != len(paths):
        raise RuntimeError("one or both model backends returned a different number of predictions than inputs")

    mismatches = [
        {
            "image": paths[index],
            "label": class_names[labels[index]],
            "pt": class_names[pt_predictions[index]],
            "onnx": class_names[onnx_predictions[index]],
        }
        for index in range(len(paths))
        if pt_predictions[index] != onnx_predictions[index]
    ]
    report = {
        "images": len(paths),
        "seed": args.seed,
        "top1_agreement": (len(paths) - len(mismatches)) / len(paths),
        "pt": f1_report(labels, pt_predictions, class_names),
        "onnx": f1_report(labels, onnx_predictions, class_names),
        "mismatches": mismatches,
    }
    output = json.dumps(report, ensure_ascii=False, indent=2)
    print(output)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(output + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
