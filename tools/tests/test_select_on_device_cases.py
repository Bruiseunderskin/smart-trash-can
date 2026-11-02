from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))


class SelectOnDeviceCasesTest(unittest.TestCase):
    def test_prepares_model_input_jpeg_for_hardware_decoder(self) -> None:
        from tools.select_on_device_cases import prepare_on_device_jpeg, write_raw_rgb

        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "source.jpg"
            destination = Path(temp_dir) / "case.jpg"
            raw_destination = Path(temp_dir) / "case.rgb"
            Image.new("RGB", (300, 240), "blue").save(source, quality=90)

            prepare_on_device_jpeg(source, destination, image_size=160)
            write_raw_rgb(destination, raw_destination)

            with Image.open(destination) as image:
                self.assertEqual(image.size, (160, 160))
                self.assertFalse(image.info.get("progressive", 0))
            self.assertEqual(raw_destination.stat().st_size, 160 * 160 * 3)

    def test_selects_low_median_and_high_confidence_correct_cases(self) -> None:
        from tools.select_on_device_cases import select_correct_cases

        predictions = [
            {
                "path": f"{class_name}/{index}.jpg",
                "truth": class_name,
                "prediction": class_name if index != 6 else "other",
                "confidence": 0.99 if index == 6 else 0.1 * (index + 1),
            }
            for class_name in ("其他", "可回收物", "廚餘", "有害")
            for index in range(7)
        ]

        selected = select_correct_cases(
            predictions,
            ("其他", "可回收物", "廚餘", "有害"),
            cases_per_class=3,
        )

        self.assertEqual(len(selected), 12)
        for class_name in ("其他", "可回收物", "廚餘", "有害"):
            confidences = [
                item["confidence"]
                for item in selected
                if item["truth"] == class_name
            ]
            for actual, expected in zip(confidences, (0.1, 0.3, 0.6), strict=True):
                self.assertAlmostEqual(actual, expected)


if __name__ == "__main__":
    unittest.main()
