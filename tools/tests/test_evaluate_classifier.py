from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))


class EvaluateClassifierTest(unittest.TestCase):
    def test_imagefolder_records_use_stable_sorted_class_order(self) -> None:
        from tools.evaluate_classifier import collect_imagefolder

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for class_name in ("其他", "可回收物", "廚餘", "有害"):
                folder = root / class_name
                folder.mkdir()
                Image.new("RGB", (32, 32), "white").save(folder / f"{class_name}.png")

            paths, labels, class_names = collect_imagefolder(root)

            self.assertEqual(class_names, ["其他", "可回收物", "廚餘", "有害"])
            self.assertEqual(labels, [0, 1, 2, 3])
            self.assertEqual([path.name for path in paths], ["其他.png", "可回收物.png", "廚餘.png", "有害.png"])

    def test_report_includes_closed_set_and_low_confidence_metrics(self) -> None:
        from tools.evaluate_classifier import classification_report

        truth = [0, 0, 1, 1, 2, 2]
        probabilities = [
            [0.90, 0.05, 0.05],
            [0.60, 0.30, 0.10],
            [0.10, 0.80, 0.10],
            [0.05, 0.20, 0.75],
            [0.55, 0.35, 0.10],
            [0.05, 0.10, 0.85],
        ]
        class_names = ["可回收物", "廚餘", "有害"]

        report = classification_report(
            truth,
            probabilities,
            class_names,
            low_confidence_threshold=0.70,
        )

        self.assertAlmostEqual(report["closed_set"]["accuracy"], 4 / 6)
        self.assertEqual(report["closed_set"]["confusion_matrix"], [[2, 0, 0], [0, 1, 1], [1, 0, 1]])
        self.assertAlmostEqual(report["closed_set"]["per_class"]["可回收物"]["precision"], 2 / 3)
        self.assertAlmostEqual(report["closed_set"]["per_class"]["廚餘"]["recall"], 0.5)
        self.assertAlmostEqual(report["closed_set"]["per_class"]["有害"]["f1"], 0.5)
        self.assertEqual(report["low_confidence"]["abstained_count"], 2)
        self.assertAlmostEqual(report["low_confidence"]["coverage"], 4 / 6)
        self.assertAlmostEqual(report["low_confidence"]["accepted_accuracy"], 0.75)
        self.assertEqual(
            report["low_confidence"]["abstained_by_true_class"],
            {"可回收物": 1, "廚餘": 0, "有害": 1},
        )


if __name__ == "__main__":
    unittest.main()
