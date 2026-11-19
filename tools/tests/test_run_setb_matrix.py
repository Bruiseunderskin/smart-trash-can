import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools import run_setb_matrix as runner  # noqa: E402


class ParsingTests(unittest.TestCase):
    def test_parse_csv_list_trims_and_dedupes(self):
        self.assertEqual(runner.parse_csv_list(" 0, 0.5 ,,1.0,0.5 "), ["0", "0.5", "1.0"])

    def test_parse_float_list_rejects_junk(self):
        with self.assertRaises(SystemExit):
            runner.parse_float_list("0,abc")

    def test_read_labels_accepts_any_permutation(self):
        import tempfile

        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "labels.txt"
            path.write_text("recyclable\nother\nhazardous\nfood\n", encoding="utf-8")
            self.assertEqual(runner.read_labels(path), ["recyclable", "other", "hazardous", "food"])

    def test_read_labels_rejects_missing_class(self):
        import tempfile

        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "labels.txt"
            path.write_text("food\nhazardous\nother\n", encoding="utf-8")
            with self.assertRaises(SystemExit):
                runner.read_labels(path)


class SweepTests(unittest.TestCase):
    def test_grid_is_cartesian_and_ordered(self):
        grid = runner.sweep_grid([0.0, 1.0], [0.5, 0.7], [0.1])
        self.assertEqual(len(grid), 4)
        self.assertEqual(grid[0], (0.0, 0.5, 0.1))
        self.assertEqual(grid[-1], (1.0, 0.7, 0.1))

    def test_choose_prefers_accurate_settings_that_keep_coverage(self):
        low_coverage = {"coverage": 0.4, "post_decision_accuracy": 0.99, "macro_f1": 0.99, "tau": 1.0, "min_conf": 0.75, "min_margin": 0.25, "ece": 0.01, "per_card": {}}
        balanced = {"coverage": 0.9, "post_decision_accuracy": 0.93, "macro_f1": 0.92, "tau": 1.0, "min_conf": 0.55, "min_margin": 0.15, "ece": 0.03, "per_card": {}}
        chosen = runner.choose([low_coverage, balanced], min_coverage=0.85)
        self.assertIs(chosen, balanced)

    def test_choose_falls_back_when_nothing_meets_coverage(self):
        low_coverage = {"coverage": 0.4, "post_decision_accuracy": 0.99, "macro_f1": 0.99, "tau": 1.0, "min_conf": 0.75, "min_margin": 0.25, "ece": 0.01, "per_card": {}}
        self.assertIs(runner.choose([low_coverage], min_coverage=0.85), low_coverage)

    def test_problem_card_floor_uses_flagged_cards_only(self):
        per_card = {"B-14": 0.99, "B-22": 0.62, "B-28": 0.80, "A-01": 0.10}
        self.assertAlmostEqual(runner.problem_card_floor(per_card), 0.62)

    def test_problem_card_floor_is_one_when_no_flagged_card_present(self):
        self.assertEqual(runner.problem_card_floor({"A-01": 0.2}), 1.0)

    def test_acceptance_flags_failed_criteria(self):
        report = {"coverage": 0.9, "post_decision_accuracy": 0.90, "ece": 0.02, "per_card": {}}
        checks = runner.passes_acceptance(report)
        self.assertTrue(checks["coverage>=0.85"])
        self.assertFalse(checks["accuracy>=0.95"])
        self.assertTrue(checks["ece<=0.05"])


class SummaryTests(unittest.TestCase):
    def test_summary_marks_failing_best_point(self):
        report = {
            "tau": 1.0,
            "min_conf": 0.55,
            "min_margin": 0.15,
            "post_decision_accuracy": 0.90,
            "coverage": 0.9,
            "macro_f1": 0.9,
            "ece": 0.02,
            "per_card": {"B-22": 0.7},
        }
        markdown = runner.summary_markdown({"model-float32": [report]}, {"model-float32": report})
        self.assertIn("| model-float32 | 1.0 | 0.55 | 0.15 |", markdown)
        self.assertIn("FAIL", markdown)
        self.assertIn("accuracy>=0.95", markdown)

    def test_summary_marks_passing_best_point(self):
        report = {
            "tau": 0.5,
            "min_conf": 0.55,
            "min_margin": 0.15,
            "post_decision_accuracy": 0.97,
            "coverage": 0.9,
            "macro_f1": 0.96,
            "ece": 0.02,
            "per_card": {"B-22": 0.98},
        }
        markdown = runner.summary_markdown({"m": [report]}, {"m": report})
        self.assertIn("PASS", markdown)


if __name__ == "__main__":
    unittest.main()


class PreprocessTests(unittest.TestCase):
    def test_div255_is_plain_scaling_and_imagenet_shifts_it(self):
        try:
            import numpy  # noqa: F401
        except ImportError:  # pragma: no cover - host without numpy
            self.skipTest("numpy not installed")
        import tempfile

        from PIL import Image

        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "grey.png"
            Image.new("RGB", (32, 32), (128, 128, 128)).save(path)
            plain = runner.preprocess(path, 160)
            self.assertEqual(plain.shape, (1, 3, 160, 160))
            self.assertAlmostEqual(float(plain.mean()), 128 / 255, places=3)
            shifted = runner.preprocess(path, 160, "imagenet")
            # Per-channel ImageNet normalisation of a mid-grey pixel.
            expected = [
                (128 / 255 - mean) / std
                for mean, std in ((0.485, 0.229), (0.456, 0.224), (0.406, 0.225))
            ]
            for channel, value in enumerate(expected):
                self.assertAlmostEqual(float(shifted[0, channel].mean()), value, places=3)

    def test_unknown_normalisation_is_rejected(self):
        import tempfile

        from PIL import Image

        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "grey.png"
            Image.new("RGB", (8, 8), (10, 20, 30)).save(path)
            with self.assertRaises(SystemExit):
                runner.preprocess(path, 160, "bgr")
