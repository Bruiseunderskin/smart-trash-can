from pathlib import Path
import json
import unittest


ROOT = Path(__file__).resolve().parents[2]
BOARD = (
    ROOT
    / "firmware/xiaozhi-esp32/main/boards/espressif/"
    "esp32-s31-korvo-1-stc"
)
MAIN_CMAKE = ROOT / "firmware/xiaozhi-esp32/main/CMakeLists.txt"


class StcFullGameIntegrationTest(unittest.TestCase):
    def test_shell_embeds_the_formal_full_screen_game_ui(self):
        shell = (BOARD / "stc_shell_ui.cc").read_text(encoding="utf-8")

        self.assertIn('#include "ui_bridge.h"', shell)
        self.assertIn("ui.game_page, LV_HOR_RES, LV_VER_RES", shell)
        self.assertIn(
            "stc_ui_bridge_create(ui.game_page, RetryGame, nullptr)",
            shell,
        )
        self.assertNotIn("stc_game_placeholder_build", shell)

    def test_board_runs_game_camera_and_espnow_runtime(self):
        board = (BOARD / "esp32_s31_korvo_1_stc.cc").read_text(
            encoding="utf-8"
        )

        self.assertIn('#include "stc_game_runtime.h"', board)
        self.assertIn("stc_game_runtime_start();", board)
        self.assertNotIn("InitializeCamera();", board)

    def test_game_sources_and_model_are_linked(self):
        cmake = MAIN_CMAKE.read_text(encoding="utf-8")

        for source in (
            "camera_capture.cpp",
            "replay_classifier.cpp",
            "espnow_controller.c",
            "stc_game_controller.c",
            "stc_hand_link.c",
            "stc_ui.c",
        ):
            self.assertIn(source, cmake)
        self.assertIn("wordless_s31.espdl", cmake)
        self.assertIn("game_replay_pack.bin", cmake)

    def test_cloud_gate_keeps_espnow_radio_alive(self):
        board = (BOARD / "esp32_s31_korvo_1_stc.cc").read_text(
            encoding="utf-8"
        )

        gate = board.split(
            'extern "C" void stc_network_gate_disable(void)', 1
        )[1].split("}", 1)[0]
        self.assertNotIn("StopStation", gate)
        self.assertIn("esp_wifi_set_channel", gate)

    def test_stc_manual_chat_disables_wakenet(self):
        voice = (BOARD / "stc_voice.cc").read_text(encoding="utf-8")
        audio_service = (
            ROOT / "firmware/xiaozhi-esp32/main/audio/audio_service.cc"
        ).read_text(encoding="utf-8")
        afe_engine = (
            ROOT
            / "firmware/xiaozhi-esp32/main/audio/engines/"
            "afe_audio_engine.cc"
        ).read_text(encoding="utf-8")

        self.assertIn("SetWakeWordAllowed(false);", voice)
        self.assertIn("wake_word_allowed_", audio_service)
        self.assertIn("wake_detector_ = WakeDetector::kNone;", afe_engine)

    def test_learning_next_is_explicit_and_not_a_sticky_start_latch(self):
        game_ui = (ROOT / "ui/lvglpro/src/stc_ui.c").read_text(
            encoding="utf-8"
        )
        game_header = (ROOT / "ui/lvglpro/include/stc_ui.h").read_text(
            encoding="utf-8"
        )
        bridge = (
            ROOT / "firmware/esp32s31/game/main/ui_bridge.c"
        ).read_text(encoding="utf-8")

        self.assertIn(
            'bind_action(ui, next, STC_ACTION_NEXT, -1, false);',
            game_ui,
        )
        self.assertIn("STC_ACTION_NEXT = 7", game_header)
        self.assertNotIn("pending_start", game_ui)
        self.assertNotIn("STC_BRIDGE_ACTION_NEXT", bridge)

    def test_chat_subtitle_window_is_multi_line_and_scrollable(self):
        chat_ui = (ROOT / "ui/lvglpro/src/stc_chat_ui.c").read_text(
            encoding="utf-8"
        )

        self.assertIn("chat_box(ui->container, 24, 190, 632, LV_SIZE_CONTENT", chat_ui)
        self.assertIn("lv_obj_set_style_max_height(speech, 168, 0);", chat_ui)
        self.assertIn("LV_FLEX_FLOW_COLUMN", chat_ui)
        self.assertIn("560, LV_SIZE_CONTENT", chat_ui)
        self.assertIn("LV_OBJ_FLAG_SCROLLABLE", chat_ui)
        self.assertIn(
            "lv_obj_scroll_to_view(role_label, LV_ANIM_OFF);",
            chat_ui,
        )

    def test_streamed_assistant_subtitles_accumulate_instead_of_overwrite(self):
        shell = (BOARD / "stc_shell_ui.cc").read_text(encoding="utf-8")

        self.assertIn("char chat_message[2048]", shell)
        self.assertNotIn(
            "ui.chat_state == STC_CHAT_ANSWERING && text[0]",
            shell,
        )
        self.assertIn('"\\n%s", text', shell)
        self.assertIn("text[0] == '%'", shell)
        self.assertIn("ui.chat_message[0] = '\\0';", shell)

    def test_chat_screens_always_expose_a_home_action(self):
        chat_ui = (ROOT / "ui/lvglpro/src/stc_chat_ui.c").read_text(
            encoding="utf-8"
        )
        shell = (BOARD / "stc_shell_ui.cc").read_text(encoding="utf-8")
        voice = (BOARD / "stc_voice.cc").read_text(encoding="utf-8")

        self.assertEqual(chat_ui.count('"返回主頁"'), 8)
        both_actions = chat_ui.split(
            "} else if (secondary != NULL) {", 1
        )[1].split("} else {", 1)[0]
        self.assertIn(
            "bind_chat_action(ui, back, STC_CHAT_ACTION_BACK);",
            both_actions,
        )
        self.assertIn("stc_voice_cancel_dialog();", shell)
        self.assertIn("stc_voice_restart_dialog();", shell)
        self.assertIn("ScheduleChatCancel();", voice)
        self.assertIn("STC_VOICE_DIALOG cancel", voice)

    def test_game_mode_closes_protocol_before_stopping_wifi(self):
        voice = (BOARD / "stc_voice.cc").read_text(encoding="utf-8")

        protocol = voice.index("app.ResetProtocol();")
        network_stop = voice.index("stc_network_gate_disable();", protocol)
        self.assertLess(protocol, network_stop)
        self.assertIn("audio.ResetDecoder();", voice)
        self.assertIn("audio.EnableVoiceProcessing(false);", voice)

    def test_real_hand_bin_choice_has_no_tap_to_pick(self):
        game_ui = (ROOT / "ui/lvglpro/src/stc_ui.c").read_text(
            encoding="utf-8"
        )
        board = (BOARD / "esp32_s31_korvo_1_stc.cc").read_text(
            encoding="utf-8"
        )
        choice = game_ui.split(
            "static void bin_choice_screen", 1
        )[1].split("static void error_screen", 1)[0]

        self.assertNotIn("bind_action", choice)
        self.assertNotIn("STC_ACTION_SELECT_BIN", choice)
        self.assertNotIn("LV_OBJ_FLAG_CLICKABLE", choice)
        self.assertIn('"stc bin "', board)
        self.assertIn('"stc mock "', board)

    def test_round_is_rendered_once_and_summary_badge_has_room(self):
        game_ui = (ROOT / "ui/lvglpro/src/stc_ui.c").read_text(
            encoding="utf-8"
        )
        app = (ROOT / "app/stc_app.c").read_text(encoding="utf-8")

        self.assertIn("snap->round + 1u", app)
        self.assertNotIn("ui->round + 1u", game_ui)
        self.assertIn(
            'label(score_card, "分類小達人", 36, 180, 196, 40',
            game_ui,
        )

    def test_button_labels_are_vertically_centered(self):
        game_ui = (ROOT / "ui/lvglpro/src/stc_ui.c").read_text(
            encoding="utf-8"
        )
        chat_ui = (ROOT / "ui/lvglpro/src/stc_chat_ui.c").read_text(
            encoding="utf-8"
        )

        self.assertIn("lv_obj_set_height(text_label, LV_SIZE_CONTENT);",
                      game_ui)
        self.assertIn("lv_obj_set_height(label, LV_SIZE_CONTENT);", chat_ui)

    def test_recreated_game_ui_can_restore_summary_after_chat(self):
        game_ui = (ROOT / "ui/lvglpro/src/stc_ui.c").read_text(
            encoding="utf-8"
        )

        self.assertIn("next == STC_SUMMARY", game_ui)

    def test_dynamic_chat_text_uses_common_theme_font(self):
        shell = (BOARD / "stc_shell_ui.cc").read_text(encoding="utf-8")
        chat_header = (
            ROOT / "ui/lvglpro/include/stc_chat_ui.h"
        ).read_text(encoding="utf-8")
        charset_path = (
            ROOT
            / "firmware/xiaozhi-esp32/managed_components/"
            "78__xiaozhi-fonts/charsets/common.json"
        )
        if not charset_path.exists():
            self.skipTest("xiaozhi-fonts managed component is not vendored here")
        charset = json.loads(charset_path.read_text(encoding="utf-8"))
        sample = "香港多雲天氣氣溫東北風濕度體感蠻舒服喔"

        self.assertIn("GetTextFont()", shell)
        self.assertIn("stc_chat_ui_set_message_font", shell)
        self.assertIn("stc_chat_ui_set_message_font", chat_header)
        available = set(charset["codepoints"])
        missing = [ch for ch in sample if ord(ch) not in available]
        self.assertEqual(missing, [])


if __name__ == "__main__":
    unittest.main()
