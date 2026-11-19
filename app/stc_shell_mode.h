/* stc_shell_mode.h — outer GAME/CHAT application shell. */
#pragma once

#include <stdint.h>

#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STC_SHELL_CHAT_TIMEOUT_MS 60000u

typedef enum {
    STC_SHELL_GAME = 0,
    STC_SHELL_CHAT_STARTING,
    STC_SHELL_CHAT_READY,
} stc_shell_state_t;

typedef struct {
    void (*exit_game)(void);
    void (*enter_chat)(void);
    void (*exit_chat)(void);
    void (*enter_game)(void);
} stc_shell_hooks_t;

void stc_shell_init(void);
void stc_shell_set_hooks(const stc_shell_hooks_t *hooks);
stc_shell_state_t stc_shell_get(void);
stc_err_t stc_shell_enter_chat(uint32_t now_ms);
stc_err_t stc_shell_mark_chat_ready(void);
stc_err_t stc_shell_notify_activity(uint32_t now_ms);
stc_err_t stc_shell_exit_chat(void);
void stc_shell_tick(uint32_t now_ms);

#ifdef __cplusplus
}
#endif
