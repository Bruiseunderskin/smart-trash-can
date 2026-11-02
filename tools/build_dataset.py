#!/usr/bin/env python3
"""Prepare the Garbage265 image-classification data for EI and YOLO11-cls."""

from __future__ import annotations

import argparse
import binascii
import csv
import hashlib
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import os
import random
import shutil
import struct
import sys
import zipfile
import zlib
from collections import defaultdict
from pathlib import Path


CLASS_ORDER = ("可回收物", "廚餘", "有害", "其他")
EI_LABELS = {"可回收物": "recyclable", "廚餘": "food", "有害": "hazardous", "其他": "other"}
IMAGE_SUFFIXES = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}
CLASS_PREFIXES = (
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


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--src", type=Path, help="Garbage265 root with train/ and val/ directories")
    parser.add_argument(
        "--download-images",
        action="store_true",
        help=(
            "Range-extract selected images from ModelScope OSS. The --src directory "
            "must already contain classname.txt, train.csv, and val.csv; this does not "
            "clone the dataset or fetch metadata."
        ),
    )
    parser.add_argument("--out", type=Path, default=Path("data/waste4"), help="Output folder (default: data/waste4)")
    parser.add_argument("--per-class-limit", type=int, help="Maximum source-train images per mapped waste class")
    parser.add_argument("--ei-zip", type=Path, help="Also create a flat class-folder ZIP from the sampled source-train images")
    parser.add_argument("--ei-per-class-limit", type=int, help="Optional cap for the EI ZIP; defaults to the training-set sample")
    parser.add_argument("--test-per-class-limit", type=int, default=250, help="Maximum held-out source-validation images per mapped class (default: 250)")
    parser.add_argument("--ei-test-zip", type=Path, help="Also create a flat class-folder ZIP from held-out source-validation images")
    parser.add_argument("--ei-test-per-class-limit", type=int, help="Optional cap for the EI test ZIP (default: 200/class)")
    parser.add_argument("--subclasses-per-class", type=int, default=5, help="Fine-grained source labels sampled within each of the four waste classes (default: 5)")
    parser.add_argument("--validation-fraction", type=float, default=0.15, help="Fraction of sampled source-train images used for val (default: 0.15)")
    parser.add_argument("--seed", type=int, default=42, help="Deterministic sampling seed (default: 42)")
    parser.add_argument("--force", action="store_true", help="Replace the selected output directory and EI ZIP")
    args = parser.parse_args()
    if args.per_class_limit is not None and args.per_class_limit < 2:
        parser.error("--per-class-limit must be at least 2 to leave training and validation samples")
    if args.ei_per_class_limit is not None and args.ei_per_class_limit < 2:
        parser.error("--ei-per-class-limit must be at least 2")
    if args.ei_per_class_limit is not None and not args.ei_zip:
        parser.error("--ei-per-class-limit requires --ei-zip")
    if args.test_per_class_limit is not None and args.test_per_class_limit < 1:
        parser.error("--test-per-class-limit must be positive")
    if args.ei_test_per_class_limit is not None and args.ei_test_per_class_limit < 1:
        parser.error("--ei-test-per-class-limit must be positive")
    if args.ei_test_per_class_limit is not None and not args.ei_test_zip:
        parser.error("--ei-test-per-class-limit requires --ei-test-zip")
    if args.subclasses_per_class < 1:
        parser.error("--subclasses-per-class must be positive")
    if not 0 < args.validation_fraction < 1:
        parser.error("--validation-fraction must be between 0 and 1")
    if args.src is None:
        parser.error("pass --src <garbage265-root>; --download-images only extracts image bytes from an existing metadata clone")
    return args


class OssRangeReader:
    """Minimal seekable reader that fetches only requested byte ranges from OSS."""

    def __init__(self, bucket: object, key: str, size: int):
        self.bucket = bucket
        self.key = key
        self.size = size
        self.position = 0

    def seek(self, offset: int, whence: int = os.SEEK_SET) -> int:
        if whence == os.SEEK_SET:
            self.position = offset
        elif whence == os.SEEK_CUR:
            self.position += offset
        elif whence == os.SEEK_END:
            self.position = self.size + offset
        else:
            raise ValueError(f"unsupported seek origin: {whence}")
        return self.position

    def tell(self) -> int:
        return self.position

    def read(self, count: int = -1) -> bytes:
        if count < 0:
            count = self.size - self.position
        count = min(count, self.size - self.position)
        if count <= 0:
            return b""
        response = self.bucket.get_object(
            self.key, byte_range=(self.position, self.position + count - 1)
        )
        try:
            data = response.read()
        finally:
            response.close()
        self.position += len(data)
        return data

    def readable(self) -> bool:
        return True

    def seekable(self) -> bool:
        return True


def download_dataset(
    destination: Path,
    class_names: list[str],
    train_limit: int | None,
    test_limit: int | None,
    seed: int,
    subclasses_per_class: int,
) -> dict[str, list[int]]:
    """Range-read balanced samples from ModelScope ZIPs without downloading multi-GB archives."""
    try:
        from modelscope.msdatasets.utils.oss_utils import OssUtilities
    except ImportError as exc:
        raise ImportError(
            "ModelScope dataset dependencies are required for --download-images; "
            "install `tools/requirements-data.txt` first"
        ) from exc

    train_csv = destination / "train.csv"
    val_csv = destination / "val.csv"
    if not train_csv.is_file() or not val_csv.is_file():
        raise FileNotFoundError(
            f"ModelScope metadata CSVs are missing under {destination}; "
            "clone tany0699/garbage265 first"
        )

    oss = OssUtilities("garbage265", "tany0699", "master")

    def count_categories(csv_path: Path, expected_prefix: str) -> dict[str, dict[int, int]]:
        counts: dict[str, dict[int, int]] = defaultdict(lambda: defaultdict(int))
        with csv_path.open("r", encoding="utf-8", newline="") as handle:
            for row in csv.DictReader(handle):
                relative = Path(row["image:FILE"])
                if len(relative.parts) < 3 or relative.parts[0] != expected_prefix:
                    raise ValueError(f"unexpected ModelScope image path: {row['image:FILE']}")
                if not row["category"].isdecimal():
                    raise ValueError(f"unexpected ModelScope category id: {row['category']}")
                index = int(row["category"])
                if index >= len(class_names):
                    raise ValueError(f"category id {index} is outside classname.txt")
                label = class_names[index]
                major = map_major_class(label)
                if major is not None:
                    counts[major][index] += 1
        return counts

    train_counts = count_categories(train_csv, "train")
    val_counts = count_categories(val_csv, "val")
    selected_categories: dict[str, list[int]] = {}
    for major in CLASS_ORDER:
        eligible = set(train_counts.get(major, {})) & set(val_counts.get(major, {}))
        ranked = sorted(
            eligible,
            key=lambda index: (min(train_counts[major][index], val_counts[major][index]), train_counts[major][index]),
            reverse=True,
        )
        if len(ranked) < subclasses_per_class:
            raise ValueError(f"not enough source subclasses for {major}: {len(ranked)}")
        selected_categories[major] = ranked[:subclasses_per_class]

    def download_split(split: str, zip_name: str, limit: int | None, counts: dict[str, dict[int, int]]) -> None:
        selected_ids = [index for major in CLASS_ORDER for index in selected_categories[major]]
        per_subclass = max(1, (limit + subclasses_per_class - 1) // subclasses_per_class) if limit is not None else None
        zip_key = os.path.join(oss.oss_dir, f"{zip_name}.zip")
        archive_size = oss.bucket.head_object(zip_key).content_length
        with zipfile.ZipFile(OssRangeReader(oss.bucket, zip_key, archive_size)) as archive:
            grouped: dict[int, list[zipfile.ZipInfo]] = defaultdict(list)
            for info in archive.infolist():
                parts = info.filename.split("/")
                if len(parts) < 3 or parts[0] != split or not parts[1].isdecimal():
                    continue
                if Path(parts[-1]).suffix.lower() in IMAGE_SUFFIXES:
                    grouped[int(parts[1])].append(info)

        selected_infos: dict[int, list[zipfile.ZipInfo]] = {}
        for index in selected_ids:
            infos = sorted(grouped.get(index, []), key=lambda item: item.header_offset)
            needed = min(counts[next(major for major in CLASS_ORDER if index in selected_categories[major])][index], per_subclass or len(infos))
            if len(infos) < needed:
                raise RuntimeError(f"ZIP contains {len(infos)} images for category {index}; metadata requires {needed}")
            selected_infos[index] = infos[:needed]

        target_root = destination / ("val" if split == "val" else "train")

        def fetch_category(index: int) -> tuple[int, int]:
            infos = selected_infos[index]
            pending = [
                info for info in infos
                if not (target_root / str(index) / Path(info.filename).name).is_file()
            ]
            if not pending:
                return index, 0
            start = min(info.header_offset for info in pending)
            last = max(pending, key=lambda item: item.header_offset)
            end = min(archive_size, last.header_offset + last.compress_size + 4096)
            response = oss.bucket.get_object(zip_key, byte_range=(start, end - 1))
            try:
                region = response.read()
            finally:
                response.close()
            if len(region) != end - start:
                raise RuntimeError(f"short range read for ModelScope category {index}: {len(region)} of {end - start} bytes")

            written = 0
            for info in pending:
                local = info.header_offset - start
                header = region[local : local + 30]
                if len(header) != 30:
                    raise RuntimeError(f"truncated local ZIP header for {info.filename}")
                (signature, _version, flags, method, _mtime, _mdate, crc, _compressed_size,
                 _uncompressed_size, filename_length, extra_length) = struct.unpack("<IHHHHHIIIHH", header)
                if signature != 0x04034B50 or flags & 0x1:
                    raise RuntimeError(f"unsupported local ZIP entry for {info.filename}")
                data_start = local + 30 + filename_length + extra_length
                compressed = region[data_start : data_start + info.compress_size]
                if len(compressed) != info.compress_size:
                    raise RuntimeError(f"truncated ZIP data for {info.filename}")
                if method == zipfile.ZIP_DEFLATED:
                    image_bytes = zlib.decompress(compressed, -15)
                elif method == zipfile.ZIP_STORED:
                    image_bytes = compressed
                else:
                    raise RuntimeError(f"unsupported ZIP compression {method} for {info.filename}")
                if len(image_bytes) != info.file_size or (binascii.crc32(image_bytes) & 0xFFFFFFFF) != crc:
                    raise RuntimeError(f"size/CRC validation failed for {info.filename}")
                target = target_root / str(index) / Path(info.filename).name
                target.parent.mkdir(parents=True, exist_ok=True)
                temporary = target.with_name(f".{target.name}.{os.getpid()}.part")
                try:
                    temporary.write_bytes(image_bytes)
                    os.replace(temporary, target)
                finally:
                    temporary.unlink(missing_ok=True)
                written += 1
            return index, written

        print(f"Range-reading {len(selected_ids)} category spans for {split} samples...", flush=True)
        downloaded = 0
        with ThreadPoolExecutor(max_workers=min(8, len(selected_ids))) as pool:
            futures = [pool.submit(fetch_category, index) for index in selected_ids]
            for future in as_completed(futures):
                _, count = future.result()
                downloaded += count
        print(f"{split}: {downloaded} selected images extracted from {len(selected_ids)} ZIP ranges", flush=True)

    destination.mkdir(parents=True, exist_ok=True)
    download_split("train", "train", train_limit, train_counts)
    download_split("val", "val", test_limit, val_counts)
    return selected_categories
def find_split(root: Path, names: tuple[str, ...]) -> Path:
    for name in names:
        candidate = root / name
        if candidate.is_dir():
            return candidate
    choices = ", ".join(names)
    raise FileNotFoundError(f"expected one of {choices} under {root}")


def read_class_names(root: Path) -> list[str]:
    path = root / "classname.txt"
    if not path.is_file():
        return []
    return [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def resolve_folder_label(folder: Path, class_names: list[str]) -> str:
    name = folder.name
    if name.isdecimal() and class_names:
        index = int(name)
        if index < len(class_names):
            return class_names[index]
    return name


def map_major_class(label: str) -> str | None:
    for prefix, major in CLASS_PREFIXES:
        if label.startswith(prefix):
            return major
    return None


def collect_images(split_dir: Path, class_names: list[str]) -> dict[str, list[tuple[str, Path]]]:
    grouped: dict[str, list[tuple[str, Path]]] = defaultdict(list)
    unknown: list[str] = []
    for folder in sorted(path for path in split_dir.iterdir() if path.is_dir() and not path.is_symlink()):
        label = resolve_folder_label(folder, class_names)
        major = map_major_class(label)
        if major is None:
            unknown.append(folder.name)
            continue
        for image in sorted(path for path in folder.iterdir() if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES):
            grouped[major].append((label, image))
    if unknown:
        preview = ", ".join(unknown[:8])
        raise ValueError(f"unmapped source class folder(s): {preview}; update CLASS_PREFIXES only after checking classname.txt")
    return grouped


def choose(files: list[tuple[str, Path]], limit: int | None, rng: random.Random) -> list[tuple[str, Path]]:
    if limit is None or len(files) <= limit:
        return list(files)
    return rng.sample(files, limit)


def unique_filename(label: str, path: Path) -> str:
    safe_label = label.replace("/", "_").replace("\\", "_")
    return f"{safe_label}--{path.name}"


def copy_partition(
    files: list[tuple[str, Path]],
    destination: Path,
    split: str,
    source_root: Path,
    output_root: Path,
    provenance: list[dict[str, str]],
) -> int:
    destination.mkdir(parents=True, exist_ok=True)
    for label, source in files:
        output = destination / unique_filename(label, source)
        shutil.copy2(source, output)
        provenance.append(
            {
                "output_path": output.relative_to(output_root).as_posix(),
                "source_class": label,
                "target_class": destination.name,
                "split": split,
                "source_path": source.relative_to(source_root).as_posix(),
            }
        )
    return len(files)


def write_provenance(
    output_root: Path,
    source_root: Path,
    provenance: list[dict[str, str]],
    train_source: dict[str, list[tuple[str, Path]]],
    test_source: dict[str, list[tuple[str, Path]]],
    counts: dict[str, dict[str, int]],
    duplicates_removed: int,
    seed: int,
) -> None:
    with (output_root / "labels.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=("output_path", "source_class", "target_class", "split", "source_path"),
        )
        writer.writeheader()
        writer.writerows(provenance)

    source_counts: dict[str, dict[str, int]] = defaultdict(
        lambda: {"source_images": 0, "output_images": 0}
    )
    source_targets: dict[str, str] = {}
    for split_source in (train_source, test_source):
        for target_class, files in split_source.items():
            for source_class, _path in files:
                previous_target = source_targets.setdefault(source_class, target_class)
                if previous_target != target_class:
                    raise ValueError(
                        f"source class maps to multiple targets: {source_class}: "
                        f"{previous_target}, {target_class}"
                    )
                source_counts[source_class]["source_images"] += 1
    for row in provenance:
        source_counts[row["source_class"]]["output_images"] += 1

    with (output_root / "mapping.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("source_class", "target_class", "source_images", "output_images", "status"))
        for source_class in sorted(source_targets):
            observed = source_counts[source_class]
            writer.writerow(
                (
                    source_class,
                    source_targets[source_class],
                    observed["source_images"],
                    observed["output_images"],
                    "selected" if observed["output_images"] else "mapped_not_selected",
                )
            )

    lines = [
        "# 資料集建置報告",
        "",
        f"- 來源目錄：`{source_root.name or source_root}`",
        f"- 隨機種子：`{seed}`",
        f"- 完全重複圖片移除：{duplicates_removed} 張",
        "- 未映射類別：0（發現未映射資料夾時程式會停止）",
        "",
        "## 每類 split 數量",
        "",
        "| 類別 | train | val | test |",
        "|---|---:|---:|---:|",
    ]
    for target_class in CLASS_ORDER:
        values = counts[target_class]
        lines.append(
            f"| {target_class} | {values['train']} | {values['val']} | {values['test']} |"
        )
    lines.extend(
        (
            "",
            "`labels.csv` 對應每張輸出圖片與來源類別、來源路徑及 split。",
            "`mapping.csv` 列出已映射來源類別及其來源與輸出數量。",
            "",
        )
    )
    (output_root / "report.md").write_text("\n".join(lines), encoding="utf-8")


def deduplicate_sources(*sources: dict[str, list[tuple[str, Path]]]) -> int:
    """Keep one exact image across all splits; reject conflicting broad labels."""
    seen: dict[str, str] = {}
    removed = 0
    for source in sources:  # Training has priority over the held-out source.
        for major in CLASS_ORDER:
            kept = []
            for item in sorted(source.get(major, []), key=lambda x: str(x[1])):
                digest = hashlib.sha256(item[1].read_bytes()).hexdigest()
                previous = seen.get(digest)
                if previous is not None:
                    if previous != major:
                        raise ValueError(f"identical image has conflicting labels: {previous}, {major}: {item[1]}")
                    removed += 1
                    continue
                seen[digest] = major
                kept.append(item)
            source[major] = kept
    return removed


def write_ei_zip(path: Path, selected: dict[str, list[tuple[str, Path]]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=1) as archive:
        for major in CLASS_ORDER:
            for _label, source in sorted(selected[major], key=lambda item: (item[0], item[1].name)):
                ei_label = EI_LABELS[major]
                filename = f"{ei_label}.{source.parent.name}.{source.name}"
                archive.write(source, arcname=f"{ei_label}/{filename}")


def main() -> int:
    args = parse_args()
    source = args.src or Path("data/garbage265")
    source_resolved = source.resolve()
    output_resolved = args.out.resolve()
    if (source_resolved == output_resolved or source_resolved in output_resolved.parents
            or output_resolved in source_resolved.parents):
        raise ValueError("source and output directories must not overlap")
    for archive in (args.ei_zip, args.ei_test_zip):
        if archive and (archive.resolve() == source_resolved or source_resolved in archive.resolve().parents):
            raise ValueError("EI ZIP must not be inside the source directory")
    if args.download_images:
        metadata_paths = [source / name for name in ("classname.txt", "train.csv", "val.csv")]
        missing = [path.name for path in metadata_paths if not path.is_file()]
        if missing:
            raise FileNotFoundError(
                f"--download-images requires an existing ModelScope tany0699/garbage265 metadata clone "
                f"under {source}; missing: {', '.join(missing)}. Clone the dataset metadata first."
            )
        class_names = read_class_names(source)
        selected_categories = download_dataset(
            source,
            class_names,
            args.per_class_limit,
            args.test_per_class_limit,
            args.seed,
            args.subclasses_per_class,
        )
    else:
        selected_categories = None

    if not source.is_dir():
        raise FileNotFoundError(f"dataset directory not found: {source}")
    train_dir = find_split(source, ("train",))
    source_test_dir = find_split(source, ("val", "validation", "valid"))
    if args.out.exists() and any(args.out.iterdir()):
        if not args.force:
            raise FileExistsError(f"output directory is not empty: {args.out}; use --force to replace it")
        shutil.rmtree(args.out)
    for path in (args.ei_zip, args.ei_test_zip):
        if path and path.exists():
            if not args.force:
                raise FileExistsError(f"EI ZIP already exists: {path}; use --force to replace it")
            path.unlink()

    class_names = read_class_names(source)
    train_source = collect_images(train_dir, class_names)
    test_source = collect_images(source_test_dir, class_names)
    duplicates_removed = deduplicate_sources(train_source, test_source)
    rng = random.Random(args.seed)
    test_rng = random.Random(args.seed)
    ei_test_rng = random.Random(args.seed + 1)
    selected: dict[str, list[tuple[str, Path]]] = {}
    test_selected: dict[str, list[tuple[str, Path]]] = {}
    ei_selected: dict[str, list[tuple[str, Path]]] = {}
    ei_test_selected: dict[str, list[tuple[str, Path]]] = {}
    for major in CLASS_ORDER:
        selected[major] = choose(train_source.get(major, []), args.per_class_limit, rng)
        if len(selected[major]) < 2:
            raise ValueError(f"source training split has fewer than two images for {major}")
        if not test_source.get(major):
            raise ValueError(f"source validation split has no images for {major}")
        test_selected[major] = choose(test_source[major], args.test_per_class_limit, test_rng)
        ei_selected[major] = []
        ei_test_selected[major] = choose(
            test_source[major], args.ei_test_per_class_limit or 200, ei_test_rng
        ) if args.ei_test_zip else []

    counts: dict[str, dict[str, int]] = {major: {"train": 0, "val": 0, "test": 0, "ei_train_zip": 0, "ei_test_zip": 0} for major in CLASS_ORDER}
    provenance: list[dict[str, str]] = []
    for major in CLASS_ORDER:
        files = list(selected[major])
        rng.shuffle(files)
        validation_count = max(1, round(len(files) * args.validation_fraction))
        validation_count = min(validation_count, len(files) - 1)
        validation_files = files[:validation_count]
        training_files = files[validation_count:]
        ei_selected[major] = choose(
            training_files, args.ei_per_class_limit, rng
        ) if args.ei_zip else []
        counts[major]["train"] = copy_partition(
            training_files, args.out / "train" / major, "train", source, args.out, provenance
        )
        counts[major]["val"] = copy_partition(
            validation_files, args.out / "val" / major, "val", source, args.out, provenance
        )
        counts[major]["test"] = copy_partition(
            test_selected[major], args.out / "test" / major, "test", source, args.out, provenance
        )
        counts[major]["ei_train_zip"] = len(ei_selected[major])
        counts[major]["ei_test_zip"] = len(ei_test_selected[major])

    if args.ei_zip:
        write_ei_zip(args.ei_zip, ei_selected)
    if args.ei_test_zip:
        write_ei_zip(args.ei_test_zip, ei_test_selected)

    (args.out / "dataset-report.json").write_text(
        json.dumps(
            {
                "source": str(source),
                "dataset": "ModelScope tany0699/garbage265",
                "source_license": "Apache-2.0 (per dataset card)",
                "exact_duplicates_removed": duplicates_removed,
                "ei_label_map": {EI_LABELS[name]: name for name in CLASS_ORDER},
                "seed": args.seed,
                "sample_method": (
                    "Choose fine-grained labels with the largest minimum count across source train/val, "
                    "then extract archive-order prefixes up to the requested per-major-class limit."
                    if selected_categories
                    else "Seeded sampling from mapped source classes, followed by a deterministic train/val split."
                ),
                "validation_fraction": args.validation_fraction,
                "per_class_limit_applies_to": "source train images before train/val split",
                "ei_per_class_limit": args.ei_per_class_limit or args.per_class_limit,
                "test_per_class_limit": args.test_per_class_limit,
                "ei_test_per_class_limit": args.ei_test_per_class_limit or 200,
                "subclasses_per_major_class": selected_categories,
                "test_source": source_test_dir.name,
                "classes": counts,
            },
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    write_provenance(
        args.out,
        source,
        provenance,
        train_source,
        test_source,
        counts,
        duplicates_removed,
        args.seed,
    )
    print(json.dumps({"output": str(args.out), "ei_train_zip": str(args.ei_zip) if args.ei_zip else None, "ei_test_zip": str(args.ei_test_zip) if args.ei_test_zip else None, "counts": counts}, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, FileExistsError, ImportError, RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
