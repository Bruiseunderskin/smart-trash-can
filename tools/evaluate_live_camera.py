#!/usr/bin/env python3
"""Evaluate labeled live SC101IOT captures with an exported classifier."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import numpy as np
from PIL import Image

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.evaluate_classifier import classification_report


LIVE_CAMERA_SOURCE = "SC101IOT live camera via ESP32-S31 UART"
PROVENANCE_KEYS = (
    "excluded_from_training",
    "excluded_from_calibration",
    "excluded_from_model_selection",
)


class LiveManifestError(ValueError):
    """Raised when live captures are missing required labels or provenance."""


@dataclass(frozen=True)
class LabeledCapture:
    path: Path
    label: str
    sha256: str
    metadata: dict[str, Any]


def load_labeled_captures(
    manifest_path: Path,
    class_names: tuple[str, ...],
    *,
    min_per_class: int,
) -> list[LabeledCapture]:
    if min_per_class < 1:
        raise LiveManifestError("min_per_class must be positive")

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    entries = manifest.get("captures")
    if not isinstance(entries, list):
        raise LiveManifestError("capture manifest must contain a captures list")

    selected: list[LabeledCapture] = []
    counts = dict.fromkeys(class_names, 0)
    for entry in entries:
        if not isinstance(entry, dict):
            raise LiveManifestError("capture manifest entries must be objects")
        label = str(entry.get("label", ""))
        if label.casefold() == "unlabeled":
            continue
        if label not in class_names:
            raise LiveManifestError(f"unknown live-camera label: {label}")
        if entry.get("source") != LIVE_CAMERA_SOURCE:
            raise LiveManifestError(f"unexpected capture source for {entry.get('file')}")
        if any(entry.get(key) is not True for key in PROVENANCE_KEYS):
            raise LiveManifestError(f"live-camera provenance is incomplete for {entry.get('file')}")

        image_path = manifest_path.parent / str(entry.get("file", ""))
        if not image_path.is_file():
            raise LiveManifestError(f"missing live-camera image: {image_path}")
        digest = hashlib.sha256(image_path.read_bytes()).hexdigest()
        expected_digest = entry.get("sha256")
        if expected_digest is not None and digest != expected_digest:
            raise LiveManifestError(f"SHA-256 mismatch for live-camera image: {image_path.name}")

        selected.append(
            LabeledCapture(
                path=image_path,
                label=label,
                sha256=digest,
                metadata=entry,
            )
        )
        counts[label] += 1

    missing = [
        f"{name} ({counts[name]}/{min_per_class})"
        for name in class_names
        if counts[name] < min_per_class
    ]
    if missing:
        raise LiveManifestError(
            "not enough labeled live-camera images for: " + ", ".join(missing)
        )
    return sorted(selected, key=lambda capture: (class_names.index(capture.label), capture.path.name))


def _markdown_report(report: dict[str, Any]) -> str:
    lines = [
        "# SC101IOT live-camera evaluation",
        "",
        f"- Images: {report['images']}",
        f"- ONNX: `{report['onnx_path']}`",
        f"- ONNX SHA-256: `{report['onnx_sha256']}`",
        f"- Manifest SHA-256: `{report['manifest_sha256']}`",
        f"- Low-confidence threshold: {report['low_confidence']['threshold']}",
        "- Provenance: live camera; excluded from training, calibration, and model selection",
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
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--onnx", type=Path, required=True)
    parser.add_argument("--out-json", type=Path, required=True)
    parser.add_argument("--out-md", type=Path, required=True)
    parser.add_argument("--imgsz", type=int, default=160)
    parser.add_argument("--low-confidence", type=float, default=0.70)
    parser.add_argument("--min-per-class", type=int, default=1)
    parser.add_argument(
        "--class-name",
        action="append",
        dest="class_names",
        help="Expected class name; repeat four times",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    class_names = tuple(args.class_names or ("其他", "可回收物", "廚餘", "有害"))
    if len(set(class_names)) != len(class_names) or not class_names:
        raise LiveManifestError("class names must be unique and non-empty")

    import onnxruntime as ort
    from ultralytics.data.augment import classify_transforms

    captures = load_labeled_captures(
        args.manifest,
        class_names,
        min_per_class=args.min_per_class,
    )
    transform = classify_transforms(args.imgsz)
    session = ort.InferenceSession(str(args.onnx), providers=["CPUExecutionProvider"])
    input_name = session.get_inputs()[0].name
    probabilities: list[list[float]] = []
    for capture in captures:
        with Image.open(capture.path) as image:
            tensor = transform(image.convert("RGB")).numpy()[None].astype(np.float32)
        output = np.asarray(session.run(None, {input_name: tensor})[0], dtype=np.float64)
        if output.ndim != 2 or output.shape != (1, len(class_names)):
            raise LiveManifestError(f"unexpected ONNX output shape: {output.shape}")
        row = output[0]
        if not (
            np.all(row >= 0)
            and np.all(row <= 1)
            and np.isclose(row.sum(), 1.0, atol=1e-4)
        ):
            shifted = np.exp(row - row.max())
            row = shifted / shifted.sum()
        probabilities.append(row.tolist())

    truth = [class_names.index(capture.label) for capture in captures]
    metrics = classification_report(
        truth,
        probabilities,
        list(class_names),
        low_confidence_threshold=args.low_confidence,
    )
    report = {
        "camera": "SC101IOT",
        "board": "ESP32-S31-Korvo-1 V1.1",
        "manifest_path": str(args.manifest.resolve()),
        "manifest_sha256": hashlib.sha256(args.manifest.read_bytes()).hexdigest(),
        "onnx_path": str(args.onnx.resolve()),
        "onnx_sha256": hashlib.sha256(args.onnx.read_bytes()).hexdigest(),
        "preprocessing": "Ultralytics classify_transforms: resize, center crop, RGB tensor, ImageNet normalization",
        "provenance_verified": True,
        "class_names": list(class_names),
        **metrics,
        "predictions": [
            {
                "file": capture.path.name,
                "sha256": capture.sha256,
                "truth": capture.label,
                "prediction": class_names[int(np.argmax(row))],
                "confidence": float(max(row)),
                "abstained": float(max(row)) < args.low_confidence,
            }
            for capture, row in zip(captures, probabilities, strict=True)
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
            },
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, LiveManifestError, RuntimeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
