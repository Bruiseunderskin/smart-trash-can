import csv
import json
import math
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools import stc_confusion as sc  # noqa: E402


class SoftmaxTests(unittest.TestCase):
    def test_sums_to_one_and_preserves_order(self):
        out = sc.softmax([1.0, 2.0, 3.0, 0.0])
        self.assertAlmostEqual(sum(out), 1.0, places=9)
        self.assertEqual(len(out), 4)
        self.assertGreater(out[2], out[1])
        self.assertGreater(out[1], out[0])

    def test_is_stable_for_large_logits(self):
        out = sc.softmax([1000.0, 1000.0, 999.0])
        self.assertAlmostEqual(sum(out), 1.0, places=9)
        self.assertAlmostEqual(out[0], out[1], places=9)


class PriorTests(unittest.TestCase):
    def test_estimate_prior_averages_rows(self):
        prior = sc.estimate_prior([[0.1, 0.2, 0.3, 0.4], [0.3, 0.2, 0.1, 0.4]])
        self.assertAlmostEqual(prior[0], 0.2)
        self.assertAlmostEqual(prior[2], 0.2)
        self.assertAlmostEqual(prior[3], 0.4)

    def test_label_prior_counts_truths(self):
        prior = sc.label_prior(["food", "food", "other", "recyclable"])
        self.assertAlmostEqual(prior[sc.INDEX["food"]], 0.5)
        self.assertAlmostEqual(prior[sc.INDEX["hazardous"]], 0.0)
        self.assertAlmostEqual(sum(prior), 1.0)

    def test_adjust_prior_pushes_against_a_large_class(self):
        probs = [0.05, 0.05, 0.10, 0.80]
        prior = [0.15, 0.15, 0.20, 0.50]
        adjusted = sc.adjust_prior(probs, prior, tau=1.0)
        self.assertAlmostEqual(sum(adjusted), 1.0, places=9)
        self.assertLess(adjusted[3], probs[3])
        # The over-predicted class loses ground relative to the under-predicted one.
        self.assertGreater(adjusted[2] / adjusted[3], probs[2] / probs[3])

    def test_tau_zero_is_identity(self):
        probs = [0.05, 0.05, 0.10, 0.80]
        prior = [0.15, 0.15, 0.20, 0.50]
        self.assertAlmostEqual(sc.adjust_prior(probs, prior, tau=0.0)[3], 0.80, places=9)

    def test_negative_tau_rejected(self):
        with self.assertRaises(ValueError):
            sc.adjust_prior([0.25] * 4, [0.25] * 4, tau=-1.0)


class DecisionTests(unittest.TestCase):
    def test_confident_row_is_accepted(self):
        self.assertEqual(sc.decide([0.02, 0.03, 0.05, 0.90], 0.55, 0.15), "recyclable")

    def test_low_confidence_is_rejected(self):
        self.assertIsNone(sc.decide([0.26, 0.25, 0.25, 0.24], 0.55, 0.15))

    def test_close_runner_up_is_rejected_even_when_confident(self):
        # The medicine-bottle failure mode: high score, tiny margin.
        self.assertIsNone(sc.decide([0.10, 0.40, 0.05, 0.45], 0.55, 0.15))

    def test_margin_gate_boundary(self):
        self.assertEqual(sc.decide([0.10, 0.20, 0.05, 0.65], 0.55, 0.15), "recyclable")


class MetricTests(unittest.TestCase):
    def test_confusion_counts_rejections_separately(self):
        table = sc.confusion(
            [("food", "recyclable"), ("food", None), ("recyclable", "recyclable")]
        )
        self.assertEqual(table["rows"]["food"]["recyclable"], 1)
        self.assertEqual(table["rejected"]["food"], 1)
        self.assertEqual(table["rows"]["recyclable"]["recyclable"], 1)

    def test_per_class_and_macro_f1(self):
        table = sc.confusion(
            [("food", "food"), ("food", "recyclable"), ("recyclable", "recyclable")]
        )
        scores = sc.per_class(table)
        self.assertAlmostEqual(scores["food"]["recall"], 0.5)
        self.assertAlmostEqual(scores["food"]["precision"], 1.0)
        self.assertAlmostEqual(scores["recyclable"]["f1"], 2 * 0.5 * 1.0 / 1.5)
        self.assertGreater(sc.macro_f1(scores), 0.0)

    def test_ece_zero_for_perfectly_calibrated_bins(self):
        rows = [(True, 1.0)] * 10 + [(False, 0.0)] * 10
        self.assertAlmostEqual(sc.ece(rows, bins=10), 0.0, places=9)

    def test_ece_positive_when_confidence_is_wrong(self):
        rows = [(False, 0.9)] * 10
        self.assertAlmostEqual(sc.ece(rows, bins=10), 0.9, places=9)

    def test_accuracy_counts_rejections_as_misses(self):
        self.assertAlmostEqual(sc.accuracy([("food", "food"), ("food", None)]), 0.5)


class CliTests(unittest.TestCase):
    def test_summarise_end_to_end_and_cli(self):
        rows = [
            {"card_id": "B-22", "truth": "hazardous", "probs": [0.05, 0.25, 0.05, 0.65]},
            {"card_id": "B-22", "truth": "hazardous", "probs": [0.05, 0.45, 0.05, 0.45]},
            {"card_id": "B-14", "truth": "food", "probs": [0.60, 0.05, 0.05, 0.30]},
            {"card_id": "B-28", "truth": "other", "probs": [0.05, 0.10, 0.30, 0.55]},
        ]
        report = sc.summarise(rows, tau=1.0, min_conf=0.55, min_margin=0.15)
        self.assertEqual(report["rows"], 4)
        self.assertIn("B-22", report["per_card"])
        self.assertLessEqual(report["coverage"], 1.0)
        self.assertGreaterEqual(report["coverage"], 0.0)

        with tempfile.TemporaryDirectory() as temp_name:
            temp = Path(temp_name)
            csv_path = temp / "rows.csv"
            with csv_path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.writer(handle)
                writer.writerow(["card_id", "truth", *sc.LABELS])
                writer.writerow(["B-14", "food", 0.60, 0.05, 0.05, 0.30])
                writer.writerow(["B-22", "hazardous", 0.05, 0.25, 0.05, 0.65])
            out = temp / "report.json"
            self.assertEqual(sc.main(["--rows", str(csv_path), "--tau", "1.0", "--json-out", str(out)]), 0)
            payload = json.loads(out.read_text(encoding="utf-8"))
            self.assertIn("post_decision_accuracy", payload)
            self.assertEqual(payload["rows"], 2)

    def test_missing_probability_column_is_reported(self):
        with tempfile.TemporaryDirectory() as temp_name:
            path = Path(temp_name) / "bad.csv"
            path.write_text("card_id,truth,food\nB-01,recyclable,0.5\n", encoding="utf-8")
            with self.assertRaises(sc.ProbabilityError):
                sc.read_rows(path)


if __name__ == "__main__":
    unittest.main()
