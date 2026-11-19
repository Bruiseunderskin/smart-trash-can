import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools import evaluate_capture_stress as stress  # noqa: E402


def row(regime, card_id, truth, prediction, confidence=0.9):
    return {"regime": regime, "card_id": card_id, "truth": truth, "prediction": prediction, "confidence": confidence}


class SummariseTests(unittest.TestCase):
    def test_per_class_accuracy_counts_recall_not_support(self):
        rows = [
            row("tight", "B-14", "food", "food"),
            row("tight", "B-15", "food", "recyclable"),
            row("tight", "B-22", "hazardous", "hazardous"),
        ]
        self.assertEqual(stress.per_class_accuracy(rows), {"food": 0.5, "hazardous": 1.0})

    def test_missing_card_id_does_not_create_a_card_row(self):
        rows = [row("held", "", "other", "other"), row("held", "B-28", "other", "recyclable")]
        self.assertEqual(stress.per_card_accuracy(rows), {"B-28": 0.0})

    def test_confusion_lists_only_errors_and_sorts_by_count(self):
        rows = [
            row("tight", "B-14", "hazardous", "recyclable"),
            row("tight", "B-15", "hazardous", "recyclable"),
            row("tight", "B-16", "food", "other"),
            row("tight", "B-17", "food", "food"),
        ]
        self.assertEqual(stress.confusion_counts(rows), {"hazardous->recyclable": 2, "food->other": 1})

    def test_summarise_reports_regimes_in_fixed_order_and_flags_failures(self):
        rows = [
            row("tight_pearl", "B-19", "hazardous", "recyclable"),
            row("nominal", "B-19", "hazardous", "hazardous"),
        ]
        report = stress.summarise(rows, "model-int8", fail_under=0.95)
        self.assertEqual(list(report["regimes"]), ["nominal", "tight_pearl"])
        self.assertTrue(report["regimes"]["nominal"]["pass"])
        self.assertFalse(report["regimes"]["tight_pearl"]["pass"])
        self.assertFalse(report["pass"])
        self.assertEqual(report["worst_regime"], "tight_pearl")
        self.assertIn("not printed-card", report["caveat"])

    def test_summarise_passes_when_every_regime_clears_the_floor(self):
        rows = [row("nominal", "B-19", "hazardous", "hazardous"), row("tight", "B-19", "hazardous", "hazardous")]
        report = stress.summarise(rows, "model", fail_under=0.95)
        self.assertTrue(report["pass"])

    def test_empty_rows_do_not_pass(self):
        report = stress.summarise([], "model")
        self.assertFalse(report["pass"])
        self.assertIsNone(report["worst_regime"])


class MarkdownTests(unittest.TestCase):
    def test_table_has_one_row_per_regime_with_per_class_columns(self):
        rows = [
            row("nominal", "B-14", "food", "food"),
            row("tight_pearl", "B-19", "hazardous", "recyclable"),
        ]
        text = stress.markdown([stress.summarise(rows, "model-int8")])
        self.assertIn("| model-int8 | tight_pearl |", text)
        self.assertIn("FAIL", text)
        self.assertIn("| food | hazardous | other | recyclable |", text)


class LabelTests(unittest.TestCase):
    def test_default_labels_are_the_frozen_four(self):
        self.assertEqual(stress.read_labels(None), ["food", "hazardous", "other", "recyclable"])

    def test_five_class_labels_add_the_unknown_exit(self):
        import tempfile

        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "labels.txt"
            path.write_text("food\nhazardous\nother\nrecyclable\nunknown\n", encoding="utf-8")
            self.assertEqual(stress.read_labels(path)[-1], "unknown")

    def test_labels_missing_a_bucket_are_rejected(self):
        import tempfile

        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "labels.txt"
            path.write_text("food\nhazardous\nother\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                stress.read_labels(path)

    def test_unknown_prediction_counts_as_a_miss(self):
        rows = [row("tight", "B-22", "hazardous", "unknown")]
        report = stress.summarise(rows, "model", labels=["food", "hazardous", "other", "recyclable", "unknown"])
        self.assertEqual(report["regimes"]["tight"]["accuracy"], 0.0)
        self.assertEqual(report["regimes"]["tight"]["confusion"], {"hazardous->unknown": 1})


class RegimeContractTests(unittest.TestCase):
    def test_pearl_regime_is_the_only_one_that_uses_the_sprite(self):
        self.assertEqual(stress.card_source("tight_pearl"), "sprites")
        self.assertEqual(stress.card_source("tight"), "cards")
        self.assertEqual(stress.card_source("nominal"), "cards")

    def test_every_regime_is_implemented(self):
        self.assertEqual(set(stress.REGIMES), {
            "nominal", "tight", "tight_pearl", "held", "isp", "low_light", "motion", "jpeg60",
        })


if __name__ == "__main__":
    unittest.main()
