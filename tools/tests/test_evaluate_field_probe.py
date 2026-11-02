from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))


class EvaluateFieldProbeTest(unittest.TestCase):
    def test_manifest_verifies_hash_and_maps_hk_bins_to_model_classes(self) -> None:
        from tools.evaluate_field_probe import load_probe_images

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            recyclable = root / "recyclable" / "cardboard.jpg"
            hazardous = root / "hazardous" / "battery.jpg"
            recyclable.parent.mkdir()
            hazardous.parent.mkdir()
            Image.new("RGB", (12, 10), "white").save(recyclable)
            Image.new("RGB", (12, 10), "black").save(hazardous)
            manifest = {
                "images": [
                    {
                        "file": "recyclable/cardboard.jpg",
                        "bin_hk": "可回收物",
                        "source_class": "Cardboard",
                        "sha256_16": hashlib.sha256(recyclable.read_bytes()).hexdigest()[:16],
                    },
                    {
                        "file": "hazardous/battery.jpg",
                        "bin_hk": "有害垃圾",
                        "source_class": "batteries",
                        "sha256_16": hashlib.sha256(hazardous.read_bytes()).hexdigest()[:16],
                    },
                ]
            }
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps(manifest, ensure_ascii=False), encoding="utf-8")

            images = load_probe_images(manifest_path, expected_count=2)

            self.assertEqual([image.truth for image in images], ["可回收物", "有害"])
            self.assertEqual([image.source for image in images], ["realwaste", "battery"])

    def test_center_crop_uses_central_sixty_percent_square(self) -> None:
        from tools.evaluate_field_probe import center_square_crop

        image = Image.new("RGB", (100, 80), "black")
        for x in range(26, 74):
            for y in range(16, 64):
                image.putpixel((x, y), (255, 255, 255))

        cropped = center_square_crop(image)

        self.assertEqual(cropped.size, (48, 48))
        self.assertEqual(cropped.getpixel((0, 0)), (255, 255, 255))
        self.assertEqual(cropped.getpixel((47, 47)), (255, 255, 255))
        self.assertEqual(image.size, (100, 80))

    def test_summary_splits_realwaste_and_battery_sources(self) -> None:
        from tools.evaluate_field_probe import summarize_predictions

        predictions = [
            {
                "file": "real.jpg",
                "source": "realwaste",
                "truth": "其他",
                "prediction": "可回收物",
                "confidence": 0.8,
                "correct": False,
                "abstained": False,
            },
            {
                "file": "battery.jpg",
                "source": "battery",
                "truth": "有害",
                "prediction": "有害",
                "confidence": 0.9,
                "correct": True,
                "abstained": False,
            },
        ]

        summary = summarize_predictions(predictions)

        self.assertEqual(summary["overall"]["images"], 2)
        self.assertEqual(summary["overall"]["accuracy"], 0.5)
        self.assertEqual(summary["by_source"]["realwaste"]["accuracy"], 0.0)
        self.assertEqual(summary["by_source"]["battery"]["accuracy"], 1.0)
        self.assertEqual(summary["overall"]["abstained_count"], 0)

    def test_background_check_flags_studio_battery_contrast(self) -> None:
        from tools.evaluate_field_probe import (
            analyze_background_shortcut,
            summarize_predictions,
        )

        predictions = [
            {
                "file": f"battery-{index}.jpg",
                "source": "battery",
                "truth": "有害",
                "prediction": "有害",
                "confidence": 0.95,
                "correct": True,
                "abstained": False,
            }
            for index in range(3)
        ]
        predictions.extend(
            {
                "file": f"real-{index}.jpg",
                "source": "realwaste",
                "truth": "其他",
                "prediction": "可回收物",
                "confidence": 0.30,
                "correct": False,
                "abstained": True,
            }
            for index in range(14)
        )

        analysis = analyze_background_shortcut(summarize_predictions(predictions))

        self.assertTrue(analysis["signal_detected"])
        self.assertEqual(analysis["battery"]["accuracy"], 1.0)
        self.assertEqual(analysis["realwaste"]["accuracy"], 0.0)
        self.assertGreater(analysis["mean_confidence_gap"], 0.5)

    def test_background_check_distinguishes_confidence_bias_from_correct_shortcut(
        self,
    ) -> None:
        from tools.evaluate_field_probe import (
            analyze_background_shortcut,
            summarize_predictions,
        )

        predictions = []
        for index, correct in enumerate((True, False, False)):
            predictions.append(
                {
                    "file": f"battery-{index}.jpg",
                    "source": "battery",
                    "truth": "有害",
                    "prediction": "有害" if correct else "可回收物",
                    "confidence": 0.9,
                    "correct": correct,
                    "abstained": False,
                    "ambiguous": False,
                }
            )
        predictions.extend(
            {
                "file": f"real-{index}.jpg",
                "source": "realwaste",
                "truth": "可回收物",
                "prediction": "可回收物" if index < 5 else "其他",
                "confidence": 0.45,
                "correct": index < 5,
                "abstained": True,
                "ambiguous": False,
            }
            for index in range(14)
        )

        analysis = analyze_background_shortcut(summarize_predictions(predictions))

        self.assertFalse(analysis["signal_detected"])
        self.assertTrue(analysis["confidence_bias_signal"])
        self.assertEqual(analysis["battery_high_confidence_count"], 3)
        self.assertEqual(analysis["battery_high_confidence_correct_count"], 1)

    def test_crop_analysis_separates_realwaste_and_battery_changes(self) -> None:
        from tools.evaluate_field_probe import analyze_crop_effect

        baseline = {
            "overall": {"accuracy": 0.2, "coverage": 0.5, "mean_confidence": 0.4},
            "by_source": {
                "realwaste": {
                    "accuracy": 0.1,
                    "coverage": 0.4,
                    "mean_confidence": 0.3,
                },
                "battery": {
                    "accuracy": 1.0,
                    "coverage": 1.0,
                    "mean_confidence": 0.9,
                },
            },
        }
        cropped = {
            "overall": {"accuracy": 0.4, "coverage": 0.6, "mean_confidence": 0.55},
            "by_source": {
                "realwaste": {
                    "accuracy": 0.35,
                    "coverage": 0.5,
                    "mean_confidence": 0.45,
                },
                "battery": {
                    "accuracy": 1.0,
                    "coverage": 1.0,
                    "mean_confidence": 0.9,
                },
            },
        }

        analysis = analyze_crop_effect(baseline, cropped)

        self.assertAlmostEqual(analysis["overall"]["accuracy_delta"], 0.2)
        self.assertAlmostEqual(analysis["by_source"]["realwaste"]["accuracy_delta"], 0.25)
        self.assertAlmostEqual(analysis["by_source"]["battery"]["accuracy_delta"], 0.0)
        self.assertTrue(analysis["realwaste_framing_sensitive"])

    def test_summary_preserves_strict_and_ambiguity_neutral_accuracy(self) -> None:
        from tools.evaluate_field_probe import summarize_predictions

        predictions = [
            {
                "file": "ambiguous.jpg",
                "source": "realwaste",
                "truth": "廚餘",
                "prediction": "其他",
                "confidence": 0.8,
                "correct": False,
                "abstained": False,
                "ambiguous": True,
            },
            {
                "file": "clear.jpg",
                "source": "realwaste",
                "truth": "可回收物",
                "prediction": "可回收物",
                "confidence": 0.9,
                "correct": True,
                "abstained": False,
                "ambiguous": False,
            },
        ]

        summary = summarize_predictions(predictions)["overall"]

        self.assertEqual(summary["accuracy"], 0.5)
        self.assertEqual(summary["ambiguity_neutral_accuracy"], 1.0)
        self.assertEqual(summary["ambiguous_misclassified_count"], 1)

    def test_markdown_contains_required_field_probe_sections(self) -> None:
        from tools.evaluate_field_probe import render_markdown

        prediction = {
            "file": "recyclable/cardboard.jpg",
            "source": "realwaste",
            "truth": "可回收物",
            "prediction": "其他",
            "confidence": 0.8,
            "correct": False,
            "abstained": False,
            "ambiguous": False,
        }
        summary = {
            "overall": {
                "images": 1,
                "accuracy": 0.0,
                "macro_f1": 0.0,
                "abstained_count": 0,
                "coverage": 1.0,
                "accepted_accuracy": 0.0,
                "mean_confidence": 0.8,
                "per_class": {},
            },
            "by_source": {
                "realwaste": {
                    "images": 1,
                    "accuracy": 0.0,
                    "macro_f1": 0.0,
                    "abstained_count": 0,
                    "coverage": 1.0,
                    "accepted_accuracy": 0.0,
                    "mean_confidence": 0.8,
                },
                "battery": {
                    "images": 1,
                    "accuracy": 1.0,
                    "macro_f1": 1.0,
                    "abstained_count": 0,
                    "coverage": 1.0,
                    "accepted_accuracy": 1.0,
                    "mean_confidence": 0.9,
                },
            },
        }
        report = {
            "model": {"path": "best.onnx", "sha256": "abc"},
            "manifest": {"path": "manifest.json", "sha256": "def", "counts_by_bin": {}},
            "threshold": 0.70,
            "crop_fraction": 0.60,
            "experiments": {
                "baseline": {"summary": summary, "predictions": [prediction]},
                "center_crop_60": {"summary": summary, "predictions": [prediction]},
            },
            "analysis": {
                "background_shortcut": {
                    "signal_detected": False,
                    "interpretation": "背景捷徑未檢出。",
                },
                "crop_effect": {
                    "overall": {"accuracy_delta": 0.0, "coverage_delta": 0.0},
                    "by_source": {
                        "realwaste": {
                            "accuracy_delta": 0.0,
                            "coverage_delta": 0.0,
                        },
                        "battery": {
                            "accuracy_delta": 0.0,
                            "coverage_delta": 0.0,
                        },
                    },
                    "interpretation": "裁切無足夠改善。",
                },
            },
            "comparison": {
                "sc101iot": {
                    "images": 4,
                    "accuracy": 0.25,
                    "macro_f1": 0.25,
                    "coverage": 0.50,
                    "accepted_accuracy": 0.0,
                }
            },
            "ambiguous_files": ["other/miscellaneous-trash_33.jpg"],
        }

        markdown = render_markdown(report)

        self.assertIn("不可作為現場準確率估計", markdown)
        self.assertIn("| file | bin_hk | prediction | confidence | correct | abstained |", markdown)
        self.assertIn("RealWaste", markdown)
        self.assertIn("電池", markdown)
        self.assertIn("背景捷徑", markdown)
        self.assertIn("中心 60% 裁切", markdown)
        self.assertIn("SC101IOT", markdown)


if __name__ == "__main__":
    unittest.main()
