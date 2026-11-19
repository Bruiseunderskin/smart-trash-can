from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
BOARD = (
    ROOT
    / "firmware/xiaozhi-esp32/main/boards/espressif/"
    "esp32-s31-korvo-1-stc"
)


class StcChatFontPolicyTest(unittest.TestCase):
    def test_chat_and_game_text_use_firmware_owned_fonts(self):
        shell_ui = (BOARD / "stc_shell_ui.cc").read_text(encoding="utf-8")
        chat_ui = (ROOT / "ui/lvglpro/src/stc_chat_ui.c").read_text(
            encoding="utf-8")
        game_ui = (ROOT / "ui/lvglpro/src/stc_ui.c").read_text(
            encoding="utf-8")

        self.assertNotIn("CurrentTextFontLocked", shell_ui)
        self.assertIn('"stc_chat_ui.h"', shell_ui)
        self.assertIn("stc_chat_ui_create", shell_ui)
        for source in (chat_ui, game_ui):
            self.assertIn('"stc_lvglpro_assets.h"', source)
            self.assertIn("stc_lp_font_", source)


if __name__ == "__main__":
    unittest.main()
