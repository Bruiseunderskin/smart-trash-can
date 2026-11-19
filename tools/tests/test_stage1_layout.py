import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAYOUT = ROOT / "design/layout.json"


def walk(node):
    yield node
    for child in node.get("children", []):
        yield from walk(child)


class Stage1LayoutTests(unittest.TestCase):
    def setUp(self):
        self.screens = json.loads(LAYOUT.read_text())["screens"]
        self.by_name = {screen["name"]: screen for screen in self.screens}

    def test_scan_ready_precedes_bin_choice(self):
        self.assertEqual(len(self.screens), 12)
        self.assertEqual(self.screens[-3]["name"], "state/scan_ready")
        self.assertEqual(self.screens[-2]["name"], "state/bin_choice")
        self.assertEqual(self.screens[-1]["name"], "state/chat")
        names = {node["name"] for node in walk(self.screens[-3])}
        self.assertIn("action/scan", names)
        self.assertIn("prompt", names)

    def test_bin_choice_has_camera_and_four_touch_buttons(self):
        choice = self.by_name["state/bin_choice"]
        names = {node["name"] for node in walk(choice)}
        self.assertIn("comp/camera", names)
        self.assertIn("prompt", names)
        buttons = [
            node for node in walk(choice)
            if node.get("name", "").startswith("action/bin-")
        ]
        self.assertEqual(
            [node["name"] for node in buttons],
            [f"action/bin-{i}" for i in range(4)],
        )
        for node in buttons:
            self.assertGreaterEqual(node["w"], 180)
            self.assertGreaterEqual(node["h"], 88)

    def test_learning_has_round_feedback_and_one_next_action(self):
        learning = self.by_name["state/learning"]
        names = {node["name"] for node in walk(learning)}
        expected = {"scanned", "dropped", "answer", "result", "score", "tip",
                    "action/next"}
        self.assertTrue(expected <= names, expected - names)
        actions = [
            node["name"] for node in walk(learning)
            if node.get("kind") == "button"
        ]
        self.assertEqual(actions, ["action/next"])

    def test_all_screens_reserve_badge_strip(self):
        for screen in self.screens:
            badge = next((n for n in walk(screen)
                          if n.get("name") == "debug-badge"), None)
            self.assertIsNotNone(badge, screen["name"])
            self.assertGreaterEqual(badge["y"], 370)

    def test_game_chat_shell_navigation(self):
        idle = {node["name"] for node in walk(self.by_name["state/idle"])}
        guide = {node["name"] for node in walk(self.by_name["state/category_guide"])}
        chat = {node["name"] for node in walk(self.by_name["state/chat"])}
        self.assertIn("action/guide", idle)
        self.assertIn("action/ask", guide)
        self.assertIn("action/home", chat)
        self.assertIn("prompt", chat)


if __name__ == "__main__":
    unittest.main()
