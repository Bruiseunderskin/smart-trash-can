#!/usr/bin/env python3
"""Group image records by exact and perceptual similarity for leakage-safe splits."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import os
import random
import shutil
import statistics
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image


IMAGE_SUFFIXES = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}
TARGET_CLASS_PREFIXES = (
    ("可回收物", "可回收物"),
    ("可回收垃圾", "可回收物"),
    ("厨余垃圾", "廚餘"),
    ("廚餘垃圾", "廚餘"),
    ("厨余", "廚餘"),
    ("廚餘", "廚餘"),
    ("有害垃圾", "有害"),
    ("有害", "有害"),
    ("其他垃圾", "其他"),
    ("其它垃圾", "其他"),
    ("其他", "其他"),
    ("其它", "其他"),
)


@dataclass(frozen=True)
class ImageRecord:
    path: Path
    source_dataset: str
    source_split: str
    source_class_id: int
    source_class_name: str
    target_class: str
    session_group: str | None = None


@dataclass(frozen=True)
class AnalyzedImage:
    record: ImageRecord
    sha256: str
    phash: str
    object_group: str


def _dct_matrix(size: int) -> np.ndarray:
    positions = np.arange(size, dtype=np.float64)
    frequencies = positions[:, None]
    coordinates = (2.0 * positions[None, :] + 1.0) * frequencies * math.pi / (2.0 * size)
    matrix = np.cos(coordinates) * math.sqrt(2.0 / size)
    matrix[0, :] *= math.sqrt(0.5)
    return matrix


_PHASH_MATRIX = _dct_matrix(32)


def perceptual_hash(path: Path) -> str:
    with Image.open(path) as source:
        grayscale = source.convert("L").resize((32, 32), Image.Resampling.LANCZOS)
        pixels = np.asarray(grayscale, dtype=np.float64)
    coefficients = _PHASH_MATRIX @ pixels @ _PHASH_MATRIX.T
    low_frequency = coefficients[:8, :8].reshape(-1)
    threshold = np.median(low_frequency[1:])
    bits = low_frequency > threshold
    value = 0
    for bit in bits:
        value = (value << 1) | int(bit)
    return f"{value:016x}"


def _visual_signature(path: Path) -> np.ndarray:
    with Image.open(path) as source:
        grayscale = source.convert("L").resize((32, 32), Image.Resampling.LANCZOS)
        values = np.asarray(grayscale, dtype=np.float64).reshape(-1)
    values -= values.mean()
    deviation = values.std()
    if deviation > 1e-6:
        values /= deviation
    return values


def _signature_distance(left: np.ndarray, right: np.ndarray) -> float:
    return float(np.sqrt(np.mean((left - right) ** 2)))


def visual_distance(left: Path, right: Path) -> float:
    return _signature_distance(_visual_signature(left), _visual_signature(right))


def _distance(left: str, right: str) -> int:
    return (int(left, 16) ^ int(right, 16)).bit_count()


def near_duplicate_match(
    left_hash: str,
    right_hash: str,
    left_path: Path,
    right_path: Path,
    *,
    hash_distance: int,
    visual_distance_limit: float,
) -> bool:
    return (
        _distance(left_hash, right_hash) <= hash_distance
        and visual_distance(left_path, right_path) <= visual_distance_limit
    )


def analyze_records(
    records: list[ImageRecord],
    *,
    near_duplicate_distance: int,
    visual_distance_limit: float = 0.08,
) -> list[AnalyzedImage]:
    if near_duplicate_distance < 0 or near_duplicate_distance > 64:
        raise ValueError("near_duplicate_distance must be between 0 and 64")
    if visual_distance_limit < 0:
        raise ValueError("visual_distance_limit must be non-negative")

    hashes = []
    signatures = []
    for record in records:
        hashes.append(
            (
                hashlib.sha256(record.path.read_bytes()).hexdigest(),
                perceptual_hash(record.path),
            )
        )
        signatures.append(_visual_signature(record.path))

    parent = list(range(len(records)))

    def find(index: int) -> int:
        while parent[index] != index:
            parent[index] = parent[parent[index]]
            index = parent[index]
        return index

    def union(left: int, right: int) -> None:
        left_root = find(left)
        right_root = find(right)
        if left_root != right_root:
            parent[right_root] = left_root

    for left in range(len(records)):
        for right in range(left + 1, len(records)):
            exact = hashes[left][0] == hashes[right][0]
            near = (
                _distance(hashes[left][1], hashes[right][1]) <= near_duplicate_distance
                and _signature_distance(signatures[left], signatures[right]) <= visual_distance_limit
            )
            if exact or near:
                union(left, right)

    members: dict[int, list[int]] = {}
    for index in range(len(records)):
        members.setdefault(find(index), []).append(index)

    groups: dict[int, str] = {}
    for indexes in members.values():
        stable_keys = sorted(
            f"{records[index].source_dataset}:{records[index].path.as_posix()}"
            for index in indexes
        )
        digest = hashlib.sha256("\n".join(stable_keys).encode("utf-8")).hexdigest()
        group_id = f"object-{digest[:16]}"
        for index in indexes:
            groups[index] = group_id

    return [
        AnalyzedImage(
            record=record,
            sha256=hashes[index][0],
            phash=hashes[index][1],
            object_group=groups[index],
        )
        for index, record in enumerate(records)
    ]


def assign_group_splits(
    analyzed: list[AnalyzedImage],
    *,
    seed: int,
) -> dict[str, str]:
    labels_by_group: dict[str, set[str]] = {}
    for item in analyzed:
        labels_by_group.setdefault(item.object_group, set()).add(item.record.target_class)
    conflicting = {
        group: labels
        for group, labels in labels_by_group.items()
        if len(labels) != 1
    }
    if conflicting:
        group, labels = next(iter(conflicting.items()))
        raise ValueError(f"near-duplicate group {group} has conflicting labels: {sorted(labels)}")

    assignments: dict[str, str] = {}
    target_classes = sorted(
        {next(iter(labels)) for labels in labels_by_group.values()}
    )
    for target_class in target_classes:
        groups = sorted(
            group
            for group, labels in labels_by_group.items()
            if target_class in labels
        )
        random.Random(f"{seed}:{target_class}").shuffle(groups)
        count = len(groups)
        validation_count = max(1, round(count * 0.15))
        test_count = max(1, round(count * 0.15))
        train_count = count - validation_count - test_count
        if train_count < 1:
            raise ValueError(f"not enough independent groups for {target_class}: {count}")
        for index, group in enumerate(groups):
            if index < train_count:
                assignments[group] = "train"
            elif index < train_count + validation_count:
                assignments[group] = "validation"
            else:
                assignments[group] = "test"
    return dict(sorted(assignments.items()))


def audit_group_splits(
    analyzed: list[AnalyzedImage],
    assignments: dict[str, str],
) -> list[dict[str, object]]:
    observed: dict[str, set[str]] = {}
    for item in analyzed:
        observed.setdefault(item.object_group, set()).add(assignments[item.object_group])
    return [
        {"object_group": group, "splits": sorted(splits)}
        for group, splits in sorted(observed.items())
        if len(splits) != 1
    ]


def audit_materialized_splits(
    analyzed: list[AnalyzedImage],
    split_by_path: dict[Path, str],
) -> list[dict[str, object]]:
    missing = [item.record.path.as_posix() for item in analyzed if item.record.path not in split_by_path]
    if missing:
        raise ValueError(f"missing split assignments for: {', '.join(missing[:5])}")

    observed: dict[str, set[str]] = {}
    for item in analyzed:
        observed.setdefault(item.object_group, set()).add(split_by_path[item.record.path])
    return [
        {"object_group": group, "splits": sorted(splits)}
        for group, splits in sorted(observed.items())
        if len(splits) != 1
    ]


def quarantine_conflicting_groups(
    analyzed: list[AnalyzedImage],
) -> tuple[list[dict[str, object]], list[AnalyzedImage]]:
    by_group: dict[str, list[AnalyzedImage]] = {}
    for item in analyzed:
        by_group.setdefault(item.object_group, []).append(item)

    conflicts = []
    retained = []
    for group, items in sorted(by_group.items()):
        labels = sorted({item.record.target_class for item in items})
        if len(labels) > 1:
            conflicts.append(
                {
                    "object_group": group,
                    "labels": labels,
                    "images": [item.record.path.as_posix() for item in items],
                }
            )
        else:
            retained.extend(items)
    return conflicts, retained


def summarize_analysis(analyzed: list[AnalyzedImage]) -> dict[str, object]:
    by_sha: dict[str, int] = {}
    by_group: dict[str, list[AnalyzedImage]] = {}
    for item in analyzed:
        by_sha[item.sha256] = by_sha.get(item.sha256, 0) + 1
        by_group.setdefault(item.object_group, []).append(item)

    group_sizes = [len(items) for items in by_group.values()]
    unique_hash_counts = [len({item.sha256 for item in items}) for items in by_group.values()]
    source_split_counts = [
        len({item.record.source_split for item in items})
        for items in by_group.values()
    ]
    session_groups = {
        item.record.session_group
        for item in analyzed
        if item.record.session_group is not None
    }
    return {
        "image_count": len(analyzed),
        "object_group_count": len(by_group),
        "exact_duplicate_extra_images": sum(count - 1 for count in by_sha.values()),
        "near_duplicate_groups": sum(count > 1 for count in unique_hash_counts),
        "multi_image_groups": sum(size > 1 for size in group_sizes),
        "mixed_source_split_groups": sum(count > 1 for count in source_split_counts),
        "largest_family_group_images": max(group_sizes, default=0),
        "median_family_group_images": statistics.median(group_sizes) if group_sizes else 0,
        "session_metadata_available": bool(session_groups),
        "known_session_group_count": len(session_groups),
    }


def load_labeled_records(source: Path, source_dataset: str) -> list[ImageRecord]:
    labels_path = source / "labels.csv"
    if not labels_path.is_file():
        raise FileNotFoundError(f"labels.csv not found under {source}")

    records = []
    with labels_path.open("r", newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            relative = Path(row["output_path"])
            path = source / relative
            if not path.is_file():
                raise FileNotFoundError(f"label points to missing image: {path}")
            source_parts = Path(row["source_path"]).parts
            source_class_id = int(source_parts[1]) if len(source_parts) > 1 else -1
            records.append(
                ImageRecord(
                    path=path,
                    source_dataset=source_dataset,
                    source_split=row["split"],
                    source_class_id=source_class_id,
                    source_class_name=row["source_class"],
                    target_class=row["target_class"],
                )
            )
    if not records:
        raise ValueError(f"labels.csv contains no images under {source}")
    return records


def map_target_class(source_class_name: str) -> str | None:
    for prefix, target_class in TARGET_CLASS_PREFIXES:
        if source_class_name.startswith(prefix):
            return target_class
    return None


def load_modelscope_records(source: Path, source_dataset: str = "modelscope:tany0699/garbage265") -> list[ImageRecord]:
    classname_path = source / "classname.txt"
    if not classname_path.is_file():
        raise FileNotFoundError(f"classname.txt not found under {source}")
    class_names = [
        line.strip()
        for line in classname_path.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]

    records = []
    for source_split in ("train", "val"):
        split_root = source / source_split
        if not split_root.is_dir():
            raise FileNotFoundError(f"source split not found: {split_root}")
        for class_dir in sorted(split_root.iterdir()):
            if not class_dir.is_dir() or not class_dir.name.isdecimal():
                continue
            class_id = int(class_dir.name)
            if class_id >= len(class_names):
                raise ValueError(f"class id {class_id} is outside classname.txt")
            source_class_name = class_names[class_id]
            target_class = map_target_class(source_class_name)
            if target_class is None:
                raise ValueError(f"unmapped source class: {source_class_name}")
            for path in sorted(class_dir.iterdir()):
                if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES:
                    records.append(
                        ImageRecord(
                            path=path,
                            source_dataset=source_dataset,
                            source_split=source_split,
                            source_class_id=class_id,
                            source_class_name=source_class_name,
                            target_class=target_class,
                        )
                    )
    if not records:
        raise ValueError(f"no supported images found under {source}")
    return records


def _link_or_copy(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    try:
        os.link(source, destination)
    except OSError:
        shutil.copy2(source, destination)


def build_grouped_dataset(
    *,
    source: Path,
    output: Path,
    source_dataset: str,
    near_duplicate_distance: int,
    visual_distance_limit: float = 0.08,
    seed: int,
) -> dict[str, object]:
    if output.exists() and any(output.iterdir()):
        raise FileExistsError(f"output directory is not empty: {output}")

    labels_path = source / "labels.csv"
    if labels_path.is_file():
        records = load_labeled_records(source, source_dataset)
        source_metadata = str(labels_path)
    else:
        records = load_modelscope_records(source, source_dataset)
        source_metadata = str(source / "classname.txt")
    analyzed = analyze_records(
        records,
        near_duplicate_distance=near_duplicate_distance,
        visual_distance_limit=visual_distance_limit,
    )
    source_audit = summarize_analysis(analyzed)
    quarantined_groups, analyzed = quarantine_conflicting_groups(analyzed)
    retained_audit = summarize_analysis(analyzed)
    assignments = assign_group_splits(analyzed, seed=seed)

    images = []
    split_by_path: dict[Path, str] = {}
    for item in analyzed:
        split = assignments[item.object_group]
        destination_name = f"{item.sha256[:12]}__{item.record.path.name}"
        destination = output / split / item.record.target_class / destination_name
        _link_or_copy(item.record.path, destination)
        split_by_path[item.record.path] = split
        images.append(
            {
                "path": destination.relative_to(output).as_posix(),
                "source_path": item.record.path.relative_to(source).as_posix(),
                "source_dataset": item.record.source_dataset,
                "source_split": item.record.source_split,
                "source_class_id": item.record.source_class_id,
                "source_class_name": item.record.source_class_name,
                "target_class": item.record.target_class,
                "session_group": item.record.session_group,
                "sha256": item.sha256,
                "phash": item.phash,
                "object_group": item.object_group,
                "split": split,
            }
        )

    leakage = audit_materialized_splits(analyzed, split_by_path)
    if leakage:
        raise RuntimeError(f"group leakage detected after materialization: {leakage[:3]}")

    split_counts = {
        split: {
            target_class: sum(
                1
                for image in images
                if image["split"] == split and image["target_class"] == target_class
            )
            for target_class in sorted({image["target_class"] for image in images})
        }
        for split in ("train", "validation", "test")
    }
    group_rows = {}
    for item in analyzed:
        row = group_rows.setdefault(
            item.object_group,
            {
                "object_group": item.object_group,
                "split": assignments[item.object_group],
                "target_class": item.record.target_class,
                "images": 0,
            },
        )
        row["images"] += 1

    output.mkdir(parents=True, exist_ok=True)
    report: dict[str, object] = {
        "schema_version": 1,
        "source_dataset": source_dataset,
        "source_metadata": source_metadata,
        "method": "64-bit DCT perceptual hash; union-find exact/near duplicate groups; group-aware stratified split",
        "near_duplicate_distance": near_duplicate_distance,
        "visual_distance_limit": visual_distance_limit,
        "visual_distance_method": "normalized 32x32 grayscale RMS",
        "seed": seed,
        "session_metadata_available": source_audit["session_metadata_available"],
        "source_audit": source_audit,
        "retained_audit": retained_audit,
        "quarantined_image_count": sum(len(item["images"]) for item in quarantined_groups),
        "quarantined_group_count": len(quarantined_groups),
        "split_ratios": {"train": 0.7, "validation": 0.15, "test": 0.15},
        "splits": split_counts,
        "images": images,
        "quarantined_groups": quarantined_groups,
        "leakage": leakage,
    }
    (output / "manifest.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    with (output / "groups.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=("object_group", "split", "target_class", "images"),
        )
        writer.writeheader()
        writer.writerows(sorted(group_rows.values(), key=lambda row: row["object_group"]))
    return report


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="Dataset tree containing labels.csv")
    parser.add_argument("--output", type=Path, required=True, help="New grouped ImageFolder output directory")
    parser.add_argument("--source-dataset", required=True, help="Stable source identifier recorded in the manifest")
    parser.add_argument(
        "--near-distance",
        type=int,
        default=8,
        help="Maximum 64-bit pHash Hamming distance for a near-duplicate group (default: 8)",
    )
    parser.add_argument(
        "--visual-distance",
        type=float,
        default=0.08,
        help="Maximum normalized 32x32 grayscale RMS for a near-duplicate match (default: 0.08)",
    )
    parser.add_argument("--seed", type=int, default=42, help="Deterministic split seed (default: 42)")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report = build_grouped_dataset(
        source=args.source,
        output=args.output,
        source_dataset=args.source_dataset,
        near_duplicate_distance=args.near_distance,
        visual_distance_limit=args.visual_distance,
        seed=args.seed,
    )
    print(
        json.dumps(
            {
                "output": str(args.output),
                "images": len(report["images"]),
                "splits": report["splits"],
                "leakage": report["leakage"],
            },
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileExistsError, FileNotFoundError, RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
