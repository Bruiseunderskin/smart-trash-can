#include <stdio.h>

#include "stc_app_fsm.h"

static int failures;

#define CHECK(expression)                                                        \
    do {                                                                         \
        if (!(expression)) {                                                     \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__,     \
                    #expression);                                                \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

int main(void) {
    CHECK(stc_app_fsm_init() == STC_OK);
    CHECK(stc_app_fsm_get() == STC_APP_GAME);

    CHECK(stc_app_fsm_request_chat() == STC_OK);
    CHECK(stc_app_fsm_get() == STC_APP_CHAT_STARTING);
    CHECK(stc_app_fsm_request_chat() == STC_ERR_NOT_READY);
    CHECK(stc_app_fsm_mark_chat_ready() == STC_OK);
    CHECK(stc_app_fsm_get() == STC_APP_CHAT_READY);
    CHECK(stc_app_fsm_mark_chat_ready() == STC_OK);

    CHECK(stc_app_fsm_request_game() == STC_OK);
    CHECK(stc_app_fsm_get() == STC_APP_GAME);
    CHECK(stc_app_fsm_mark_chat_ready() == STC_ERR_NOT_READY);
    CHECK(stc_app_fsm_request_game() == STC_OK);

    if (failures) {
        fprintf(stderr, "%d app FSM checks failed\n", failures);
        return 1;
    }
    puts("App FSM checks passed");
    return 0;
}
