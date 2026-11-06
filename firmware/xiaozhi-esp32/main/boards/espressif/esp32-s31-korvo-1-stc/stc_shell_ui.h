#pragma once

#include "device_state.h"
#include "display.h"
#include "stc_app_fsm.h"

#include <functional>

stc_err_t stc_shell_ui_init(Display* display, std::function<void(stc_mode_t)> request_mode);
void stc_shell_ui_set_state(stc_app_state_t state);
void stc_shell_ui_refresh_status(stc_app_state_t state, DeviceState device_state,
                                 bool network_connected);
void stc_shell_ui_set_chat_message(const char* role, const char* content);
void stc_shell_ui_clear_chat_message(void);
bool stc_shell_ui_ready(void);
