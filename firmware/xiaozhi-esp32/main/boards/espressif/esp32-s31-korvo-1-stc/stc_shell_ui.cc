#include "stc_shell_ui.h"

#include "stc_game_placeholder.h"

#include <esp_log.h>
#include <lvgl.h>

#include <functional>
#include <utility>

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);

namespace {

constexpr const char* TAG = "StcShellUi";
constexpr int kNavHeight = 94;

struct ShellUi {
    Display* display = nullptr;
    std::function<void(stc_mode_t)> request_mode;
    lv_obj_t* root = nullptr;
    lv_obj_t* game_page = nullptr;
    lv_obj_t* chat_status = nullptr;
    lv_obj_t* game_button = nullptr;
    lv_obj_t* chat_button = nullptr;
    stc_app_state_t state = STC_APP_GAME;
};

ShellUi ui;

void StyleButton(lv_obj_t* button, const char* text) {
    lv_obj_set_size(button, LV_PCT(44), 64);
    lv_obj_set_style_radius(button, 14, 0);
    lv_obj_set_style_border_width(button, 0, 0);
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

void ApplyVisualStateLocked() {
    const bool chat = ui.state != STC_APP_GAME;
    if (chat) {
        lv_obj_add_flag(ui.game_page, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(ui.game_page, LV_OBJ_FLAG_HIDDEN);
    }

    if (ui.state == STC_APP_CHAT_STARTING) {
        lv_obj_clear_flag(ui.chat_status, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lv_obj_get_child(ui.chat_status, 0), "CHAT CONNECTING...");
    } else {
        lv_obj_add_flag(ui.chat_status, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_style_bg_color(ui.game_button, lv_color_hex(chat ? 0x263244 : 0x2563EB), 0);
    lv_obj_set_style_bg_color(ui.chat_button, lv_color_hex(chat ? 0x16A34A : 0x263244), 0);
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

    lv_obj_t* top = lv_layer_top();
    ui.root = lv_obj_create(top);
    lv_obj_set_size(ui.root, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(ui.root, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(ui.root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui.root, 0, 0);
    lv_obj_set_style_pad_all(ui.root, 0, 0);
    lv_obj_remove_flag(ui.root, LV_OBJ_FLAG_SCROLLABLE);

    ui.game_page = lv_obj_create(ui.root);
    lv_obj_set_size(ui.game_page, LV_HOR_RES, LV_VER_RES - kNavHeight);
    lv_obj_align(ui.game_page, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_flag(ui.game_page, LV_OBJ_FLAG_CLICKABLE);
    stc_game_placeholder_build(ui.game_page);

    ui.chat_status = lv_obj_create(ui.root);
    lv_obj_set_size(ui.chat_status, 280, 48);
    lv_obj_align(ui.chat_status, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_set_style_radius(ui.chat_status, 24, 0);
    lv_obj_set_style_bg_color(ui.chat_status, lv_color_hex(0x0F766E), 0);
    lv_obj_set_style_bg_opa(ui.chat_status, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui.chat_status, 0, 0);
    lv_obj_t* chat_label = lv_label_create(ui.chat_status);
    lv_obj_set_style_text_font(chat_label, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(chat_label, lv_color_white(), 0);
    lv_obj_center(chat_label);

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
    ApplyVisualStateLocked();
    ESP_LOGI(TAG, "STC_SHELL_UI ready game_button=1 chat_button=1");
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
    ui.state = state;
    ApplyVisualStateLocked();
}

bool stc_shell_ui_ready(void) { return ui.root != nullptr; }
