#include "stc_shell_ui.h"

#include "stc_app.h"
#include "stc_preprocess.h"
#include "config.h"
#include "stc_chat_ui.h"
#include "stc_ui.h"
#include "stc_voice.h"
#include "ui_bridge.h"
#include "lvgl_font.h"
#include "wifi_manager.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <lvgl.h>

#include <cstdio>
#include <cstring>
#include <functional>
#include <utility>

namespace {

constexpr const char* TAG = "StcShellUi";

struct ShellUi {
    Display* display = nullptr;
    std::function<void(stc_shell_state_t)> request_mode;
    std::function<void()> request_dialog;
    lv_obj_t* root = nullptr;
    lv_obj_t* game_page = nullptr;
    void* game_ui = nullptr;
    lv_obj_t* chat_page = nullptr;
    stc_chat_ui_t* chat_ui = nullptr;
    stc_ui_action_cb game_action_callback = nullptr;
    stc_shell_state_t state = STC_SHELL_GAME;
    char chat_role[16] = {};
    char chat_message[2048] = {};
    char applied_chat_message[2048] = {};
    stc_chat_state_t chat_state = STC_CHAT_READY;
    stc_chat_state_t applied_chat_state = STC_CHAT_READY;
    bool chat_applied = false;
    bool network_connected = false;
    bool goodbye_pending = false;
    bool goodbye_reply_seen = false;
    bool goodbye_exit_requested = false;
    int64_t goodbye_started_us = 0;
    int64_t goodbye_display_due_us = 0;
};

ShellUi ui;
constexpr uint16_t kCameraPreviewSize = 240;
uint16_t* preview_pixels[2] = {};
int preview_index = -1;
lv_image_dsc_t preview_desc[2] = {
    {
        .header = {
            .magic = LV_IMAGE_HEADER_MAGIC,
            .cf = LV_COLOR_FORMAT_RGB565,
            .w = kCameraPreviewSize,
            .h = kCameraPreviewSize,
            .stride = kCameraPreviewSize * 2,
        },
        .data_size = kCameraPreviewSize * kCameraPreviewSize *
                     sizeof(uint16_t),
        .data = nullptr,
    },
    {
        .header = {
            .magic = LV_IMAGE_HEADER_MAGIC,
            .cf = LV_COLOR_FORMAT_RGB565,
            .w = kCameraPreviewSize,
            .h = kCameraPreviewSize,
            .stride = kCameraPreviewSize * 2,
        },
        .data_size = kCameraPreviewSize * kCameraPreviewSize *
                     sizeof(uint16_t),
        .data = nullptr,
    },
};

bool EnsurePreviewBuffers() {
    if (preview_pixels[0] != nullptr) {
        return true;
    }
    constexpr size_t kBufferBytes =
        kCameraPreviewSize * kCameraPreviewSize * sizeof(uint16_t);
    auto* pixels = static_cast<uint16_t*>(heap_caps_calloc(
        2, kBufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (pixels == nullptr) {
        ESP_LOGE(TAG, "STC_PREVIEW alloc PSRAM failed bytes=%u",
                 static_cast<unsigned>(kBufferBytes * 2u));
        return false;
    }
    preview_pixels[0] = pixels;
    preview_pixels[1] =
        pixels + kCameraPreviewSize * kCameraPreviewSize;
    preview_desc[0].data =
        reinterpret_cast<const uint8_t*>(preview_pixels[0]);
    preview_desc[1].data =
        reinterpret_cast<const uint8_t*>(preview_pixels[1]);
    ESP_LOGI(TAG, "STC_PREVIEW buffers=PSRAM bytes=%u",
             static_cast<unsigned>(kBufferBytes * 2u));
    return true;
}

void BridgeGameAction(int32_t action, int32_t category_id, void* user_data);
void RetryGame(void* user_data);

void RequestMode(stc_shell_state_t state) {
    if (ui.request_mode) {
        ui.request_mode(state);
    }
}

void DispatchGameAction(stc_ui_action_t action, int32_t category_id) {
    if (ui.game_action_callback) {
        ui.game_action_callback(action, category_id, nullptr);
    }
}

void ChatAction(stc_chat_action_t action, void* user_data) {
    (void)user_data;
    if (action == STC_CHAT_ACTION_BACK) {
        stc_voice_cancel_dialog();
        RequestMode(STC_SHELL_GAME);
        return;
    }
    if (ui.chat_state == STC_CHAT_LISTENING ||
        ui.chat_state == STC_CHAT_THINKING) {
        stc_voice_cancel_dialog();
        return;
    }
    if (ui.chat_state == STC_CHAT_ANSWERING) {
        stc_voice_restart_dialog();
        return;
    }
    if (ui.request_dialog) {
        ui.request_dialog();
    } else {
        RequestMode(STC_SHELL_CHAT_STARTING);
    }
}

void RefreshChatLocked() {
    if (ui.chat_ui == nullptr) {
        return;
    }
    char runtime_message[256] = {};
    const char* message =
        ui.chat_message[0] != '\0' ? ui.chat_message : nullptr;
    if (ui.chat_state == STC_CHAT_PROVISIONING) {
        auto& wifi = WifiManager::GetInstance();
        if (wifi.IsConfigMode()) {
            snprintf(runtime_message, sizeof(runtime_message),
                     "連接 Wi-Fi: %s\n再開啟 %s",
                     wifi.GetApSsid().c_str(),
                     wifi.GetApWebUrl().c_str());
            message = runtime_message;
        }
    }
    const char* compare_message = message != nullptr ? message : "";
    if (ui.chat_applied &&
        ui.applied_chat_state == ui.chat_state &&
        strcmp(ui.applied_chat_message, compare_message) == 0) {
        return;
    }
    stc_chat_payload_t payload = {
        .state = ui.chat_state,
        .heading = nullptr,
        .message = message,
        .primary_label = nullptr,
        .secondary_label = nullptr,
    };
    if (stc_chat_ui_apply(ui.chat_ui, &payload)) {
        ui.applied_chat_state = ui.chat_state;
        snprintf(ui.applied_chat_message,
                 sizeof(ui.applied_chat_message), "%s", compare_message);
        ui.chat_applied = true;
        ESP_LOGI(TAG, "STC_CHAT_UI_RENDER state=%d message_bytes=%u",
                 static_cast<int>(ui.chat_state),
                 static_cast<unsigned>(strlen(ui.chat_message)));
    }
}

void EnsureChatUiLocked() {
    if (ui.chat_ui != nullptr || ui.chat_page == nullptr) {
        return;
    }
    lv_obj_update_layout(ui.chat_page);
    ESP_LOGI(TAG, "STC_CHAT_UI host=%ldx%ld content=%ldx%ld",
             static_cast<long>(lv_obj_get_width(ui.chat_page)),
             static_cast<long>(lv_obj_get_height(ui.chat_page)),
             static_cast<long>(lv_obj_get_content_width(ui.chat_page)),
             static_cast<long>(lv_obj_get_content_height(ui.chat_page)));
    ui.chat_ui = stc_chat_ui_create(ui.chat_page, ChatAction, nullptr);
    if (ui.chat_ui == nullptr) {
        ESP_LOGE(TAG, "LVGL Pro chat UI could not attach");
    } else {
        ui.chat_applied = false;
    }
}

void UpdateChatMessageFontLocked() {
    if (ui.chat_ui == nullptr || ui.display == nullptr ||
        ui.display->GetTheme() == nullptr) {
        return;
    }
    auto font = ui.display->GetTheme()->GetTextFont();
    if (font == nullptr || font->font() == nullptr) {
        return;
    }
    stc_chat_ui_set_message_font(ui.chat_ui, font->font());
}

void DestroyGameUiLocked() {
    if (ui.game_ui == nullptr) {
        return;
    }
    stc_ui_bridge_destroy(ui.game_ui);
    ui.game_ui = nullptr;
}

void EnsureGameUiLocked() {
    if (ui.game_ui != nullptr || ui.game_page == nullptr) {
        return;
    }
    ui.game_ui = stc_ui_bridge_create(ui.game_page, RetryGame, nullptr);
    if (ui.game_ui == nullptr) {
        ESP_LOGE(TAG, "formal game UI could not attach");
        return;
    }
    if (ui.game_action_callback != nullptr) {
        stc_ui_bridge_set_action_callback(
            ui.game_ui, BridgeGameAction);
    }
}

void BridgeGameAction(int32_t action, int32_t category_id, void* user_data) {
    (void)user_data;
    DispatchGameAction(static_cast<stc_ui_action_t>(action), category_id);
}

void RetryGame(void* user_data) {
    (void)user_data;
    DispatchGameAction(STC_UI_ACTION_HOME, -1);
}

void BuildChatPage(lv_obj_t* parent) {
    ui.chat_page = lv_obj_create(parent);
    lv_obj_remove_style_all(ui.chat_page);
    lv_obj_set_size(ui.chat_page, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(ui.chat_page, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(ui.chat_page, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(ui.chat_page, LV_OBJ_FLAG_SCROLLABLE);
}

void ApplyVisualStateLocked() {
    if (ui.game_page == nullptr || ui.chat_page == nullptr) {
        return;
    }
    const bool chat = ui.state != STC_SHELL_GAME;
    if (chat) {
        DestroyGameUiLocked();
        lv_obj_add_flag(ui.game_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui.chat_page, LV_OBJ_FLAG_HIDDEN);
        EnsureChatUiLocked();
    } else {
        lv_obj_add_flag(ui.chat_page, LV_OBJ_FLAG_HIDDEN);
        if (ui.chat_ui != nullptr) {
            stc_chat_ui_destroy(ui.chat_ui);
            ui.chat_ui = nullptr;
        }
        ui.chat_applied = false;
        ui.applied_chat_message[0] = '\0';
        EnsureGameUiLocked();
        lv_obj_clear_flag(ui.game_page, LV_OBJ_FLAG_HIDDEN);
    }

}

void ApplyRuntimeStatusLocked(stc_shell_state_t state, DeviceState device_state,
                              bool network_connected) {
    const bool state_changed = ui.state != state;
    ui.state = state;
    ui.network_connected = network_connected;
    if (state_changed) {
        ApplyVisualStateLocked();
    }
    if (state == STC_SHELL_GAME) {
        ui.goodbye_pending = false;
        ui.goodbye_reply_seen = false;
        ui.goodbye_exit_requested = false;
        ui.goodbye_started_us = 0;
        ui.goodbye_display_due_us = 0;
    }
    if (state != STC_SHELL_GAME) {
        const int64_t now_us = esp_timer_get_time();
        if (ui.goodbye_pending) {
            if (device_state == kDeviceStateSpeaking) {
                ui.goodbye_reply_seen = true;
            }
            if (ui.goodbye_display_due_us == 0 &&
                ((ui.goodbye_reply_seen && device_state == kDeviceStateIdle) ||
                 (!ui.goodbye_reply_seen &&
                  now_us - ui.goodbye_started_us >= 6 * 1000 * 1000))) {
                ui.goodbye_display_due_us = now_us + 1500 * 1000;
            }
            if (ui.goodbye_display_due_us != 0) {
                ui.chat_state = STC_CHAT_GOODBYE;
                RefreshChatLocked();
                if (now_us >= ui.goodbye_display_due_us &&
                    !ui.goodbye_exit_requested) {
                    ui.goodbye_exit_requested = true;
                    ESP_LOGI(TAG, "STC_CHAT_GOODBYE auto_home");
                    if (ui.request_mode) {
                        ui.request_mode(STC_SHELL_GAME);
                    }
                }
                return;
            }
        }
        if (WifiManager::GetInstance().IsConfigMode()) {
            ui.chat_state = STC_CHAT_PROVISIONING;
        } else if (!network_connected &&
                   state == STC_SHELL_CHAT_STARTING) {
            ui.chat_state = STC_CHAT_CONNECTING;
        } else if (!network_connected) {
            ui.chat_state = STC_CHAT_OFFLINE;
        } else if (device_state == kDeviceStateListening) {
            ui.chat_state = STC_CHAT_LISTENING;
        } else if (device_state == kDeviceStateSpeaking) {
            ui.chat_state = STC_CHAT_ANSWERING;
        } else if (device_state == kDeviceStateConnecting ||
                   device_state == kDeviceStateActivating) {
            ui.chat_state = STC_CHAT_THINKING;
        } else if (device_state == kDeviceStateFatalError) {
            ui.chat_state = STC_CHAT_OFFLINE;
        } else {
            ui.chat_state = STC_CHAT_READY;
        }
        RefreshChatLocked();
    }
}

void SetSubtitleLocked(const char* role, const char* content) {
    UpdateChatMessageFontLocked();
    const char* normalized_role = role != nullptr ? role : "assistant";
    const char* text = content != nullptr ? content : "";
    const bool assistant_chunk = strcmp(normalized_role, "assistant") == 0;
    const bool same_role = strcmp(ui.chat_role, normalized_role) == 0;

    /* Some providers emit tool-call progress as assistant text, for example
     * "% get_weather...". Keep it out of the learner-facing subtitle. */
    if (assistant_chunk && text[0] == '%') {
        return;
    }
    if (assistant_chunk && same_role && text[0] != '\0' &&
        ui.chat_message[0] != '\0') {
        const size_t used = strlen(ui.chat_message);
        if (used + 1 < sizeof(ui.chat_message)) {
            snprintf(ui.chat_message + used,
                     sizeof(ui.chat_message) - used, "%s", text);
        }
    } else {
        snprintf(ui.chat_message, sizeof(ui.chat_message), "%s", text);
    }
    snprintf(ui.chat_role, sizeof(ui.chat_role), "%s", normalized_role);
    if (strcmp(normalized_role, "user") == 0 &&
        stc_shell_text_is_goodbye(text) && !ui.goodbye_pending) {
        ui.goodbye_pending = true;
        ui.goodbye_reply_seen = false;
        ui.goodbye_exit_requested = false;
        ui.goodbye_started_us = esp_timer_get_time();
        ui.goodbye_display_due_us = 0;
        ESP_LOGI(TAG, "STC_CHAT_GOODBYE detected");
    }
    if (ui.chat_ui == nullptr) {
        return;
    }
    if (strcmp(normalized_role, "user") == 0) {
        ui.chat_state = STC_CHAT_LISTENING;
    } else if (ui.network_connected) {
        ui.chat_state = STC_CHAT_ANSWERING;
    }
    RefreshChatLocked();
    ESP_LOGI(TAG, "STC_CHAT_UI message role=%s bytes=%u", normalized_role,
             static_cast<unsigned>(ui.chat_message[0] == '\0'
                                        ? 0
                                        : strlen(ui.chat_message)));
}

}  // namespace

stc_err_t stc_shell_ui_init(
    Display* display,
    std::function<void(stc_shell_state_t)> request_mode,
    std::function<void()> request_dialog) {
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
    ui.request_dialog = std::move(request_dialog);
    (void)EnsurePreviewBuffers();

    lv_obj_t* top = lv_screen_active();
    ui.root = lv_obj_create(top);
    lv_obj_set_size(ui.root, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(ui.root, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(ui.root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui.root, 0, 0);
    lv_obj_set_style_pad_all(ui.root, 0, 0);
    lv_obj_remove_flag(ui.root, LV_OBJ_FLAG_SCROLLABLE);

    ui.game_page = lv_obj_create(ui.root);
    lv_obj_remove_style_all(ui.game_page);
    lv_obj_set_size(ui.game_page, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(ui.game_page, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_flag(ui.game_page, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(ui.game_page, 0, 0);
    lv_obj_remove_flag(ui.game_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_update_layout(ui.game_page);
    ESP_LOGI(TAG,
             "STC_GAME_UI attach parent=%ldx%ld content=%ldx%ld",
             static_cast<long>(lv_obj_get_width(ui.game_page)),
             static_cast<long>(lv_obj_get_height(ui.game_page)),
             static_cast<long>(lv_obj_get_content_width(ui.game_page)),
             static_cast<long>(lv_obj_get_content_height(ui.game_page)));
    ui.game_ui = stc_ui_bridge_create(ui.game_page, RetryGame, nullptr);
    if (!ui.game_ui) {
        ESP_LOGE(TAG, "STC game UI could not attach");
        return STC_ERR_IO;
    }
    if (ui.game_action_callback) {
        stc_ui_bridge_set_action_callback(ui.game_ui, BridgeGameAction);
    }

    BuildChatPage(ui.root);

    ui.state = STC_SHELL_GAME;
    SetSubtitleLocked("assistant", "");
    ApplyVisualStateLocked();
    ESP_LOGI(TAG,
             "STC_SHELL_UI ready formal_game_ui=1 chat_page=1");
    return STC_OK;
}

void stc_shell_ui_set_state(stc_shell_state_t state) {
    if (ui.display == nullptr || ui.root == nullptr) {
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

void stc_shell_ui_render_game(const stc_snapshot_t* snapshot,
                              const stc_game_view_t* view) {
    if (!ui.display || !ui.game_ui || !snapshot || !view) {
        return;
    }
    stc_ui_payload_t payload = {};
    if (!stc_app_payload_from_state(snapshot, view, &payload)) {
        return;
    }
    stc_ui_bridge_payload_t bridge = {
        .state = (int32_t)payload.state,
        .category_id = payload.category_id,
        .confidence = payload.confidence,
        .score = payload.score,
        .streak = payload.streak,
        .lid_open = payload.lid_open,
        .error_code = payload.error_code,
        .round = payload.round,
        .rounds_total = payload.rounds_total,
        .time_left_ms = payload.time_left_ms,
        .time_total_ms = payload.time_total_ms,
        .debug_flags = payload.debug_flags,
        .reserved = payload.reserved,
        .scanned_bin = payload.learning.scanned_bin,
        .dropped_bin = payload.learning.dropped_bin,
        .correct_bin = payload.learning.correct_bin,
        .outcome = (int32_t)payload.learning.outcome,
        .round_delta = payload.learning.round_delta,
    };
    for (int i = 0; i < 2; ++i) {
        bridge.candidates[i].category_id = payload.candidates[i].category_id;
        bridge.candidates[i].p = payload.candidates[i].p;
    }
    DisplayLockGuard lock(ui.display);
    if (!lock) {
        return;
    }
    if (!stc_ui_bridge_apply(ui.game_ui, &bridge)) {
        ESP_LOGW(TAG, "formal game UI rejected state=%d",
                 (int)payload.state);
    }
}

void stc_shell_ui_set_game_action_callback(stc_ui_action_cb callback) {
    ui.game_action_callback = callback;
    if (ui.game_ui) {
        stc_ui_bridge_set_action_callback(
            ui.game_ui, callback ? BridgeGameAction : nullptr);
    }
}

void stc_shell_ui_set_camera_rgb(const uint8_t* rgb, uint16_t width,
                                 uint16_t height) {
    if (!ui.display || !ui.game_ui || !rgb || width != height ||
        (width != 160 && width != kCameraPreviewSize)) {
        return;
    }
    if (!EnsurePreviewBuffers()) {
        return;
    }
    const int next = preview_index < 0 ? 0 : 1 - preview_index;
    if (stc_preprocess_rgb_to_rgb565(rgb, width * height,
                                     preview_pixels[next]) != STC_OK) {
        return;
    }
    DisplayLockGuard lock(ui.display);
    if (!lock) {
        return;
    }
    preview_desc[next].header.w = width;
    preview_desc[next].header.h = height;
    preview_desc[next].header.stride = width * sizeof(uint16_t);
    preview_desc[next].data_size =
        width * height * sizeof(uint16_t);
    stc_ui_bridge_set_images(ui.game_ui, &preview_desc[next], nullptr);
    preview_index = next;
}

void stc_shell_ui_set_camera_rgb160(const uint8_t* rgb, uint16_t width,
                                    uint16_t height) {
    stc_shell_ui_set_camera_rgb(rgb, width, height);
}

int32_t stc_shell_ui_time_bar_value(void) {
    if (!ui.display || !ui.game_ui) {
        return -1;
    }
    DisplayLockGuard lock(ui.display);
    if (!lock) {
        return -1;
    }
    return stc_ui_bridge_time_bar_value(ui.game_ui);
}

void stc_shell_ui_show_guide(void) {
    if (!ui.display || !ui.game_ui) {
        return;
    }
    DisplayLockGuard lock(ui.display);
    if (lock) {
        stc_ui_bridge_show_guide(ui.game_ui);
    }
}

void stc_shell_ui_request_chat(void) {
    /* 問綠豆: leave the game page and let the shell start the voice session. */
    RequestMode(STC_SHELL_CHAT_STARTING);
}

void stc_shell_ui_refresh_status(stc_shell_state_t state,
                                 DeviceState device_state,
                                 bool network_connected) {
    if (ui.display == nullptr || ui.root == nullptr ||
        ui.chat_page == nullptr) {
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

void stc_shell_ui_clear_chat_message(void) {
    if (ui.display == nullptr) {
        return;
    }
    DisplayLockGuard lock(ui.display);
    if (!lock) {
        return;
    }
    ui.chat_role[0] = '\0';
    ui.chat_message[0] = '\0';
    ui.chat_state = STC_CHAT_READY;
    RefreshChatLocked();
}

bool stc_shell_ui_ready() { return ui.root != nullptr && ui.game_ui != nullptr; }
