#!/usr/bin/env python3
"""Select low/median/high-confidence host-correct images for on-device parity tests."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

from PIL import Image, ImageOps


def select_correct_cases(
    predictions: list[dict[str, Any]],
    class_names: tuple[str, ...],
    *,
    cases_per_class: int,
) -> list[dict[str, Any]]:
    if cases_per_class < 1:
        raise ValueError("cases_per_class must be positive")
    selected = []
    for class_name in class_names:
        correct = sorted(
            (
                item
                for item in predictions
                if item["truth"] == class_name and item["prediction"] == class_name
            ),
            key=lambda item: (float(item["confidence"]), item["path"]),
        )
        if len(correct) < cases_per_class:
            raise ValueError(
                f"only {len(correct)} correct host cases for {class_name}; "
                f"{cases_per_class} requested"
            )
        if cases_per_class == 1:
            indexes = [0]
        else:
            indexes = [
                round(index * (len(correct) - 1) / (cases_per_class - 1))
                for index in range(cases_per_class)
            ]
        selected.extend(correct[index] for index in indexes)
    return selected


def make_header(cases: list[dict[str, Any]]) -> str:
    lines = [
        "#pragma once",
        "#include <stdint.h>",
        "",
        "typedef struct {",
        "    const uint8_t *jpeg;",
        "    uint32_t jpeg_size;",
        "    const uint8_t *rgb;",
        "    uint32_t rgb_size;",
        "    const char *case_id;",
        "    const char *expected_class;",
        "    int expected_index;",
        "    float host_confidence;",
        "} on_device_case_t;",
        "",
    ]
    for index in range(len(cases)):
        lines.append(
            f'extern const uint8_t case_{index:02d}_jpg[] '
            f'asm("_binary_case_{index:02d}_jpg_start");'
        )
        lines.append(
            f'extern const uint8_t case_{index:02d}_jpg_end[] '
            f'asm("_binary_case_{index:02d}_jpg_end");'
        )
        lines.append(
            f'extern const uint8_t case_{index:02d}_rgb[] '
            f'asm("_binary_case_{index:02d}_rgb_start");'
        )
        lines.append(
            f'extern const uint8_t case_{index:02d}_rgb_end[] '
            f'asm("_binary_case_{index:02d}_rgb_end");'
        )
    lines.extend(("", "static const on_device_case_t ON_DEVICE_CASES[] = {"))
    for case in cases:
        lines.append(
            f'    {{case_{case["case_index"]:02d}_jpg, '
            f'(uint32_t)(case_{case["case_index"]:02d}_jpg_end - '
            f'case_{case["case_index"]:02d}_jpg), '
            f'case_{case["case_index"]:02d}_rgb, '
            f'(uint32_t)(case_{case["case_index"]:02d}_rgb_end - '
            f'case_{case["case_index"]:02d}_rgb), "{case["case_id"]}", '
            f'"{case["truth"]}", {case["expected_index"]}, '
            f'{case["host_confidence"]:.9f}f}},'
        )
    lines.extend(("};", ""))
    return "\n".join(lines)


def prepare_on_device_jpeg(
    source: Path,
    destination: Path,
    *,
    image_size: int,
    quality: int = 95,
) -> None:
    if image_size <= 0 or image_size % 8:
        raise ValueError("image_size must be a positive multiple of 8")
    with Image.open(source) as original:
        image = ImageOps.exif_transpose(original).convert("RGB")

    width, height = image.size
    scale = image_size / min(width, height)
    resized_size = (max(image_size, round(width * scale)), max(image_size, round(height * scale)))
    resized = image.resize(resized_size, Image.Resampling.BILINEAR)
    left = (resized.width - image_size) // 2
    top = (resized.height - image_size) // 2
    prepared = resized.crop((left, top, left + image_size, top + image_size))
    prepared.save(
        destination,
        format="JPEG",
        quality=quality,
        subsampling=2,
        progressive=False,
        optimize=False,
    )


def write_raw_rgb(source: Path, destination: Path) -> None:
    with Image.open(source) as image:
        rgb = image.convert("RGB")
        destination.write_bytes(rgb.tobytes())


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--metrics", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True, help="ImageFolder test split")
    parser.add_argument("--output", type=Path, required=True, help="Directory for selected jpg files")
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--cases-per-class", type=int, default=3)
    parser.add_argument("--image-size", type=int, default=160)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    metrics = json.loads(args.metrics.read_text(encoding="utf-8"))
    class_names = tuple(metrics["class_names"])
    selected = select_correct_cases(
        metrics["predictions"],
        class_names,
        cases_per_class=args.cases_per_class,
    )
    args.output.mkdir(parents=True, exist_ok=True)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.header.parent.mkdir(parents=True, exist_ok=True)

    cases = []
    for index, item in enumerate(selected):
        source = args.data / item["path"]
        destination = args.output / f"case_{index:02d}.jpg"
        raw_destination = args.output / f"case_{index:02d}.rgb"
        prepare_on_device_jpeg(source, destination, image_size=args.image_size)
        write_raw_rgb(destination, raw_destination)
        cases.append(
            {
                "case_index": index,
                "case_id": f"case_{index:02d}",
                "source_path": item["path"],
                "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                "file": destination.name,
                "sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
                "raw_file": raw_destination.name,
                "raw_sha256": hashlib.sha256(raw_destination.read_bytes()).hexdigest(),
                "image_size": args.image_size,
                "preprocessing": "short-edge bilinear resize and center crop",
                "truth": item["truth"],
                "expected_index": class_names.index(item["truth"]),
                "host_prediction": item["prediction"],
                "host_confidence": float(item["confidence"]),
                "host_abstained": bool(item["abstained"]),
            }
        )

    args.manifest.write_text(
        json.dumps(
            {
                "source_metrics": str(args.metrics.resolve()),
                "source_test_data": str(args.data.resolve()),
                "selection": "low, evenly spaced, and high confidence among host-correct cases per class; normalized to model input size for hardware JPEG decoding",
                "class_names": class_names,
                "cases": cases,
            },
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    args.header.write_text(make_header(cases), encoding="utf-8")
    print(json.dumps({"cases": len(cases), "output": str(args.output)}, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
