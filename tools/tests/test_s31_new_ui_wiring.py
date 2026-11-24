"""Standalone S31 wiring for the v3 UI and the preserved S3 hand result."""

import unittest
from pathlib import Path

from PIL import Image


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
        idle_preview = runtime.split("if (!has_request)", 1)[1]

        self.assertIn("camera_frame(ui, ui->container);", scan_ready)
        self.assertIn('"確認卡片\\n完整入框"', scan_ready)
        self.assertIn('"5 秒後自動掃描"', scan_ready)
        self.assertIn("stc_camera_capture_preview(", idle_preview)
        self.assertIn("stc_shell_ui_set_camera_rgb", idle_preview)

    def test_live_preview_uses_320px_and_keeps_the_sensor_stream_paced(self):
        runtime = (
            ROOT
            / "firmware/xiaozhi-esp32/main/boards/espressif/"
            "esp32-s31-korvo-1-stc/stc_game_runtime.cc"
        ).read_text(encoding="utf-8")
        capture = (
            ROOT / "firmware/esp32s31/game/main/camera_capture.cpp"
        ).read_text(encoding="utf-8")
        shell = (
            ROOT
            / "firmware/xiaozhi-esp32/main/boards/espressif/"
            "esp32-s31-korvo-1-stc/stc_shell_ui.cc"
        ).read_text(encoding="utf-8")
        game_ui = (ROOT / "ui/lvglpro/src/stc_ui.c").read_text(
            encoding="utf-8"
        )

        self.assertIn("constexpr int kCameraPreviewSize = 240;", runtime)
        self.assertIn("stc_camera_capture_preview(", runtime)
        self.assertIn("LIVE_CAMERA_STATS", runtime)
        self.assertNotIn("pdMS_TO_TICKS(80)", runtime)
        self.assertIn("stc_preprocess_uyvy_to_rgb_preview", capture)
        self.assertIn("constexpr uint16_t kCameraPreviewSize = 240;", shell)
        self.assertIn("lv_image_set_src(ui->camera_image, source);", game_ui)

    def test_s3_enters_a_real_idle_state_during_chat(self):
        protocol = (ROOT / "firmware/common/stc_hand_msg.h").read_text(
            encoding="utf-8"
        )
        s3 = (
            ROOT / "firmware/esp32s3/game_hand/main/app_main.c"
        ).read_text(encoding="utf-8")
        tof = (
            ROOT / "firmware/esp32s3/game_hand/main/tof4.c"
        ).read_text(encoding="utf-8")
        runtime = (
            ROOT
            / "firmware/xiaozhi-esp32/main/boards/espressif/"
            "esp32-s31-korvo-1-stc/stc_game_runtime.cc"
        ).read_text(encoding="utf-8")

        self.assertIn("#define STC_HAND_MSG_VERSION 4u", protocol)
        self.assertIn("STC_HAND_ACTION_PAUSE", protocol)
        self.assertIn("STC_HAND_ACTION_RESUME", protocol)
        self.assertIn("esp_err_t stc_tof4_stop_continuous(void);",
                      (ROOT / "firmware/esp32s3/game_hand/main/tof4.h")
                      .read_text(encoding="utf-8"))
        self.assertIn("stc_tof4_stop_continuous()", s3)
        self.assertIn('"RX_PAUSE', s3)
        self.assertIn('"RX_RESUME', s3)
        self.assertIn("if (s_paused)", s3)
        self.assertIn("vTaskDelay(pdMS_TO_TICKS(20));\n            continue;",
                      s3)
        self.assertIn("REG_RANGE_START, 0x01", tof)
        self.assertIn("SendHandControl(STC_HAND_ACTION_PAUSE)", runtime)
        self.assertIn("SendHandControl(STC_HAND_ACTION_RESUME)", runtime)

    def test_servo_pwm_restarts_after_pause_stop(self):
        servo = (
            ROOT / "firmware/esp32s3/game_hand/main/servo_lids.c"
        ).read_text(encoding="utf-8")

        self.assertIn("static bool s_channels_stopped = true;", servo)
        self.assertIn("if (s_channels_stopped)", servo)
        self.assertIn("stc_servo_lids_init(), TAG", servo)
        self.assertIn("s_channels_stopped = true;", servo)
        self.assertGreaterEqual(servo.count("vTaskDelay(pdMS_TO_TICKS(SERVO_STAGGER_MS));"), 2)
        self.assertIn("SERVO_STAGGER_MS", servo)

    def test_manual_chat_exit_waits_for_activation_to_finish(self):
        board = (
            ROOT
            / "firmware/xiaozhi-esp32/main/boards/espressif/"
            "esp32-s31-korvo-1-stc/esp32_s31_korvo_1_stc.cc"
        ).read_text(encoding="utf-8")

        self.assertIn("bool game_exit_pending = false;", board)
        self.assertIn("request_state == kDeviceStateActivating", board)
        self.assertIn("request_state == kDeviceStateConnecting", board)
        self.assertIn("device_state == kDeviceStateIdle", board)
        self.assertIn("10000u", board)

    def test_guide_and_classifier_use_the_same_square_crop(self):
        game_ui = (ROOT / "ui/lvglpro/src/stc_ui.c").read_text(
            encoding="utf-8"
        )
        preprocess = (
            ROOT / "firmware/common/stc_preprocess.c"
        ).read_text(encoding="utf-8")
        camera_card = game_ui.split("static void camera_card", 1)[
            1
        ].split("static void home_screen", 1)[0]
        scan_ready = game_ui.split(
            "static void scan_ready_screen", 1
        )[1].split("static void capturing_screen", 1)[0]

        self.assertIn("camera_frame(ui, ui->container);", camera_card)
        self.assertIn("camera_frame(ui, ui->container);", scan_ready)
        self.assertIn('"確認卡片\\n完整入框"', scan_ready)
        self.assertIn("#define CAMERA_FRAME_SIZE 320", game_ui)
        self.assertIn(
            "ui->camera = box(parent, OUTER, 112, CAMERA_FRAME_SIZE,\n"
            "                     CAMERA_FRAME_SIZE, COLOR_BRAND, 12, 3,",
            game_ui,
        )
        self.assertIn("COLOR_ACCENT);", game_ui)
        self.assertIn("lv_obj_set_style_border_width(ui->camera, 8, 0);",
                      game_ui)
        self.assertIn("#define CAMERA_IMAGE_INSET 12", game_ui)
        self.assertIn(
            "lv_obj_get_width(ui->camera) - 2 * CAMERA_IMAGE_INSET",
            game_ui,
        )
        self.assertIn(
            "label(ui->container, title, CAMERA_COPY_X, CAMERA_COPY_TITLE_Y,\n"
            "          CAMERA_COPY_W, CAMERA_COPY_TITLE_H",
            camera_card,
        )
        self.assertIn("LV_TEXT_ALIGN_CENTER);", camera_card)
        self.assertIn("const int side = (width < height) ? width : height;",
                      preprocess)
        self.assertIn("const int ox = (width - side) / 2;", preprocess)
        self.assertIn("const int oy = (height - side) / 2;", preprocess)

    def test_classifying_freezes_the_first_inference_frame(self):
        runtime = (
            ROOT
            / "firmware/xiaozhi-esp32/main/boards/espressif/"
            "esp32-s31-korvo-1-stc/stc_game_runtime.cc"
        ).read_text(encoding="utf-8")
        standalone = (
            ROOT / "firmware/esp32s31/game/main/app_main.cpp"
        ).read_text(encoding="utf-8")

        for source in (runtime, standalone):
            self.assertIn("bool still_shown = false;", source)
            self.assertIn("if (!still_shown) {", source)
            self.assertIn("still_shown = true;", source)

    def test_round_bar_is_time_based_and_scan_waits_five_seconds(self):
        game_ui = (ROOT / "ui/lvglpro/src/stc_ui.c").read_text(
            encoding="utf-8"
        )
        app = (ROOT / "app/stc_app.c").read_text(encoding="utf-8")

        self.assertIn('"5 秒後自動掃描"', game_ui)
        self.assertIn(
            "auto_advance(ui, STC_ACTION_SCAN, true, 50);", game_ui
        )
        self.assertIn(
            "ui->time_left_ms > 100 ? ui->time_left_ms - 100 : 0;",
            game_ui,
        )
        self.assertIn("out->time_left_ms", app)
        self.assertIn("out->time_total_ms", app)

    def test_scan_titles_and_countdown_share_one_centred_anchor(self):
        game_ui = (ROOT / "ui/lvglpro/src/stc_ui.c").read_text(
            encoding="utf-8"
        )
        camera_card = game_ui.split("static void camera_card", 1)[
            1
        ].split("static void home_screen", 1)[0]
        scan_ready = game_ui.split(
            "static void scan_ready_screen", 1
        )[1].split("static void capturing_screen", 1)[0]

        for define in (
            "#define CAMERA_COPY_X 352",
            "#define CAMERA_COPY_W 424",
            "#define CAMERA_COPY_TITLE_Y 128",
            "#define CAMERA_COPY_TITLE_H 96",
            "#define CAMERA_COPY_SUB_Y 240",
            "#define CAMERA_COPY_SUB_H 80",
        ):
            self.assertIn(define, game_ui)
        self.assertIn("CAMERA_COPY_X, CAMERA_COPY_TITLE_Y", camera_card)
        self.assertIn(
            '"確認卡片\\n完整入框", CAMERA_COPY_X,', scan_ready
        )
        self.assertIn('"5 秒後自動掃描", CAMERA_COPY_X,', scan_ready)

    def test_ai_answer_is_hidden_until_after_the_drop(self):
        game_ui = (ROOT / "ui/lvglpro/src/stc_ui.c").read_text(
            encoding="utf-8"
        )
        bin_card = game_ui.split("static void bin_card", 1)[1].split(
            "static void open_bins_screen", 1
        )[0]
        result_screen = game_ui.split("static void result_screen", 1)[
            1
        ].split("static void bin_choice_screen", 1)[0]
        bin_choice = game_ui.split("static void bin_choice_screen", 1)[
            1
        ].split("static void error_screen", 1)[0]

        self.assertNotIn("bin_examples", bin_card)
        self.assertNotIn("bin_examples", bin_choice)
        self.assertNotIn("selected", bin_card)
        self.assertNotIn("i == category", result_screen)
        self.assertNotIn("open ? 4 : 3", bin_choice)
        self.assertNotIn("shadow_width", bin_choice)

    def test_open_lid_and_choose_bin_share_the_same_layout(self):
        game_ui = (ROOT / "ui/lvglpro/src/stc_ui.c").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            'open_bins_screen(ui, "正在開啟桶蓋");', game_ui
        )
        self.assertIn(
            'open_bins_screen(ui, "請將卡牌放入垃圾桶");', game_ui
        )

        baseline = ROOT / "tools/lvglpro_preview/baselines"
        with Image.open(baseline / "result_ok.png") as result:
            with Image.open(baseline / "bin_choice.png") as choice:
                self.assertEqual(
                    result.crop((0, 0, 800, 112)).tobytes(),
                    choice.crop((0, 0, 800, 112)).tobytes(),
                )
                self.assertEqual(
                    result.crop((0, 216, 800, 480)).tobytes(),
                    choice.crop((0, 216, 800, 480)).tobytes(),
                )

    def test_scan_ready_bean_hold_cancels_round_safely(self):
        ui = (ROOT / "ui/lvglpro/src/stc_ui.c").read_text(encoding="utf-8")
        runtime = (
            ROOT
            / "firmware/xiaozhi-esp32/main/boards/espressif/"
            "esp32-s31-korvo-1-stc/stc_game_runtime.cc"
        ).read_text(encoding="utf-8")
        standalone = (
            GAME / "main/app_main.cpp"
        ).read_text(encoding="utf-8")
        contract = (ROOT / "contracts/stc_ui.h").read_text(encoding="utf-8")

        scan_ready = ui.split(
            "static void scan_ready_screen", 1
        )[1].split("static void capturing_screen", 1)[0]
        self.assertIn("bean_control(ui, BEAN_X, BEAN_Y, BEAN_SIZE);",
                      scan_ready)
        self.assertIn("static lv_obj_t *bean_control", ui)
        self.assertIn("lv_timer_create(logout_timer_cb, 100, ui)", ui)
        self.assertIn("ui->logout_ticks = 12;", ui)
        self.assertIn("STC_ACTION_CANCEL_ROUND", ui)
        self.assertIn("STC_UI_ACTION_CANCEL_ROUND", contract)

        for source in (runtime, standalone):
            self.assertIn("STC_UI_ACTION_CANCEL_ROUND", source)
            self.assertIn("ROUND_CANCEL", source)
            self.assertIn("stc_voice_stop();", source)
            self.assertIn("STC_HAND_ACTION_CLOSE_ALL", source)


if __name__ == "__main__":
    unittest.main()
