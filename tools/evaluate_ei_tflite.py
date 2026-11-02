#!/usr/bin/env python3
"""Evaluate the Waste-Rec TFLite export on a hash-verified test manifest.

This is a host accuracy check using LiteRT, not an ESP32 latency measurement.
It mirrors Edge Impulse's FIT_SHORTEST center crop and fixed-point bilinear
resize before quantizing RGB pixels into the model's int8 input tensor.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from importlib.metadata import PackageNotFoundError, version
from pathlib import Path
from typing import Any

import numpy as np
from PIL import Image


EI_LABELS = ("food", "hazardous", "other", "recyclable")
TARGET_TO_EI = {
    "廚餘": "food",
    "有害": "hazardous",
    "其他": "other",
    "可回收物": "recyclable",
}
FRAC_BITS = 14
FRAC_VALUE = 1 << FRAC_BITS
FRAC_MASK = FRAC_VALUE - 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, required=True, help="Local EI Waste-Rec int8 .tflite file")
    parser.add_argument("--manifest", type=Path, required=True, help="YOLO evaluation manifest JSON")
    parser.add_argument("--test-root", type=Path, required=True, help="ImageFolder root for the manifest test split")
    parser.add_argument("--output", type=Path, help="Optional JSON report path")
    parser.add_argument("--threads", type=int, default=2)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    if args.threads < 1:
        parser.error("--threads must be positive")
    return args


def resize_fit_shortest_rgb(image: np.ndarray, size: int) -> np.ndarray:
    """Mirror Edge Impulse's RGB FIT_SHORTEST crop + bilinear resize."""
    if image.ndim != 3 or image.shape[2] != 3:
        raise ValueError(f"expected HxWx3 RGB image, got {image.shape}")
    source_height, source_width, _ = image.shape
    if min(source_height, source_width) < 2:
        raise ValueError(f"image is too small to resize: {image.shape}")

    if source_width > source_height:
        crop_width = (size * source_height) // size
        crop_height = source_height
    else:
        crop_height = (size * source_width) // size
        crop_width = source_width
    left = (source_width - crop_width) // 2
    top = (source_height - crop_height) // 2
    cropped = image[top : top + crop_height, left : left + crop_width]

    if crop_width == size and crop_height == size:
        return np.ascontiguousarray(cropped)

    x_step = (crop_width * FRAC_VALUE) // size
    y_step = (crop_height * FRAC_VALUE) // size
    x_acc = np.arange(size, dtype=np.uint32) * x_step
    y_acc = np.arange(size, dtype=np.uint32) * y_step
    x0 = x_acc >> FRAC_BITS
    y0 = y_acc >> FRAC_BITS
    # The SDK's RGB resize reads the right/bottom neighbor even when an input
    # side is smaller than the destination. Clamp those neighbors to the edge
    # so host evaluation is deterministic instead of reading past the crop.
    x1 = np.minimum(x0 + 1, crop_width - 1)
    y1 = np.minimum(y0 + 1, crop_height - 1)
    x_fraction = x_acc & FRAC_MASK
    y_fraction = y_acc & FRAC_MASK
    x_inverse = FRAC_VALUE - x_fraction
    y_inverse = FRAC_VALUE - y_fraction

    source = cropped.astype(np.uint32, copy=False)
    p00 = source[y0[:, None], x0[None, :]]
    p10 = source[y0[:, None], x1[None, :]]
    p01 = source[y1[:, None], x0[None, :]]
    p11 = source[y1[:, None], x1[None, :]]
    top_row = (
        p00 * x_inverse[None, :, None]
        + p10 * x_fraction[None, :, None]
        + FRAC_VALUE // 2
    ) >> FRAC_BITS
    bottom_row = (
        p01 * x_inverse[None, :, None]
        + p11 * x_fraction[None, :, None]
        + FRAC_VALUE // 2
    ) >> FRAC_BITS
    resized = (
        top_row * y_inverse[:, None, None]
        + bottom_row * y_fraction[:, None, None]
        + FRAC_VALUE // 2
    ) >> FRAC_BITS
    return np.ascontiguousarray(resized.astype(np.uint8))


def load_rgb_input(path: Path, size: int, scale: float, zero_point: int) -> np.ndarray:
    with Image.open(path) as image:
        rgb = np.asarray(image.convert("RGB"), dtype=np.uint8)
    resized = resize_fit_shortest_rgb(rgb, size)
    real_values = resized.astype(np.float32) / 255.0
    quantized = np.rint(real_values / scale + zero_point)
    return np.clip(quantized, -128, 127).astype(np.int8)[None, ...]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def calculate_metrics(confusion: np.ndarray) -> dict[str, Any]:
    per_class: dict[str, float] = {}
    for index, label in enumerate(EI_LABELS):
        tp = int(confusion[index, index])
        fp = int(confusion[:, index].sum()) - tp
        fn = int(confusion[index, :].sum()) - tp
        denominator = 2 * tp + fp + fn
        per_class[label] = (2 * tp / denominator) if denominator else 0.0
    count = int(confusion.sum())
    correct = int(np.trace(confusion))
    return {
        "accuracy": correct / count if count else 0.0,
        "macro_f1": sum(per_class.values()) / len(per_class),
        "per_class_f1": per_class,
        "confusion_matrix": confusion.tolist(),
        "confusion_matrix_label_order": list(EI_LABELS),
    }


def main() -> int:
    args = parse_args()
    for path in (args.model, args.manifest, args.test_root):
        if not path.exists():
            raise FileNotFoundError(path)
    if args.output and args.output.exists() and not args.force:
        raise FileExistsError(f"output exists: {args.output}; use --force to replace it")

    try:
        from ai_edge_litert.interpreter import Interpreter
        runtime_version = version("ai-edge-litert")
    except (ImportError, PackageNotFoundError) as exc:
        raise ImportError("install the host runtime with `pip install -r tools/requirements-tflite-eval.txt`") from exc

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    test_manifest = manifest.get("test")
    if not isinstance(test_manifest, dict) or not isinstance(test_manifest.get("images"), list):
        raise ValueError("manifest must contain test.images entries")
    entries = test_manifest["images"]
    if test_manifest.get("count") != len(entries):
        raise ValueError("test manifest count does not match its image entries")

    interpreter = Interpreter(model_path=str(args.model), num_threads=args.threads)
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]
    input_shape = tuple(int(value) for value in input_details["shape"])
    output_shape = tuple(int(value) for value in output_details["shape"])
    if input_shape != (1, 160, 160, 3) or input_details["dtype"] != np.int8:
        raise ValueError(f"unexpected model input: shape={input_shape}, dtype={input_details['dtype']}")
    if output_shape != (1, len(EI_LABELS)):
        raise ValueError(f"unexpected model output: shape={output_shape}")
    scale, zero_point = input_details["quantization"]
    if not np.isclose(scale, 1 / 255, rtol=1e-6) or int(zero_point) != -128:
        raise ValueError(f"unexpected input quantization: scale={scale}, zero_point={zero_point}")

    confusion = np.zeros((len(EI_LABELS), len(EI_LABELS)), dtype=np.int64)
    verified = 0
    for item in entries:
        relative_path = Path(item["path"])
        if relative_path.is_absolute() or ".." in relative_path.parts or len(relative_path.parts) < 2:
            raise ValueError(f"unsafe or unclassified test path: {item['path']}")
        target_class = relative_path.parts[0]
        expected_label = TARGET_TO_EI.get(target_class)
        if expected_label is None:
            raise ValueError(f"unmapped target class in manifest: {target_class}")
        image_path = args.test_root / relative_path
        if not image_path.is_file():
            raise FileNotFoundError(f"missing manifest image: {image_path}")
        actual_hash = sha256_file(image_path)
        if actual_hash != item["sha256"]:
            raise ValueError(f"SHA-256 mismatch: {image_path}")
        verified += 1

        image_input = load_rgb_input(image_path, input_shape[1], float(scale), int(zero_point))
        interpreter.set_tensor(int(input_details["index"]), image_input)
        interpreter.invoke()
        scores = interpreter.get_tensor(int(output_details["index"]))[0]
        prediction = int(np.argmax(scores))
        truth = EI_LABELS.index(expected_label)
        confusion[truth, prediction] += 1

    try:
        model_runtime = runtime_version
    except NameError:  # pragma: no cover - the import error path exits above
        model_runtime = "unknown"
    result: dict[str, Any] = {
        "runtime": "ai-edge-litert",
        "runtime_version": model_runtime,
        "evaluation": "host TFLite accuracy evaluation; not ESP32 inference latency",
        "model_sha256": sha256_file(args.model),
        "input_shape": list(input_shape),
        "input_dtype": "int8",
        "input_quantization_scale": float(scale),
        "input_quantization_zero_point": int(zero_point),
        "preprocessing": "Edge Impulse FIT_SHORTEST center crop and fixed-point bilinear RGB resize; uint8 RGB mapped to int8; border neighbors are edge-clamped when upsampling",
        "label_order": list(EI_LABELS),
        "manifest_path": str(args.manifest),
        "manifest_test_count": len(entries),
        "verified_image_hashes": verified,
        **calculate_metrics(confusion),
    }
    rendered = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, FileExistsError, ImportError, OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
