#pragma once

#include "device_state.h"
#include "display.h"
#include "stc_game.h"
#include "stc_game_controller.h"
#include "stc_shell_mode.h"

#include <stdint.h>
#include <functional>

stc_err_t stc_shell_ui_init(
    Display* display,
    std::function<void(stc_shell_state_t)> request_mode,
    std::function<void()> request_dialog = {});
void stc_shell_ui_set_state(stc_shell_state_t state);
void stc_shell_ui_render_game(const stc_snapshot_t* snapshot,
                              const stc_game_view_t* view);
void stc_shell_ui_set_game_action_callback(stc_ui_action_cb callback);
void stc_shell_ui_set_camera_rgb160(const uint8_t* rgb, uint16_t width,
                                    uint16_t height);
void stc_shell_ui_show_guide(void);
void stc_shell_ui_refresh_status(stc_shell_state_t state,
                                 DeviceState device_state,
                                 bool network_connected);
void stc_shell_ui_set_chat_message(const char* role, const char* content);
void stc_shell_ui_clear_chat_message(void);
bool stc_shell_ui_ready(void);
