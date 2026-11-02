from __future__ import annotations

import csv
import shutil
import random
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image, ImageEnhance


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))


def make_pattern(path: Path, offset: int = 0) -> None:
    image = Image.new("RGB", (96, 96), "white")
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            value = 40 + offset + (x * 2 + y * 3) % 170
            pixels[x, y] = (value, (value + x) % 256, (value + y * 2) % 256)
    image.save(path)


class GroupedDatasetTest(unittest.TestCase):
    def test_cli_exposes_reproducible_build_arguments(self) -> None:
        result = subprocess.run(
            [sys.executable, str(REPOSITORY_ROOT / "tools" / "grouped_dataset.py"), "--help"],
            capture_output=True,
            text=True,
            check=False,
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        for option in ("--source", "--output", "--source-dataset", "--near-distance", "--seed"):
            self.assertIn(option, result.stdout)

    def test_near_duplicate_images_share_object_group(self) -> None:
        from tools.grouped_dataset import ImageRecord, analyze_records

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            original_path = root / "original.png"
            dimmed_path = root / "dimmed.png"
            distinct_path = root / "distinct.png"

            make_pattern(original_path)
            with Image.open(original_path) as image:
                ImageEnhance.Brightness(image.convert("RGB")).enhance(0.88).save(dimmed_path)

            distinct = Image.new("RGB", (96, 96), "black")
            for y in range(0, distinct.height, 12):
                for x in range(0, distinct.width, 12):
                    if (x // 12 + y // 12) % 2 == 0:
                        distinct.paste("white", (x, y, x + 12, y + 12))
            distinct.save(distinct_path)

            records = [
                ImageRecord(
                    path=original_path,
                    source_dataset="unit-test",
                    source_split="train",
                    source_class_id=0,
                    source_class_name="source-a",
                    target_class="可回收物",
                ),
                ImageRecord(
                    path=dimmed_path,
                    source_dataset="unit-test",
                    source_split="validation",
                    source_class_id=0,
                    source_class_name="source-a",
                    target_class="可回收物",
                ),
                ImageRecord(
                    path=distinct_path,
                    source_dataset="unit-test",
                    source_split="train",
                    source_class_id=0,
                    source_class_name="source-a",
                    target_class="可回收物",
                ),
            ]

            analyzed = analyze_records(records, near_duplicate_distance=8)
            by_name = {item.record.path.name: item for item in analyzed}

            self.assertEqual(by_name["original.png"].object_group, by_name["dimmed.png"].object_group)
            self.assertNotEqual(by_name["original.png"].object_group, by_name["distinct.png"].object_group)

    def test_near_duplicate_match_rejects_visual_collision(self) -> None:
        from tools.grouped_dataset import near_duplicate_match, visual_distance

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            original_path = root / "original.png"
            dimmed_path = root / "dimmed.png"
            distinct_path = root / "distinct.png"
            make_pattern(original_path)
            with Image.open(original_path) as image:
                ImageEnhance.Brightness(image.convert("RGB")).enhance(0.88).save(dimmed_path)
            distinct = Image.new("RGB", (96, 96), "white")
            distinct.paste("black", (28, 28, 68, 68))
            distinct.save(distinct_path)

            self.assertLess(visual_distance(original_path, dimmed_path), 0.08)
            self.assertGreater(visual_distance(original_path, distinct_path), 0.08)
            self.assertTrue(
                near_duplicate_match(
                    "0000000000000000",
                    "0000000000000001",
                    original_path,
                    dimmed_path,
                    hash_distance=8,
                    visual_distance_limit=0.08,
                )
            )
            self.assertFalse(
                near_duplicate_match(
                    "0000000000000000",
                    "0000000000000000",
                    original_path,
                    distinct_path,
                    hash_distance=8,
                    visual_distance_limit=0.08,
                )
            )

    def test_group_split_is_deterministic_and_keeps_groups_together(self) -> None:
        from tools.grouped_dataset import (
            AnalyzedImage,
            ImageRecord,
            assign_group_splits,
            audit_group_splits,
        )

        analyzed = []
        for class_index, target_class in enumerate(("可回收物", "廚餘", "有害", "其他")):
            for group_index in range(20):
                group_id = f"object-{class_index:02d}-{group_index:02d}"
                for member_index in range(2 if group_index == 0 else 1):
                    analyzed.append(
                        AnalyzedImage(
                            record=ImageRecord(
                                path=Path(f"{class_index}-{group_index}-{member_index}.jpg"),
                                source_dataset="unit-test",
                                source_split="train",
                                source_class_id=class_index,
                                source_class_name=f"source-{class_index}",
                                target_class=target_class,
                            ),
                            sha256=f"{class_index:02x}{group_index:02x}",
                            phash=f"{group_index:016x}",
                            object_group=group_id,
                        )
                    )

        first = assign_group_splits(analyzed, seed=42)
        second = assign_group_splits(analyzed, seed=42)

        self.assertEqual(first, second)
        self.assertEqual(audit_group_splits(analyzed, first), [])
        for target_class in ("可回收物", "廚餘", "有害", "其他"):
            class_groups = {
                item.object_group
                for item in analyzed
                if item.record.target_class == target_class
            }
            split_counts = {
                split: sum(first[group] == split for group in class_groups)
                for split in ("train", "validation", "test")
            }
            self.assertEqual(split_counts, {"train": 14, "validation": 3, "test": 3})

    def test_materialized_split_audit_detects_group_leakage(self) -> None:
        from tools.grouped_dataset import (
            AnalyzedImage,
            ImageRecord,
            audit_materialized_splits,
        )

        analyzed = [
            AnalyzedImage(
                record=ImageRecord(
                    path=Path("train/original.jpg"),
                    source_dataset="unit-test",
                    source_split="train",
                    source_class_id=0,
                    source_class_name="source-a",
                    target_class="可回收物",
                ),
                sha256="same",
                phash="0000000000000000",
                object_group="object-shared",
            ),
            AnalyzedImage(
                record=ImageRecord(
                    path=Path("test/dimmed.jpg"),
                    source_dataset="unit-test",
                    source_split="validation",
                    source_class_id=0,
                    source_class_name="source-a",
                    target_class="可回收物",
                ),
                sha256="near",
                phash="0000000000000003",
                object_group="object-shared",
            ),
        ]

        violations = audit_materialized_splits(
            analyzed,
            {
                Path("train/original.jpg"): "train",
                Path("test/dimmed.jpg"): "test",
            },
        )

        self.assertEqual(len(violations), 1)
        self.assertEqual(violations[0]["object_group"], "object-shared")
        self.assertEqual(violations[0]["splits"], ["test", "train"])

    def test_conflicting_duplicate_groups_are_quarantined(self) -> None:
        from tools.grouped_dataset import (
            AnalyzedImage,
            ImageRecord,
            quarantine_conflicting_groups,
        )

        analyzed = [
            AnalyzedImage(
                record=ImageRecord(
                    path=Path("recyclable.jpg"),
                    source_dataset="unit-test",
                    source_split="train",
                    source_class_id=0,
                    source_class_name="可回收物-盒子",
                    target_class="可回收物",
                ),
                sha256="same",
                phash="0000000000000000",
                object_group="object-conflict",
            ),
            AnalyzedImage(
                record=ImageRecord(
                    path=Path("other.jpg"),
                    source_dataset="unit-test",
                    source_split="train",
                    source_class_id=1,
                    source_class_name="其他垃圾-纸盒",
                    target_class="其他",
                ),
                sha256="same",
                phash="0000000000000000",
                object_group="object-conflict",
            ),
            AnalyzedImage(
                record=ImageRecord(
                    path=Path("clean.jpg"),
                    source_dataset="unit-test",
                    source_split="train",
                    source_class_id=2,
                    source_class_name="其他垃圾-竹筷",
                    target_class="其他",
                ),
                sha256="clean",
                phash="ffffffffffffffff",
                object_group="object-clean",
            ),
        ]

        conflicts, retained = quarantine_conflicting_groups(analyzed)

        self.assertEqual([item["object_group"] for item in conflicts], ["object-conflict"])
        self.assertEqual([item.record.path.name for item in retained], ["clean.jpg"])

    def test_modelscope_tree_records_keep_source_class_and_split(self) -> None:
        from tools.grouped_dataset import load_modelscope_records

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "classname.txt").write_text(
                "可回收物-盒子\n其他垃圾-竹筷\n",
                encoding="utf-8",
            )
            samples = {
                root / "train" / "0" / "train-recyclable.png": "可回收物",
                root / "train" / "1" / "train-other.png": "其他",
                root / "val" / "0" / "val-recyclable.png": "可回收物",
            }
            for index, (path, _label) in enumerate(samples.items()):
                path.parent.mkdir(parents=True, exist_ok=True)
                noise = random.Random(index).randbytes(96 * 96 * 3)
                Image.frombytes("RGB", (96, 96), noise).save(path)

            records = load_modelscope_records(root)
            by_name = {record.path.name: record for record in records}

            self.assertEqual(by_name["train-recyclable.png"].source_class_id, 0)
            self.assertEqual(by_name["train-recyclable.png"].source_class_name, "可回收物-盒子")
            self.assertEqual(by_name["train-recyclable.png"].target_class, "可回收物")
            self.assertEqual(by_name["train-recyclable.png"].source_split, "train")
            self.assertEqual(by_name["val-recyclable.png"].source_split, "val")
            self.assertEqual(by_name["train-other.png"].target_class, "其他")

    def test_analysis_summary_counts_duplicate_and_original_split_linkage(self) -> None:
        from tools.grouped_dataset import AnalyzedImage, ImageRecord, summarize_analysis

        base = {
            "source_dataset": "unit-test",
            "source_class_id": 0,
            "source_class_name": "source-a",
            "target_class": "可回收物",
        }
        analyzed = [
            AnalyzedImage(
                record=ImageRecord(path=Path("a.jpg"), source_split="train", **base),
                sha256="same",
                phash="0000000000000000",
                object_group="object-a",
            ),
            AnalyzedImage(
                record=ImageRecord(path=Path("b.jpg"), source_split="val", **base),
                sha256="same",
                phash="0000000000000000",
                object_group="object-a",
            ),
            AnalyzedImage(
                record=ImageRecord(path=Path("c.jpg"), source_split="train", **base),
                sha256="different",
                phash="ffffffffffffffff",
                object_group="object-c",
            ),
        ]

        summary = summarize_analysis(analyzed)

        self.assertEqual(summary["image_count"], 3)
        self.assertEqual(summary["exact_duplicate_extra_images"], 1)
        self.assertEqual(summary["multi_image_groups"], 1)
        self.assertEqual(summary["mixed_source_split_groups"], 1)
        self.assertFalse(summary["session_metadata_available"])

    def test_build_writes_group_manifest_and_imagefolder_splits(self) -> None:
        from tools.grouped_dataset import build_grouped_dataset

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            output = root / "output"
            rows = []
            image_index = 0
            for class_index, class_name in enumerate(("可回收物", "廚餘", "有害", "其他")):
                for group_index in range(6):
                    noise = random.Random(class_index * 100 + group_index).randbytes(96 * 96 * 3)
                    base_image = Image.frombytes("RGB", (96, 96), noise)
                    for variant in range(2):
                        relative = Path("train") / class_name / f"{group_index}-{variant}.png"
                        path = source / relative
                        path.parent.mkdir(parents=True, exist_ok=True)
                        image = base_image if variant == 0 else ImageEnhance.Brightness(base_image).enhance(0.88)
                        image.save(path)
                        rows.append(
                            {
                                "output_path": relative.as_posix(),
                                "source_class": class_name,
                                "target_class": class_name,
                                "split": "train",
                                "source_path": f"train/0/{path.name}",
                            }
                        )
                        image_index += 1

            with (source / "labels.csv").open("w", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(handle, fieldnames=rows[0].keys())
                writer.writeheader()
                writer.writerows(rows)

            conflict_path = source / "train" / "其他" / "conflict.png"
            shutil.copy2(source / "train" / "可回收物" / "0-0.png", conflict_path)
            with (source / "labels.csv").open("a", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(handle, fieldnames=rows[0].keys())
                writer.writerow(
                    {
                        "output_path": conflict_path.relative_to(source).as_posix(),
                        "source_class": "其他垃圾-冲突",
                        "target_class": "其他",
                        "split": "train",
                        "source_path": "train/1/conflict.png",
                    }
                )

            report = build_grouped_dataset(
                source=source,
                output=output,
                source_dataset="unit-test/garbage",
                near_duplicate_distance=8,
                seed=42,
            )

            self.assertEqual(report["leakage"], [])
            self.assertEqual(len(report["quarantined_groups"]), 1)
            self.assertEqual(report["quarantined_groups"][0]["labels"], ["其他", "可回收物"])
            self.assertEqual(report["splits"].keys(), {"train", "validation", "test"})
            self.assertTrue((output / "manifest.json").is_file())
            self.assertTrue((output / "groups.csv").is_file())
            for split in ("train", "validation", "test"):
                for class_name in ("可回收物", "廚餘", "有害", "其他"):
                    self.assertTrue((output / split / class_name).is_dir())
            for image in report["images"]:
                self.assertEqual(image["source_dataset"], "unit-test/garbage")
                self.assertIsNone(image["session_group"])
                self.assertFalse(image["path"].endswith("conflict.png"))


if __name__ == "__main__":
    unittest.main()
