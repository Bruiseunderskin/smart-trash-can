from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]
BOARD_MAIN = (
    ROOT
    / "firmware/xiaozhi-esp32/main/boards/espressif/"
    "esp32-s31-korvo-1-stc/esp32_s31_korvo_1_stc.cc"
)


class StcChatIdlePolicyTest(unittest.TestCase):
    def test_voice_detection_refreshes_activity_only_on_rising_edge(self):
        source = BOARD_MAIN.read_text(encoding="utf-8")

        self.assertRegex(
            source,
            re.compile(
                r"voice_started\s*=\s*"
                r"voice_detected && !last_voice_detected"
            ),
        )
        self.assertIn(
            "device_state != last_device_state || active_state ||\n"
            "                voice_started || playback_active",
            source,
        )
        self.assertNotIn(
            "device_state != last_device_state || active_state ||\n"
            "                voice_detected || playback_active",
            source,
        )


if __name__ == "__main__":
    unittest.main()
