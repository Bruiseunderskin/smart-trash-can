#pragma once

#include "stc_types.h"
#include "stc_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

stc_err_t stc_game_runtime_start(void);
void stc_game_runtime_submit_action(stc_ui_action_t action,
                                    int32_t category_id);
void stc_game_runtime_submit_mock_label(int32_t bin);
void stc_game_runtime_play_button(void);
void stc_game_runtime_render(void);
stc_err_t stc_game_runtime_suspend_hand(void);
stc_err_t stc_game_runtime_resume_hand(void);
bool stc_game_runtime_hand_online(void);
void stc_game_runtime_log_status(const char *source);

#ifdef __cplusplus
}
#endif
