#pragma once

#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STC_APP_GAME = 0,
    STC_APP_CHAT_STARTING,
    STC_APP_CHAT_READY,
} stc_app_state_t;

stc_err_t stc_app_fsm_init(void);
stc_app_state_t stc_app_fsm_get(void);
stc_err_t stc_app_fsm_request_chat(void);
stc_err_t stc_app_fsm_mark_chat_ready(void);
stc_err_t stc_app_fsm_request_game(void);

#ifdef __cplusplus
}
#endif
