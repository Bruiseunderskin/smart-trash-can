#include "stc_shell_ui.h"

#include "config.h"
#include "device_state_machine.h"
#include "lvgl_theme.h"
#include "stc_game_placeholder.h"

#include <esp_log.h>
#include <lvgl.h>

#include <cstdio>
#include <cstring>
#include <functional>
#include <utility>

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(font_noto_sans_basic_20_4);

namespace {

constexpr const char* TAG = "StcShellUi";
constexpr int kNavHeight = 94;
constexpr int kContentHeight = DISPLAY_HEIGHT - kNavHeight;
constexpr int kSubtitleScale = 276;  // 1.078x; slightly larger than the native subtitle.

struct ShellUi {
    Display* display = nullptr;
    std::function<void(stc_mode_t)> request_mode;
    lv_obj_t* root = nullptr;
    lv_obj_t* game_page = nullptr;
    lv_obj_t* chat_page = nullptr;
    lv_obj_t* connection_dot = nullptr;
    lv_obj_t* connection_label = nullptr;
    lv_obj_t* device_state_dot = nullptr;
    lv_obj_t* device_state_label = nullptr;
    lv_obj_t* speaker_label = nullptr;
    lv_obj_t* subtitle_label = nullptr;
    const lv_font_t* subtitle_font = nullptr;
    lv_obj_t* game_button = nullptr;
    lv_obj_t* chat_button = nullptr;
    stc_app_state_t state = STC_APP_GAME;
    char connection_text[32] = {};
    char device_state_text[64] = {};
};

ShellUi ui;

void SetTextIfChanged(lv_obj_t* label, const char* text) {
    if (label == nullptr || text == nullptr) {
        return;
    }
    const char* current = lv_label_get_text(label);
    if (current == nullptr || strcmp(current, text) != 0) {
        lv_label_set_text(label, text);
    }
}

const lv_font_t* CurrentTextFontLocked() {
    if (ui.display != nullptr) {
        auto* theme = static_cast<LvglTheme*>(ui.display->GetTheme());
        if (theme != nullptr && theme->text_font() != nullptr &&
            theme->text_font()->font() != nullptr) {
            return theme->text_font()->font();
        }
    }
    return &BUILTIN_TEXT_FONT;
}

void StyleButton(lv_obj_t* button, const char* text) {
    lv_obj_set_size(button, LV_PCT(44), 64);
    lv_obj_set_style_radius(button, 16, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0x31516B), 0);
    lv_obj_set_style_shadow_width(button, 0, 0);

    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
}

void RequestMode(stc_mode_t mode) {
    if (ui.request_mode) {
        ui.request_mode(mode);
    }
}

void GameButtonClicked(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        RequestMode(STC_MODE_GAME);
    }
}

void ChatButtonClicked(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        RequestMode(STC_MODE_CHAT);
    }
}

void BuildTitle(lv_obj_t* parent) {
    lv_obj_t* title_row = lv_obj_create(parent);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, 38);
    lv_obj_set_style_bg_opa(title_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_remove_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "小智语音");
    lv_obj_set_style_text_font(title, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xF8FAFC), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* mode_label = lv_label_create(title_row);
    lv_label_set_text(mode_label, "CHAT MODE");
    lv_obj_set_style_text_font(mode_label, &font_noto_sans_basic_20_4, 0);
    lv_obj_set_style_text_color(mode_label, lv_color_hex(0x67E8F9), 0);
    lv_obj_align(mode_label, LV_ALIGN_RIGHT_MID, 0, 0);
}

void BuildStatusPill(lv_obj_t* parent, lv_obj_t** dot, lv_obj_t** label, const char* initial_text) {
    lv_obj_t* pill = lv_obj_create(parent);
    lv_obj_set_height(pill, 48);
    lv_obj_set_flex_grow(pill, 1);
    lv_obj_set_style_radius(pill, 14, 0);
    lv_obj_set_style_bg_color(pill, lv_color_hex(0x0F1E33), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pill, 1, 0);
    lv_obj_set_style_border_color(pill, lv_color_hex(0x26445D), 0);
    lv_obj_set_style_pad_left(pill, 14, 0);
    lv_obj_set_style_pad_right(pill, 14, 0);
    lv_obj_set_style_pad_column(pill, 10, 0);
    lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(pill, LV_OBJ_FLAG_SCROLLABLE);

    *dot = lv_obj_create(pill);
    lv_obj_set_size(*dot, 12, 12);
    lv_obj_set_style_radius(*dot, 6, 0);
    lv_obj_set_style_bg_color(*dot, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_bg_opa(*dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(*dot, 0, 0);

    *label = lv_label_create(pill);
    lv_label_set_text(*label, initial_text);
    lv_obj_set_style_text_font(*label, &font_noto_sans_basic_20_4, 0);
    lv_obj_set_style_text_color(*label, lv_color_hex(0xD8E5F2), 0);
    lv_obj_set_flex_grow(*label, 1);
}

void BuildChatPage(lv_obj_t* parent) {
    ui.chat_page = lv_obj_create(parent);
    lv_obj_set_size(ui.chat_page, LV_HOR_RES, kContentHeight);
    lv_obj_align(ui.chat_page, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_radius(ui.chat_page, 0, 0);
    lv_obj_set_style_bg_color(ui.chat_page, lv_color_hex(0x07111F), 0);
    lv_obj_set_style_bg_opa(ui.chat_page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui.chat_page, 0, 0);
    lv_obj_set_style_pad_all(ui.chat_page, 18, 0);
    lv_obj_set_style_pad_row(ui.chat_page, 12, 0);
    lv_obj_set_flex_flow(ui.chat_page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui.chat_page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(ui.chat_page, LV_OBJ_FLAG_SCROLLABLE);

    BuildTitle(ui.chat_page);

    lv_obj_t* status_row = lv_obj_create(ui.chat_page);
    lv_obj_set_width(status_row, LV_PCT(100));
    lv_obj_set_height(status_row, 48);
    lv_obj_set_style_bg_opa(status_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_row, 0, 0);
    lv_obj_set_style_pad_all(status_row, 0, 0);
    lv_obj_set_style_pad_column(status_row, 12, 0);
    lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
    lv_obj_remove_flag(status_row, LV_OBJ_FLAG_SCROLLABLE);
    BuildStatusPill(status_row, &ui.connection_dot, &ui.connection_label, "网络未连接");
    BuildStatusPill(status_row, &ui.device_state_dot, &ui.device_state_label, "等待启动 · UNKNOWN");

    lv_obj_t* subtitle_card = lv_obj_create(ui.chat_page);
    lv_obj_set_width(subtitle_card, LV_PCT(100));
    lv_obj_set_flex_grow(subtitle_card, 1);
    lv_obj_set_style_radius(subtitle_card, 18, 0);
    lv_obj_set_style_bg_color(subtitle_card, lv_color_hex(0x0B192B), 0);
    lv_obj_set_style_bg_opa(subtitle_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(subtitle_card, 1, 0);
    lv_obj_set_style_border_color(subtitle_card, lv_color_hex(0x24445E), 0);
    lv_obj_set_style_pad_all(subtitle_card, 18, 0);
    lv_obj_set_style_pad_row(subtitle_card, 8, 0);
    lv_obj_set_flex_flow(subtitle_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(subtitle_card, LV_OBJ_FLAG_SCROLLABLE);

    ui.speaker_label = lv_label_create(subtitle_card);
    lv_label_set_text(ui.speaker_label, "小智");
    lv_obj_set_style_text_font(ui.speaker_label, &font_noto_sans_basic_20_4, 0);
    lv_obj_set_style_text_color(ui.speaker_label, lv_color_hex(0x67E8F9), 0);

    ui.subtitle_label = lv_label_create(subtitle_card);
    lv_label_set_text(ui.subtitle_label, "直接说话，我在听。");
    lv_obj_set_width(ui.subtitle_label, 660);
    lv_obj_set_height(ui.subtitle_label, LV_SIZE_CONTENT);
    lv_label_set_long_mode(ui.subtitle_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(ui.subtitle_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(ui.subtitle_label, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(ui.subtitle_label, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_text_line_space(ui.subtitle_label, 8, 0);
    lv_obj_set_style_text_letter_space(ui.subtitle_label, 1, 0);
    lv_obj_set_style_text_outline_stroke_width(ui.subtitle_label, 1, 0);
    lv_obj_set_style_text_outline_stroke_color(ui.subtitle_label, lv_color_hex(0xCBDAE9), 0);
    lv_obj_set_style_text_outline_stroke_opa(ui.subtitle_label, LV_OPA_COVER, 0);
    lv_obj_set_style_transform_scale(ui.subtitle_label, kSubtitleScale, 0);
    lv_obj_align(ui.subtitle_label, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* hint = lv_label_create(ui.chat_page);
    lv_label_set_text(hint, "直接说话即可，无需点击停止");
    lv_obj_set_style_text_font(hint, &font_noto_sans_basic_20_4, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x8FA7BC), 0);
}

void ApplyVisualStateLocked() {
    const bool chat = ui.state != STC_APP_GAME;
    if (chat) {
        lv_obj_add_flag(ui.game_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui.chat_page, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(ui.game_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui.chat_page, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_style_bg_color(ui.game_button, lv_color_hex(chat ? 0x263244 : 0x2563EB), 0);
    lv_obj_set_style_bg_color(ui.chat_button, lv_color_hex(chat ? 0x0F766E : 0x263244), 0);
}

const char* DeviceStateChinese(DeviceState state) {
    switch (state) {
        case kDeviceStateStarting:
            return "启动中";
        case kDeviceStateWifiConfiguring:
            return "配网中";
        case kDeviceStateIdle:
            return "待命";
        case kDeviceStateConnecting:
            return "连接中";
        case kDeviceStateListening:
            return "聆听中";
        case kDeviceStateSpeaking:
            return "回复中";
        case kDeviceStateNotifying:
            return "播报中";
        case kDeviceStateUpgrading:
            return "升级中";
        case kDeviceStateActivating:
            return "激活中";
        case kDeviceStateAudioTesting:
            return "音频测试";
        case kDeviceStateFatalError:
            return "故障";
        case kDeviceStateUnknown:
        default:
            return "未知";
    }
}

lv_color_t DeviceStateColor(DeviceState state) {
    switch (state) {
        case kDeviceStateListening:
            return lv_color_hex(0x2DD4BF);
        case kDeviceStateSpeaking:
            return lv_color_hex(0xF59E0B);
        case kDeviceStateConnecting:
        case kDeviceStateActivating:
            return lv_color_hex(0x38BDF8);
        case kDeviceStateFatalError:
            return lv_color_hex(0xEF4444);
        case kDeviceStateIdle:
            return lv_color_hex(0x94A3B8);
        default:
            return lv_color_hex(0xA78BFA);
    }
}

void ApplyRuntimeStatusLocked(stc_app_state_t state, DeviceState device_state,
                              bool network_connected) {
    const bool state_changed = ui.state != state;
    ui.state = state;
    if (state_changed) {
        ApplyVisualStateLocked();
    }

    const bool in_chat = state != STC_APP_GAME;
    const char* connection = "网络关闭";
    lv_color_t connection_color = lv_color_hex(0x64748B);
    if (in_chat && network_connected) {
        connection = "网络已连接";
        connection_color = lv_color_hex(0x22C55E);
    } else if (in_chat && state == STC_APP_CHAT_STARTING) {
        connection = "网络连接中";
        connection_color = lv_color_hex(0xF59E0B);
    } else if (in_chat) {
        connection = "网络已断开";
        connection_color = lv_color_hex(0xEF4444);
    }

    char state_text[64];
    snprintf(state_text, sizeof(state_text), "%s · %s", DeviceStateChinese(device_state),
             DeviceStateMachine::GetStateName(device_state));
    if (strcmp(ui.connection_text, connection) != 0) {
        snprintf(ui.connection_text, sizeof(ui.connection_text), "%s", connection);
        SetTextIfChanged(ui.connection_label, ui.connection_text);
        lv_obj_set_style_bg_color(ui.connection_dot, connection_color, 0);
        ESP_LOGI(TAG, "STC_CHAT_UI connection=%s", ui.connection_text);
    }
    if (strcmp(ui.device_state_text, state_text) != 0) {
        snprintf(ui.device_state_text, sizeof(ui.device_state_text), "%s", state_text);
        SetTextIfChanged(ui.device_state_label, ui.device_state_text);
        lv_obj_set_style_bg_color(ui.device_state_dot, DeviceStateColor(device_state), 0);
        ESP_LOGI(TAG, "STC_CHAT_UI device_state=%s", ui.device_state_text);
    }
}

void SetSubtitleLocked(const char* role, const char* content) {
    if (ui.subtitle_label == nullptr || ui.speaker_label == nullptr) {
        return;
    }

    const bool empty = content == nullptr || content[0] == '\0';
    const char* normalized_role = role != nullptr ? role : "assistant";
    const char* speaker = "小智";
    const char* fallback = "直接说话，我在听。";
    lv_color_t role_color = lv_color_hex(0x2DD4BF);
    lv_color_t text_color = lv_color_hex(0xF8FAFC);

    if (strcmp(normalized_role, "user") == 0) {
        speaker = "你";
        role_color = lv_color_hex(0x38BDF8);
        fallback = "没有听清，请再说一次。";
    } else if (strcmp(normalized_role, "system") == 0) {
        speaker = "系统";
        role_color = lv_color_hex(0xA78BFA);
        fallback = "小智正在准备语音服务。";
    }
    if (empty) {
        text_color = lv_color_hex(0x9FB4C8);
    }

    SetTextIfChanged(ui.speaker_label, speaker);
    SetTextIfChanged(ui.subtitle_label, empty ? fallback : content);
    lv_obj_set_style_text_color(ui.speaker_label, role_color, 0);
    lv_obj_set_style_text_color(ui.subtitle_label, text_color, 0);

    const lv_font_t* font = CurrentTextFontLocked();
    if (font != nullptr && font != ui.subtitle_font) {
        ui.subtitle_font = font;
        lv_obj_set_style_text_font(ui.subtitle_label, font, 0);
    }

    ESP_LOGI(TAG, "STC_CHAT_UI message role=%s bytes=%u", normalized_role,
             static_cast<unsigned>(empty ? 0 : strlen(content)));
}

}  // namespace

stc_err_t stc_shell_ui_init(Display* display, std::function<void(stc_mode_t)> request_mode) {
    if (display == nullptr) {
        return STC_ERR_IO;
    }
    if (ui.root != nullptr) {
        ui.request_mode = std::move(request_mode);
        return STC_OK;
    }

    DisplayLockGuard lock(display);
    if (!lock) {
        return STC_ERR_TIMEOUT;
    }

    ui.display = display;
    ui.request_mode = std::move(request_mode);

    lv_obj_t* top = lv_screen_active();
    ui.root = lv_obj_create(top);
    lv_obj_set_size(ui.root, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(ui.root, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(ui.root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui.root, 0, 0);
    lv_obj_set_style_pad_all(ui.root, 0, 0);
    lv_obj_remove_flag(ui.root, LV_OBJ_FLAG_SCROLLABLE);

    ui.game_page = lv_obj_create(ui.root);
    lv_obj_set_size(ui.game_page, LV_HOR_RES, kContentHeight);
    lv_obj_align(ui.game_page, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_flag(ui.game_page, LV_OBJ_FLAG_CLICKABLE);
    stc_game_placeholder_build(ui.game_page);

    BuildChatPage(ui.root);

    lv_obj_t* nav = lv_obj_create(ui.root);
    lv_obj_set_size(nav, LV_HOR_RES, kNavHeight);
    lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(nav, 0, 0);
    lv_obj_set_style_bg_color(nav, lv_color_hex(0x0B1220), 0);
    lv_obj_set_style_bg_opa(nav, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(nav, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(nav, 2, 0);
    lv_obj_set_style_border_color(nav, lv_color_hex(0x273449), 0);
    lv_obj_set_style_pad_all(nav, 12, 0);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(nav, LV_OBJ_FLAG_SCROLLABLE);

    ui.game_button = lv_button_create(nav);
    StyleButton(ui.game_button, "GAME");
    lv_obj_add_event_cb(ui.game_button, GameButtonClicked, LV_EVENT_CLICKED, nullptr);

    ui.chat_button = lv_button_create(nav);
    StyleButton(ui.chat_button, "CHAT");
    lv_obj_add_event_cb(ui.chat_button, ChatButtonClicked, LV_EVENT_CLICKED, nullptr);

    ui.state = STC_APP_GAME;
    SetSubtitleLocked("assistant", "");
    ApplyVisualStateLocked();
    ESP_LOGI(TAG, "STC_SHELL_UI ready game_button=1 chat_button=1 chat_page=1");
    return STC_OK;
}

void stc_shell_ui_set_state(stc_app_state_t state) {
    if (ui.display == nullptr) {
        return;
    }
    DisplayLockGuard lock(ui.display);
    if (!lock) {
        return;
    }
    const bool changed = ui.state != state;
    ui.state = state;
    if (changed) {
        ApplyVisualStateLocked();
    }
}

void stc_shell_ui_refresh_status(stc_app_state_t state, DeviceState device_state,
                                 bool network_connected) {
    if (ui.display == nullptr) {
        return;
    }
    DisplayLockGuard lock(ui.display);
    if (!lock) {
        return;
    }
    ApplyRuntimeStatusLocked(state, device_state, network_connected);
}

void stc_shell_ui_set_chat_message(const char* role, const char* content) {
    if (ui.display == nullptr) {
        return;
    }
    DisplayLockGuard lock(ui.display);
    if (!lock) {
        return;
    }
    SetSubtitleLocked(role, content);
}

void stc_shell_ui_clear_chat_message(void) { stc_shell_ui_set_chat_message("assistant", ""); }

bool stc_shell_ui_ready(void) { return ui.root != nullptr; }
