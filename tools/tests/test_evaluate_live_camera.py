from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))


class EvaluateLiveCameraTest(unittest.TestCase):
    def test_loads_only_labeled_provenance_verified_captures(self) -> None:
        from tools.evaluate_live_camera import load_labeled_captures

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            captures = []
            for index, label in enumerate(("其他", "可回收物", "廚餘", "有害", "unlabeled")):
                filename = f"{index}.jpg"
                Image.new("RGB", (32, 32), "white").save(root / filename)
                captures.append(
                    {
                        "file": filename,
                        "label": label,
                        "source": "SC101IOT live camera via ESP32-S31 UART",
                        "excluded_from_training": True,
                        "excluded_from_calibration": True,
                        "excluded_from_model_selection": True,
                    }
                )
            manifest = root / "captures.json"
            manifest.write_text(json.dumps({"captures": captures}), encoding="utf-8")

            selected = load_labeled_captures(
                manifest,
                ("其他", "可回收物", "廚餘", "有害"),
                min_per_class=1,
            )

            self.assertEqual(len(selected), 4)
            self.assertEqual([item.label for item in selected], list(("其他", "可回收物", "廚餘", "有害")))

    def test_rejects_capture_used_for_training(self) -> None:
        from tools.evaluate_live_camera import LiveManifestError, load_labeled_captures

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            Image.new("RGB", (32, 32), "white").save(root / "case.jpg")
            manifest = root / "captures.json"
            manifest.write_text(
                json.dumps(
                    {
                        "captures": [
                            {
                                "file": "case.jpg",
                                "label": "其他",
                                "source": "SC101IOT live camera via ESP32-S31 UART",
                                "excluded_from_training": False,
                                "excluded_from_calibration": True,
                                "excluded_from_model_selection": True,
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )

            with self.assertRaisesRegex(LiveManifestError, "provenance"):
                load_labeled_captures(manifest, ("其他",), min_per_class=1)

    def test_rejects_missing_class(self) -> None:
        from tools.evaluate_live_camera import LiveManifestError, load_labeled_captures

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            Image.new("RGB", (32, 32), "white").save(root / "case.jpg")
            manifest = root / "captures.json"
            manifest.write_text(
                json.dumps(
                    {
                        "captures": [
                            {
                                "file": "case.jpg",
                                "label": "其他",
                                "source": "SC101IOT live camera via ESP32-S31 UART",
                                "excluded_from_training": True,
                                "excluded_from_calibration": True,
                                "excluded_from_model_selection": True,
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )

            with self.assertRaisesRegex(LiveManifestError, "可回收物"):
                load_labeled_captures(
                    manifest,
                    ("其他", "可回收物"),
                    min_per_class=1,
                )


if __name__ == "__main__":
    unittest.main()
