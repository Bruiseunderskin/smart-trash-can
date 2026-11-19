#include <stdio.h>

#include "stc_mode.h"
#include "stc_mode_request.h"

int main(void)
{
    stc_mode_t requested;

    stc_mode_request_init();
    if (stc_mode_request_take(&requested)) {
        fprintf(stderr, "empty request queue returned an event\n");
        return 1;
    }

    if (stc_mode_request_post(STC_MODE_CHAT) != STC_OK) return 1;
    if (!stc_mode_request_take(&requested)) {
        fprintf(stderr, "chat request was lost\n");
        return 1;
    }
    if (requested != STC_MODE_CHAT) return 1;
    if (stc_mode_request_take(&requested)) {
        fprintf(stderr, "chat request was replayed\n");
        return 1;
    }

    if (stc_mode_request_post(STC_MODE_GAME) != STC_OK) return 1;
    if (!stc_mode_request_take(&requested) || requested != STC_MODE_GAME) {
        fprintf(stderr, "game request was lost\n");
        return 1;
    }
    if (stc_mode_request_take(&requested)) {
        fprintf(stderr, "game request was replayed\n");
        return 1;
    }

    puts("test_mode_request: one-shot GAME/CHAT requests OK");
    return 0;
}
