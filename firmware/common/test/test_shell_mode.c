/* test_shell_mode.c — two-state outer shell around game/chat. */
#include <stdio.h>

#include "stc_shell_mode.h"

static int g_exit_game;
static int g_enter_chat;
static int g_exit_chat;
static int g_enter_game;

static void exit_game_hook(void) { g_exit_game++; }
static void enter_chat_hook(void) { g_enter_chat++; }
static void exit_chat_hook(void) { g_exit_chat++; }
static void enter_game_hook(void) { g_enter_game++; }

int main(void)
{
    const stc_shell_hooks_t hooks = {
        .exit_game = exit_game_hook,
        .enter_chat = enter_chat_hook,
        .exit_chat = exit_chat_hook,
        .enter_game = enter_game_hook,
    };
    stc_shell_init();
    stc_shell_set_hooks(&hooks);
    if (stc_shell_get() != STC_SHELL_GAME) return 1;

    if (stc_shell_enter_chat(1000) != STC_OK) return 1;
    if (stc_shell_get() != STC_SHELL_CHAT_STARTING) return 1;
    if (g_exit_game != 1 || g_enter_chat != 1) {
        fprintf(stderr, "chat entry hooks\n");
        return 1;
    }

    if (stc_shell_enter_chat(1100) != STC_ERR_NOT_READY) return 1;
    if (g_exit_game != 1 || g_enter_chat != 1) {
        fprintf(stderr, "duplicate chat entry side effect\n");
        return 1;
    }

    if (stc_shell_mark_chat_ready() != STC_OK) return 1;
    if (stc_shell_get() != STC_SHELL_CHAT_READY) return 1;
    if (g_enter_chat != 1) {
        fprintf(stderr, "chat ready repeated enter hook\n");
        return 1;
    }
    if (stc_shell_mark_chat_ready() != STC_ERR_NOT_READY) return 1;

    if (stc_shell_notify_activity(45000) != STC_OK) return 1;
    stc_shell_tick(1000 + STC_SHELL_CHAT_TIMEOUT_MS - 1);
    if (stc_shell_get() != STC_SHELL_CHAT_READY) return 1;
    stc_shell_tick(45000 + STC_SHELL_CHAT_TIMEOUT_MS);
    if (stc_shell_get() != STC_SHELL_GAME) return 1;
    if (g_exit_chat != 1 || g_enter_game != 1) {
        fprintf(stderr, "chat timeout hooks\n");
        return 1;
    }

    if (stc_shell_enter_chat(2000) != STC_OK) return 1;
    if (stc_shell_get() != STC_SHELL_CHAT_STARTING) return 1;
    if (stc_shell_exit_chat() != STC_OK) return 1;
    if (stc_shell_get() != STC_SHELL_GAME) return 1;
    if (g_exit_chat != 2 || g_enter_game != 2) {
        fprintf(stderr, "manual chat exit hooks\n");
        return 1;
    }
    if (stc_shell_exit_chat() != STC_ERR_NOT_READY) return 1;

    puts("test_shell_mode: game/chat transitions and timeout OK");
    return 0;
}
