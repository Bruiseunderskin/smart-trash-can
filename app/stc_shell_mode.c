#include "stc_shell_mode.h"

#include <stdbool.h>
#include <ctype.h>
#include <string.h>

static stc_shell_state_t g_state;
static stc_shell_hooks_t g_hooks;
static uint32_t g_last_activity;

static void call(void (*hook)(void))
{
    if (hook) {
        hook();
    }
}

static bool contains_ascii_case_insensitive(const char *text,
                                            const char *needle)
{
    if (!text || !needle || needle[0] == '\0') {
        return false;
    }
    const size_t needle_len = strlen(needle);
    for (const char *start = text; *start != '\0'; ++start) {
        size_t matched = 0;
        while (matched < needle_len && start[matched] != '\0' &&
               tolower((unsigned char)start[matched]) ==
                   tolower((unsigned char)needle[matched])) {
            ++matched;
        }
        if (matched == needle_len) {
            return true;
        }
    }
    return false;
}

bool stc_shell_text_is_goodbye(const char *text)
{
    return text != NULL &&
           (strstr(text, "拜拜") != NULL || strstr(text, "再見") != NULL ||
            contains_ascii_case_insensitive(text, "bye") ||
            contains_ascii_case_insensitive(text, "goodbye"));
}

void stc_shell_init(void)
{
    g_state = STC_SHELL_GAME;
    g_last_activity = 0;
    memset(&g_hooks, 0, sizeof g_hooks);
}

void stc_shell_set_hooks(const stc_shell_hooks_t *hooks)
{
    if (hooks) {
        g_hooks = *hooks;
    } else {
        memset(&g_hooks, 0, sizeof g_hooks);
    }
}

stc_shell_state_t stc_shell_get(void)
{
    return g_state;
}

stc_err_t stc_shell_enter_chat(uint32_t now_ms)
{
    if (g_state != STC_SHELL_GAME) {
        return STC_ERR_NOT_READY;
    }
    g_state = STC_SHELL_CHAT_STARTING;
    g_last_activity = now_ms;
    call(g_hooks.exit_game);
    call(g_hooks.enter_chat);
    return STC_OK;
}

stc_err_t stc_shell_mark_chat_ready(void)
{
    if (g_state != STC_SHELL_CHAT_STARTING) {
        return STC_ERR_NOT_READY;
    }
    g_state = STC_SHELL_CHAT_READY;
    return STC_OK;
}

stc_err_t stc_shell_notify_activity(uint32_t now_ms)
{
    if (g_state != STC_SHELL_CHAT_STARTING &&
        g_state != STC_SHELL_CHAT_READY) {
        return STC_ERR_NOT_READY;
    }
    g_last_activity = now_ms;
    return STC_OK;
}

stc_err_t stc_shell_exit_chat(void)
{
    if (g_state != STC_SHELL_CHAT_STARTING &&
        g_state != STC_SHELL_CHAT_READY) {
        return STC_ERR_NOT_READY;
    }
    g_state = STC_SHELL_GAME;
    g_last_activity = 0;
    call(g_hooks.exit_chat);
    call(g_hooks.enter_game);
    return STC_OK;
}

void stc_shell_tick(uint32_t now_ms)
{
    if (g_state != STC_SHELL_GAME &&
        (uint32_t)(now_ms - g_last_activity) >= STC_SHELL_CHAT_TIMEOUT_MS) {
        (void)stc_shell_exit_chat();
    }
}
