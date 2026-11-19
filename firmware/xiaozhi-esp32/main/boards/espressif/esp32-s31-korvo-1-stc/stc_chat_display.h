#pragma once

#include "display/lcd_display.h"
#include "stc_shell_mode.h"

#include <functional>

class StcChatDisplay : public RgbLcdDisplay {
public:
    StcChatDisplay(
        esp_lcd_panel_io_handle_t panel_io,
        esp_lcd_panel_handle_t panel,
        int width,
        int height,
        int offset_x,
        int offset_y,
        bool mirror_x,
        bool mirror_y,
        bool swap_xy,
        std::function<void(stc_shell_state_t)> request_mode);

    void SetupUI() override;
    void SetStatus(const char* status) override;
    void ShowNotification(const char* notification,
                          int duration_ms = 3000) override;
    void SetEmotion(const char* emotion) override;
    void SetChatMessage(const char* role, const char* content) override;
    void ClearChatMessages() override;
    void SetTheme(Theme* theme) override;
    void UpdateStatusBar(bool update_all = false) override;
    void SetPowerSaveMode(bool on) override;
    void SetHideSubtitle(bool hide) override;
    void SetPreviewImage(std::unique_ptr<LvglImage> image) override;

private:
    std::function<void(stc_shell_state_t)> request_mode_;
};
