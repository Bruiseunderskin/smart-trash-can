#!/usr/bin/env python3
"""Evaluate classification probabilities with closed-set and abstention metrics."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

import numpy as np
from PIL import Image


IMAGE_SUFFIXES = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}


def _per_class_metrics(
    confusion: list[list[int]],
    class_names: list[str],
    *,
    supports: list[int] | None = None,
) -> dict[str, dict[str, float | int]]:
    size = len(class_names)
    if supports is None:
        supports = [sum(row) for row in confusion]
    metrics: dict[str, dict[str, float | int]] = {}
    for index, name in enumerate(class_names):
        true_positive = confusion[index][index]
        false_positive = sum(confusion[row][index] for row in range(size)) - true_positive
        false_negative = supports[index] - true_positive
        precision_denominator = true_positive + false_positive
        recall_denominator = true_positive + false_negative
        precision = true_positive / precision_denominator if precision_denominator else 0.0
        recall = true_positive / recall_denominator if recall_denominator else 0.0
        f1 = 2.0 * precision * recall / (precision + recall) if precision + recall else 0.0
        metrics[name] = {
            "precision": precision,
            "recall": recall,
            "f1": f1,
            "support": supports[index],
            "true_positive": true_positive,
            "false_positive": false_positive,
            "false_negative": false_negative,
        }
    return metrics


def classification_report(
    truth: list[int],
    probabilities: list[list[float]],
    class_names: list[str],
    *,
    low_confidence_threshold: float,
) -> dict[str, Any]:
    if not truth or len(truth) != len(probabilities):
        raise ValueError("truth and probabilities must contain the same non-zero number of samples")
    if not class_names or not 0.0 <= low_confidence_threshold <= 1.0:
        raise ValueError("class names are required and threshold must be in [0, 1]")
    class_count = len(class_names)
    if any(len(row) != class_count for row in probabilities):
        raise ValueError("every probability row must match class_names")
    if any(index < 0 or index >= class_count for index in truth):
        raise ValueError("truth contains an invalid class index")

    predictions = [
        max(range(class_count), key=lambda index: row[index])
        for row in probabilities
    ]
    confusion = [[0 for _ in class_names] for _ in class_names]
    for actual, predicted in zip(truth, predictions, strict=True):
        confusion[actual][predicted] += 1
    per_class = _per_class_metrics(confusion, class_names)
    correct = sum(confusion[index][index] for index in range(class_count))
    macro_f1 = sum(float(item["f1"]) for item in per_class.values()) / class_count

    accepted_confusion = [[0 for _ in class_names] for _ in class_names]
    abstained_by_true_class = {name: 0 for name in class_names}
    accepted_count = 0
    accepted_correct = 0
    for actual, predicted, row in zip(truth, predictions, probabilities, strict=True):
        confidence = float(row[predicted])
        if confidence < low_confidence_threshold:
            abstained_by_true_class[class_names[actual]] += 1
            continue
        accepted_confusion[actual][predicted] += 1
        accepted_count += 1
        accepted_correct += int(actual == predicted)

    low_confidence_metrics = _per_class_metrics(
        accepted_confusion,
        class_names,
        supports=[sum(row) for row in confusion],
    )
    return {
        "images": len(truth),
        "class_names": class_names,
        "closed_set": {
            "accuracy": correct / len(truth),
            "macro_f1": macro_f1,
            "per_class": per_class,
            "confusion_matrix": confusion,
        },
        "low_confidence": {
            "threshold": low_confidence_threshold,
            "abstained_count": len(truth) - accepted_count,
            "coverage": accepted_count / len(truth),
            "accepted_accuracy": accepted_correct / accepted_count if accepted_count else 0.0,
            "abstained_by_true_class": abstained_by_true_class,
            "per_class": low_confidence_metrics,
            "accepted_confusion_matrix": accepted_confusion,
        },
    }


def collect_imagefolder(root: Path) -> tuple[list[Path], list[int], list[str]]:
    classes = sorted(path for path in root.iterdir() if path.is_dir())
    if not classes:
        raise ValueError(f"expected class subdirectories under {root}")
    paths = []
    labels = []
    class_names = [path.name for path in classes]
    for label, folder in enumerate(classes):
        images = sorted(
            path
            for path in folder.rglob("*")
            if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES
        )
        paths.extend(images)
        labels.extend([label] * len(images))
    if not paths:
        raise ValueError(f"no supported images found under {root}")
    return paths, labels, class_names


def _softmax(values: np.ndarray) -> np.ndarray:
    shifted = values - values.max(axis=1, keepdims=True)
    exponentials = np.exp(shifted)
    return exponentials / exponentials.sum(axis=1, keepdims=True)


def _markdown_report(report: dict[str, Any]) -> str:
    lines = [
        "# Grouped-split test evaluation",
        "",
        f"- Images: {report['images']}",
        f"- ONNX: `{report['onnx_path']}`",
        f"- ONNX SHA-256: `{report['onnx_sha256']}`",
        f"- Input: {report['input_shape']}",
        f"- Preprocessing: {report['preprocessing']}",
        f"- Low-confidence threshold: {report['low_confidence']['threshold']}",
        "",
        "| Class | Precision | Recall | F1 | Support |",
        "|---|---:|---:|---:|---:|",
    ]
    for name in report["class_names"]:
        metrics = report["closed_set"]["per_class"][name]
        lines.append(
            f"| {name} | {metrics['precision']:.6f} | {metrics['recall']:.6f} | "
            f"{metrics['f1']:.6f} | {metrics['support']} |"
        )
    closed = report["closed_set"]
    low = report["low_confidence"]
    lines.extend(
        (
            "",
            f"Closed-set accuracy: {closed['accuracy']:.6f}",
            f"Closed-set macro-F1: {closed['macro_f1']:.6f}",
            "",
            f"Abstained: {low['abstained_count']}",
            f"Coverage: {low['coverage']:.6f}",
            f"Accepted accuracy: {low['accepted_accuracy']:.6f}",
            "",
            "Confusion matrix rows are truth and columns are predicted:",
            "",
            "```json",
            json.dumps(closed["confusion_matrix"]),
            "```",
            "",
        )
    )
    return "\n".join(lines)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--onnx", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--out-json", type=Path, required=True)
    parser.add_argument("--out-md", type=Path, required=True)
    parser.add_argument("--imgsz", type=int, default=160)
    parser.add_argument("--batch", type=int, default=16)
    parser.add_argument("--low-confidence", type=float, default=0.70)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.imgsz < 1 or args.batch < 1:
        raise ValueError("imgsz and batch must be positive")

    import onnxruntime as ort
    from ultralytics.data.augment import classify_transforms

    paths, truth, class_names = collect_imagefolder(args.data)
    transform = classify_transforms(args.imgsz)
    session = ort.InferenceSession(
        str(args.onnx),
        providers=["CPUExecutionProvider"],
    )
    input_name = session.get_inputs()[0].name
    probabilities: list[list[float]] = []
    for start in range(0, len(paths), args.batch):
        tensors = []
        for path in paths[start : start + args.batch]:
            with Image.open(path) as image:
                tensors.append(transform(image.convert("RGB")).numpy())
        batch = np.stack(tensors).astype(np.float32)
        output = np.asarray(session.run(None, {input_name: batch})[0], dtype=np.float64)
        if output.ndim != 2 or output.shape[1] != len(class_names):
            raise RuntimeError(f"unexpected ONNX output shape: {output.shape}")
        if not (
            np.all(output >= 0)
            and np.all(output <= 1)
            and np.allclose(output.sum(axis=1), 1.0, atol=1e-4)
        ):
            output = _softmax(output)
        probabilities.extend(output.tolist())

    report = {
        "onnx_path": str(args.onnx.resolve()),
        "onnx_bytes": args.onnx.stat().st_size,
        "onnx_sha256": hashlib.sha256(args.onnx.read_bytes()).hexdigest(),
        "data_path": str(args.data.resolve()),
        "input_shape": [1, 3, args.imgsz, args.imgsz],
        "preprocessing": "Ultralytics classify_transforms: resize, center crop, RGB tensor, ImageNet normalization",
        "class_names": class_names,
        **classification_report(
            truth,
            probabilities,
            class_names,
            low_confidence_threshold=args.low_confidence,
        ),
        "predictions": [
            {
                "path": path.relative_to(args.data).as_posix(),
                "truth": class_names[actual],
                "prediction": class_names[int(np.argmax(row))],
                "confidence": float(max(row)),
                "abstained": float(max(row)) < args.low_confidence,
            }
            for path, actual, row in zip(paths, truth, probabilities, strict=True)
        ],
    }
    args.out_json.parent.mkdir(parents=True, exist_ok=True)
    args.out_md.parent.mkdir(parents=True, exist_ok=True)
    args.out_json.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    args.out_md.write_text(_markdown_report(report), encoding="utf-8")
    print(
        json.dumps(
            {
                "images": report["images"],
                "accuracy": report["closed_set"]["accuracy"],
                "macro_f1": report["closed_set"]["macro_f1"],
                "abstained": report["low_confidence"]["abstained_count"],
                "coverage": report["low_confidence"]["coverage"],
                "accepted_accuracy": report["low_confidence"]["accepted_accuracy"],
            },
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
