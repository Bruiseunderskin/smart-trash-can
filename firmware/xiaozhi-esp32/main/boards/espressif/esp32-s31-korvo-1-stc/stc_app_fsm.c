#include "stc_app_fsm.h"

static stc_app_state_t state = STC_APP_GAME;

stc_err_t stc_app_fsm_init(void) {
    state = STC_APP_GAME;
    return STC_OK;
}

stc_app_state_t stc_app_fsm_get(void) {
    return state;
}

stc_err_t stc_app_fsm_request_chat(void) {
    if (state != STC_APP_GAME) {
        return STC_ERR_NOT_READY;
    }
    state = STC_APP_CHAT_STARTING;
    return STC_OK;
}

stc_err_t stc_app_fsm_mark_chat_ready(void) {
    if (state == STC_APP_CHAT_READY) {
        return STC_OK;
    }
    if (state != STC_APP_CHAT_STARTING) {
        return STC_ERR_NOT_READY;
    }
    state = STC_APP_CHAT_READY;
    return STC_OK;
}

stc_err_t stc_app_fsm_request_game(void) {
    state = STC_APP_GAME;
    return STC_OK;
}
