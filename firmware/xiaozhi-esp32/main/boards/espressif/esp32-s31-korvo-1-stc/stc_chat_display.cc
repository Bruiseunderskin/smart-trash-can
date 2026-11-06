#include "stc_chat_display.h"

#include "stc_shell_ui.h"

#include <esp_log.h>

#include <utility>

namespace {

constexpr const char* TAG = "StcChatDisplay";

}  // namespace

StcChatDisplay::StcChatDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                               int width, int height, int offset_x, int offset_y, bool mirror_x,
                               bool mirror_y, bool swap_xy,
                               std::function<void(stc_mode_t)> request_mode)
    : RgbLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y,
                    swap_xy),
      request_mode_(std::move(request_mode)) {}

void StcChatDisplay::SetupUI() {
    if (IsSetupUICalled()) {
        return;
    }

    // The RGB panel uses direct-mode double buffering. Keep the normal display
    // initialization path so LVGL manages its framebuffers, then hide all
    // native XiaoZhi widgets before mounting the STC page above them.
    LcdDisplay::SetupUI();
    {
        DisplayLockGuard lock(this);
        if (!lock) {
            return;
        }

        lv_obj_t* screen = lv_screen_active();
        const uint32_t child_count = lv_obj_get_child_count(screen);
        for (uint32_t index = 0; index < child_count; ++index) {
            lv_obj_add_flag(lv_obj_get_child(screen, index), LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x07111F), 0);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(screen, 0, 0);
        lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    }

    const stc_err_t error = stc_shell_ui_init(this, std::move(request_mode_));
    if (error != STC_OK) {
        ESP_LOGE(TAG, "STC_CHAT_UI init failed=%d", static_cast<int>(error));
        return;
    }
    ESP_LOGI(TAG, "STC_CHAT_DISPLAY ready native_xiaozhi_ui=0");
}

void StcChatDisplay::SetStatus(const char* status) { (void)status; }

void StcChatDisplay::ShowNotification(const char* notification, int duration_ms) {
    (void)notification;
    (void)duration_ms;
}

void StcChatDisplay::SetEmotion(const char* emotion) { (void)emotion; }

void StcChatDisplay::SetChatMessage(const char* role, const char* content) {
    stc_shell_ui_set_chat_message(role, content);
}

void StcChatDisplay::ClearChatMessages() { stc_shell_ui_clear_chat_message(); }

void StcChatDisplay::SetTheme(Theme* theme) {
    // Persist and select the theme, but do not touch the native LcdDisplay
    // widget pointers because that screen is never created.
    Display::SetTheme(theme);
}

void StcChatDisplay::UpdateStatusBar(bool update_all) { (void)update_all; }

void StcChatDisplay::SetPowerSaveMode(bool on) { (void)on; }

void StcChatDisplay::SetHideSubtitle(bool hide) { (void)hide; }

void StcChatDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) { (void)image; }
