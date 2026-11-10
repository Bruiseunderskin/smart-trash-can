#include "stc_mode.h"

#include <string.h>

#include "stc_game.h"

#define STC_CHAT_IDLE_MS 60000u

static stc_state_t g_state = STC_ST_BOOT;
static stc_mode_hooks_t g_hooks;
static uint32_t g_chat_since;
static bool g_chat_active;
static uint16_t g_cloud_violations;

static void call(void (*fn)(void)) { if (fn) fn(); }

stc_err_t stc_mode_init(void)
{
    g_state = STC_ST_BOOT;
    memset(&g_hooks, 0, sizeof g_hooks);
    g_chat_active = false;
    g_cloud_violations = 0;
    return STC_OK;
}

void stc_mode_set_hooks(const stc_mode_hooks_t *hooks)
{
    if (hooks) {
        g_hooks = *hooks;
    } else {
        memset(&g_hooks, 0, sizeof g_hooks);
    }
}

stc_state_t stc_mode_get(void) { return g_state; }

stc_mode_t stc_mode_layer(void)
{
    return (g_state == STC_ST_CHAT) ? STC_MODE_CHAT : STC_MODE_GAME;
}

const char *stc_mode_state_name(stc_state_t s)
{
    switch (s) {
    case STC_ST_BOOT: return "boot";
    case STC_ST_GAME_IDLE: return "game_idle";
    case STC_ST_GAME_ROUND: return "game_round";
    case STC_ST_SUMMARY: return "summary";
    case STC_ST_CHAT: return "chat";
    case STC_ST_FAULT: return "fault";
    default: return "unknown";
    }
}

bool stc_mode_allows_game(void) { return g_state != STC_ST_CHAT && g_state != STC_ST_FAULT; }
bool stc_mode_allows_chat(void) { return g_state == STC_ST_GAME_IDLE; }

void stc_mode_note_cloud_activity(void)
{
    if (stc_mode_layer() == STC_MODE_GAME && g_cloud_violations < 0xFFFFu) {
        g_cloud_violations++;
    }
}

uint16_t stc_mode_cloud_violations(void) { return g_cloud_violations; }

static stc_err_t to_idle_from_chat(void)
{
    g_chat_active = false;
    call(g_hooks.exit_chat);
    call(g_hooks.enter_game);
    g_state = STC_ST_GAME_IDLE;
    return STC_OK;
}

stc_err_t stc_mode_input(stc_input_t in, uint32_t now_ms)
{
    switch (in) {
    case STC_IN_FAULT:
        call(g_hooks.exit_chat);
        g_chat_active = false;
        g_state = STC_ST_FAULT;
        return STC_OK;
    case STC_IN_RESET:
        if (g_state != STC_ST_FAULT) {
            return STC_ERR_NOT_READY;
        }
        g_state = STC_ST_BOOT;
        return STC_OK;
    default:
        break;
    }

    switch (g_state) {
    case STC_ST_BOOT:
        if (in == STC_IN_PLAY) {
            call(g_hooks.enter_game);
            g_state = STC_ST_GAME_IDLE;
            return STC_OK;
        }
        return STC_ERR_NOT_READY;

    case STC_ST_GAME_IDLE:
        if (in == STC_IN_PLAY) {
            call(g_hooks.enter_round);
            (void)stc_game_start();
            g_state = STC_ST_GAME_ROUND;
            return STC_OK;
        }
        if (in == STC_IN_CHAT_ENTER) {
            call(g_hooks.enter_chat);
            g_chat_active = true;
            g_chat_since = now_ms;
            g_state = STC_ST_CHAT;
            return STC_OK;
        }
        return STC_ERR_NOT_READY;

    case STC_ST_GAME_ROUND:
        if (in == STC_IN_ROUND_END) {
            const stc_snapshot_t snap = stc_game_snapshot();
            call(g_hooks.exit_round);
            if ((uint8_t)(snap.round + 1) >= snap.rounds_total && snap.rounds_total != 0) {
                g_state = STC_ST_SUMMARY;
            } else {
                g_state = STC_ST_GAME_IDLE;
            }
            return STC_OK;
        }
        return STC_ERR_NOT_READY;   /* PLAY、SET_LONG 在回合中一律拒絕 */

    case STC_ST_SUMMARY:
        if (in == STC_IN_PLAY) {
            g_state = STC_ST_GAME_IDLE;
            return STC_OK;
        }
        return STC_ERR_NOT_READY;

    case STC_ST_CHAT:
        if (in == STC_IN_CHAT_EXIT || in == STC_IN_CHAT_TIMEOUT) {
            return to_idle_from_chat();
        }
        return STC_ERR_NOT_READY;

    case STC_ST_FAULT:
        return STC_ERR_NOT_READY;

    default:
        return STC_ERR_NOT_READY;
    }
}

void stc_mode_tick(uint32_t now_ms)
{
    if (g_state == STC_ST_CHAT && g_chat_active &&
        (uint32_t)(now_ms - g_chat_since) >= STC_CHAT_IDLE_MS) {
        (void)to_idle_from_chat();
    }
}
