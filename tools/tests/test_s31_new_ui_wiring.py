"""Standalone S31 wiring for the v3 UI and the preserved S3 hand result."""

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GAME = ROOT / "firmware/esp32s31/game"


class S31NewUiWiringTest(unittest.TestCase):
    def test_first_start_shows_intro_before_starting_the_round(self):
        app = (GAME / "main/app_main.cpp").read_text(encoding="utf-8")
        port = (GAME / "main/ui_port.h").read_text(encoding="utf-8")
        bridge = (GAME / "main/ui_bridge.c").read_text(encoding="utf-8")
        game_ui = (ROOT / "ui/lvglpro/src/stc_ui.c").read_text(
            encoding="utf-8"
        )

        self.assertIn("bool stc_game_ui_show_intro(void);", port)
        self.assertIn("stc_game_ui_show_intro()", app)
        self.assertIn("g_intro_pending", app)
        self.assertIn("stc_ui_bridge_show_intro", bridge)
        self.assertIn(
            "bind_action(ui, start, STC_ACTION_START, -1, false);",
            game_ui,
        )

    def test_ask_renders_the_v3_chat_instead_of_a_noop(self):
        app = (GAME / "main/app_main.cpp").read_text(encoding="utf-8")
        port = (GAME / "main/ui_port.h").read_text(encoding="utf-8")
        bridge = (GAME / "main/ui_bridge.c").read_text(encoding="utf-8")

        self.assertIn("stc_game_ui_show_chat(voice_mode == STC_OK);", app)
        self.assertIn("void stc_game_ui_show_chat(bool voice_available);", port)
        self.assertIn("stc_ui_show_ask", bridge)
        self.assertIn("STC_CHAT_OFFLINE", bridge)

    def test_s3_drop_bin_remains_the_dropped_bin_and_score_input(self):
        app = (GAME / "main/app_main.cpp").read_text(encoding="utf-8")

        self.assertIn(".bin = (stc_bin_t)msg->bin", app)
        self.assertIn(
            "stc_game_controller_on_drop(&drop, round_id, now_ms())",
            app,
        )

    def test_scan_ready_hosts_the_live_camera_preview(self):
        game_ui = (ROOT / "ui/lvglpro/src/stc_ui.c").read_text(
            encoding="utf-8"
        )
        runtime = (
            ROOT
            / "firmware/xiaozhi-esp32/main/boards/espressif/"
            "esp32-s31-korvo-1-stc/stc_game_runtime.cc"
        ).read_text(encoding="utf-8")
        scan_ready = game_ui.split(
            "static void scan_ready_screen", 1
        )[1].split("static void capturing_screen", 1)[0]
        idle_preview = runtime.split("if (!has_request)", 1)[1].split(
            "continue;", 1
        )[0]

        self.assertIn("ui->camera =", scan_ready)
        self.assertIn("PAGE_W - 2 * COMPACT_PAD, 244", scan_ready)
        self.assertIn("stc_camera_capture_frame(rgb, 1000) == STC_OK",
                      idle_preview)
        self.assertIn("stc_shell_ui_set_camera_rgb160", idle_preview)


if __name__ == "__main__":
    unittest.main()
