#pragma once

#include "display.h"
#include "stc_app_fsm.h"

#include <functional>

stc_err_t stc_shell_ui_init(Display* display, std::function<void(stc_mode_t)> request_mode);
void stc_shell_ui_set_state(stc_app_state_t state);
bool stc_shell_ui_ready(void);
