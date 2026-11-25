"""Validate the committed LVGL UI handoff package without rebuilding it."""
from __future__ import annotations

import csv
import hashlib
import json
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
HANDOFF = ROOT / "ui" / "handoff"
W, H = 800, 480


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def walk(node: dict):
    yield node
    for child in node.get("children", []):
        yield from walk(child)


class UiHandoffTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads(
            (HANDOFF / "manifest.json").read_text(encoding="utf-8")
        )
        cls.remotion = json.loads(
            (HANDOFF / "remotion-manifest.json").read_text(encoding="utf-8")
        )
        cls.verification = json.loads(
            (HANDOFF / "verification.json").read_text(encoding="utf-8")
        )
        cls.records = cls.manifest["screens"]

    def test_canvas_and_screen_inventory_are_complete(self):
        self.assertEqual(self.manifest["canvas"], {
            "width": W,
            "height": H,
            "rotation": 0,
            "colorDepth": 16,
            "pixelFormat": "RGB565",
        })
        states = [record["state"] for record in self.records]
        self.assertEqual(len(states), 26)
        self.assertEqual(len(states), len(set(states)))
        self.assertIn("idle", states)
        self.assertIn("scan_ready", states)
        self.assertIn("bin_choice", states)
        self.assertIn("learning", states)
        self.assertIn("summary", states)
        self.assertIn("ask_offline", states)

    def test_screenshots_are_exact_canvas_size_and_hashes_match(self):
        for record in self.records:
            path = HANDOFF / record["file"]
            self.assertTrue(path.is_file(), path)
            with Image.open(path) as image:
                self.assertEqual(image.size, (W, H), path)
                self.assertEqual(image.mode, "RGB", path)
            self.assertEqual(sha256(path), record["pngSha256"], path)
            self.assertLessEqual(record["baselineDiffRatio"], 0.02, path)

    def test_layout_trees_come_from_the_same_canvas(self):
        for record in self.records:
            path = HANDOFF / record["layout"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(sha256(path), record["layoutSha256"], path)
            layout = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(layout["state"], record["state"])
            self.assertEqual(layout["canvas"]["width"], W)
            self.assertEqual(layout["canvas"]["height"], H)
            root = layout["root"]
            self.assertEqual((root["x"], root["y"], root["w"], root["h"]),
                             (0, 0, W, H))
            nodes = list(walk(root))
            self.assertTrue(any(node.get("widget") == "label" for node in nodes))
            for node in nodes:
                self.assertGreaterEqual(node["x"], 0, (record["state"], node))
                self.assertGreaterEqual(node["y"], 0, (record["state"], node))
                self.assertLessEqual(node["x"] + node["w"], W,
                                     (record["state"], node))
                self.assertLessEqual(node["y"] + node["h"], H,
                                     (record["state"], node))
            for button in layout["derived"]["buttons"]:
                self.assertLessEqual(button["x"] + button["w"], W)
                self.assertLessEqual(button["y"] + button["h"], H)

    def test_every_flow_references_a_real_screen(self):
        states = {record["state"] for record in self.records}
        for flow in self.manifest["flows"]:
            for step in flow.get("steps", []):
                self.assertIn(step["state"], states, flow["id"])
            for state in flow.get("roundSequence", []):
                self.assertIn(state, states, flow["id"])

    def test_remotion_manifest_matches_the_screen_assets(self):
        composition = self.remotion["composition"]
        self.assertEqual(composition["width"], W)
        self.assertEqual(composition["height"], H)
        self.assertEqual(composition["fps"], 60)
        self.assertEqual(self.remotion["renderGuidance"]["screenTransition"], "cut")
        self.assertEqual(
            self.remotion["renderGuidance"]["greenbeanAnchorPx"],
            {"x": 688, "y": 112, "w": 64, "h": 64},
        )
        states = {record["state"] for record in self.records}
        for flow in self.remotion["flows"]:
            for step in flow.get("steps", []):
                self.assertIn(step["state"], states)

    def test_motion_frames_exist_and_have_matching_hashes(self):
        frames = self.manifest["motionFrames"]
        self.assertGreaterEqual(len(frames), 10)
        for frame in frames:
            path = HANDOFF / frame["file"]
            self.assertTrue(path.is_file(), path)
            with Image.open(path) as image:
                self.assertEqual(image.size, (W, H), path)
            self.assertEqual(sha256(path), frame["pngSha256"], path)
            self.assertFalse(frame["previewReducedMotion"])

    def test_svg_frames_and_atlas_parse(self):
        svg_dir = HANDOFF / "figma-import"
        for record in self.records:
            path = svg_dir / f"{record['state']}.svg"
            root = ET.parse(path).getroot()
            self.assertEqual(root.attrib["width"], str(W))
            self.assertEqual(root.attrib["height"], str(H))
        atlas = ET.parse(svg_dir / "screen-atlas.svg").getroot()
        self.assertGreater(int(atlas.attrib["width"]), W)
        self.assertGreater(int(atlas.attrib["height"]), H)
        self.assertEqual(len(list(atlas)), 1 + len(self.records))

    def test_verification_and_csv_index_agree(self):
        self.assertEqual(self.verification["screenCount"], len(self.records))
        self.assertEqual(self.verification["motionFrameCount"],
                         len(self.manifest["motionFrames"]))
        for state, item in self.verification["screens"].items():
            record = next(r for r in self.records if r["state"] == state)
            self.assertEqual(item["pngSha256"], record["pngSha256"])
            self.assertEqual(item["layoutSha256"], record["layoutSha256"])
        with (HANDOFF / "screen-index.csv").open(
            newline="", encoding="utf-8"
        ) as handle:
            rows = list(csv.DictReader(handle))
        self.assertEqual(len(rows), len(self.records))
        self.assertEqual({row["state"] for row in rows},
                         {record["state"] for record in self.records})

    def test_source_hashes_and_guide_files_are_present(self):
        for source in self.manifest["visualSource"]["sourceFiles"]:
            path = ROOT / source["path"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(sha256(path), source["sha256"], path)
        for relative in (
            "README.md",
            "screens/README.md",
            "layouts/README.md",
            "motion/README.md",
            "figma-import/README.md",
            "flow.mmd",
            "index.html",
            "tokens.json",
            "remotion-prompt.md",
        ):
            self.assertTrue((HANDOFF / relative).is_file(), relative)
        combined = "\n".join(
            path.read_text(encoding="utf-8")
            for path in (
                HANDOFF / "README.md",
                HANDOFF / "manifest.json",
                HANDOFF / "index.html",
            )
        )
        self.assertNotIn("小智", combined)


if __name__ == "__main__":
    unittest.main()