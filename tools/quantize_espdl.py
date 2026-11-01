#!/usr/bin/env python3
"""Quantize a YOLO classification ONNX model to ESP-DL and score its test predictions."""

from __future__ import annotations

import argparse
import json
import random
import sys
from pathlib import Path
from typing import Any

import torch
from PIL import Image
from torch.utils.data import DataLoader, Dataset
from ultralytics.data.augment import classify_transforms


IMAGE_SUFFIXES = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--onnx", type=Path, required=True)
    parser.add_argument("--calib", type=Path, required=True, help="ImageFolder training split; never use test images here")
    parser.add_argument("--test", type=Path, required=True, help="Held-out ImageFolder test split")
    parser.add_argument("--out", type=Path, required=True, help="Output .espdl path")
    parser.add_argument("--imgsz", type=int, default=160)
    parser.add_argument("--calib-images", "--n", dest="calib_images", type=int, default=200)
    parser.add_argument("--target", default="esp32s31")
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    if args.imgsz < 1 or args.calib_images < 1:
        parser.error("--imgsz and --calib-images must be positive")
    return args


def image_files(root: Path) -> tuple[list[Path], list[int], list[str]]:
    classes = sorted(path for path in root.iterdir() if path.is_dir())
    if not classes:
        raise ValueError(f"expected class subdirectories under {root}")
    files: list[Path] = []
    labels: list[int] = []
    names = [path.name for path in classes]
    for label, folder in enumerate(classes):
        images = sorted(path for path in folder.rglob("*") if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES)
        files.extend(images)
        labels.extend([label] * len(images))
    if not files:
        raise ValueError(f"no supported images found in {root}")
    return files, labels, names


def balanced_sample(paths: list[Path], labels: list[int], class_names: list[str], count: int, seed: int) -> list[Path]:
    rng = random.Random(seed)
    by_class = {index: [] for index in range(len(class_names))}
    for path, label in zip(paths, labels, strict=True):
        by_class[label].append(path)
    base, remainder = divmod(count, len(class_names))
    selected: list[Path] = []
    remaining: list[Path] = []
    for index in range(len(class_names)):
        images = by_class[index]
        quota = base + (1 if index < remainder else 0)
        take = min(quota, len(images))
        selected.extend(rng.sample(images, take))
        remaining.extend(images)
    if len(selected) < count:
        selected_set = set(selected)
        leftovers = [path for path in remaining if path not in selected_set]
        selected.extend(rng.sample(leftovers, count - len(selected)))
    if len(selected) < count:
        raise ValueError(f"requested {count} calibration images but only found {len(selected)}")
    rng.shuffle(selected)
    return selected


class ImageDataset(Dataset[torch.Tensor]):
    def __init__(self, paths: list[Path], transform: Any) -> None:
        self.paths = paths
        self.transform = transform

    def __len__(self) -> int:
        return len(self.paths)

    def __getitem__(self, index: int) -> torch.Tensor:
        with Image.open(self.paths[index]) as image:
            return self.transform(image.convert("RGB"))


class TestDataset(Dataset[tuple[torch.Tensor, int]]):
    def __init__(self, paths: list[Path], labels: list[int], transform: Any) -> None:
        self.paths = paths
        self.labels = labels
        self.transform = transform

    def __len__(self) -> int:
        return len(self.paths)

    def __getitem__(self, index: int) -> tuple[torch.Tensor, int]:
        with Image.open(self.paths[index]) as image:
            return self.transform(image.convert("RGB")), self.labels[index]


def f1_report(confusion: list[list[int]], class_names: list[str]) -> dict[str, Any]:
    scores: dict[str, float] = {}
    for index, name in enumerate(class_names):
        tp = confusion[index][index]
        fp = sum(confusion[row][index] for row in range(len(class_names))) - tp
        fn = sum(confusion[index]) - tp
        denominator = 2 * tp + fp + fn
        scores[name] = (2 * tp / denominator) if denominator else 0.0
    return {"macro_f1": sum(scores.values()) / len(scores), "per_class_f1": scores, "confusion_matrix": confusion}


def evaluate_quantized(graph: Any, test_loader: DataLoader, class_names: list[str], device: str) -> dict[str, Any]:
    from esp_ppq.executor.torch import TorchExecutor

    executor = TorchExecutor(graph=graph, device=device)
    confusion = [[0 for _ in class_names] for _ in class_names]
    with torch.no_grad():
        for image, label in test_loader:
            image = image.to(device)
            output = executor(image)
            if isinstance(output, (list, tuple)):
                output = output[0]
            prediction = int(torch.argmax(output, dim=1).item())
            confusion[int(label.item())][prediction] += 1
    return f1_report(confusion, class_names)


def main() -> int:
    args = parse_args()
    for path in (args.onnx, args.calib, args.test):
        if not path.exists():
            raise FileNotFoundError(path)
    if args.out.exists() and not args.force:
        raise FileExistsError(f"output already exists: {args.out}; use --force to replace it")

    calib_paths, calib_labels, calib_classes = image_files(args.calib)
    test_paths, test_labels, test_classes = image_files(args.test)
    if calib_classes != test_classes:
        raise ValueError(f"class order differs between calibration and test data: {calib_classes} vs {test_classes}")
    selected = balanced_sample(calib_paths, calib_labels, calib_classes, args.calib_images, args.seed)
    transform = classify_transforms(args.imgsz)
    calib_loader = DataLoader(ImageDataset(selected, transform), batch_size=1, shuffle=False, num_workers=0)
    test_loader = DataLoader(TestDataset(test_paths, test_labels, transform), batch_size=1, shuffle=False, num_workers=0)

    from esp_ppq.api import QuantizationSettingFactory, espdl_quantize_onnx

    args.out.parent.mkdir(parents=True, exist_ok=True)

    def collate_fn(batch: torch.Tensor) -> torch.Tensor:
        return batch.to(args.device)

    quantized_graph = espdl_quantize_onnx(
        onnx_import_file=str(args.onnx),
        espdl_export_file=str(args.out),
        calib_dataloader=calib_loader,
        calib_steps=args.calib_images,
        input_shape=[1, 3, args.imgsz, args.imgsz],
        target=args.target,
        quant_type="w8a8",
        setting=QuantizationSettingFactory.espdl_setting(),
        collate_fn=collate_fn,
        device=args.device,
        error_report=True,
        export_test_values=True,
        verbose=1,
    )
    metrics = evaluate_quantized(quantized_graph, test_loader, test_classes, args.device)
    report = {
        "target": args.target,
        "quant_type": "w8a8",
        "calibration_images": len(selected),
        "test_images": len(test_paths),
        "input_shape": [1, 3, args.imgsz, args.imgsz],
        "espdl_path": str(args.out),
        "espdl_bytes": args.out.stat().st_size if args.out.exists() else None,
        **metrics,
    }
    output = json.dumps(report, ensure_ascii=False, indent=2)
    print(output)
    report_path = args.out.with_suffix(".metrics.json")
    report_path.write_text(output + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, FileExistsError, RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
