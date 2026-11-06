#include "stc_chat_display.h"

#include "stc_shell_ui.h"

#include <esp_log.h>

namespace {

constexpr const char* TAG = "StcChatDisplay";

}  // namespace

void StcChatDisplay::SetupUI() {
    if (IsSetupUICalled()) {
        return;
    }

    // Mark the display ready without calling LcdDisplay::SetupUI(), which
    // would create the native XiaoZhi page underneath our shell.
    Display::SetupUI();
    DisplayLockGuard lock(this);
    if (!lock) {
        return;
    }

    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x07111F), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
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
