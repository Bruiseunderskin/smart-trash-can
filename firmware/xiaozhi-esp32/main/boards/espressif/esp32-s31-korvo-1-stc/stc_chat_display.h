#pragma once

#include "display/lcd_display.h"

// Board-owned display shell. It intentionally does not build XiaoZhi's native
// screen; subtitle and status events are forwarded to the STC chat page.
class StcChatDisplay : public RgbLcdDisplay {
public:
    using RgbLcdDisplay::RgbLcdDisplay;

    void SetupUI() override;
    void SetStatus(const char* status) override;
    void ShowNotification(const char* notification, int duration_ms = 3000) override;
    void SetEmotion(const char* emotion) override;
    void SetChatMessage(const char* role, const char* content) override;
    void ClearChatMessages() override;
    void SetTheme(Theme* theme) override;
    void UpdateStatusBar(bool update_all = false) override;
    void SetPowerSaveMode(bool on) override;
    void SetHideSubtitle(bool hide) override;
    void SetPreviewImage(std::unique_ptr<LvglImage> image) override;
};
