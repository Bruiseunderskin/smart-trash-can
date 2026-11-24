#include "stc_game_controller.h"

#include <string.h>

#include "stc_vision.h"

static stc_game_controller_ports_t g_ports;
static void *g_user;
static stc_game_view_t g_view;
static uint32_t g_open_sent_for;
static uint32_t g_drop_prompt_for;
static uint32_t g_close_sent_for;
static uint32_t g_feedback_for;
static uint32_t g_summary_hold_for;
static uint8_t g_wrong_streak;
static uint32_t g_start_close_round_id;
static bool g_start_close_pending;
static bool g_start_close_sent;
static bool g_summary_announced;
static uint8_t g_debug_base_flags;

static bool touch_bin_mode(void)
{
    return !g_view.hand_online ||
           (!g_ports.hand_open_all && !g_ports.hand_close_all);
}

static void play_sfx(stc_sfx_t sfx)
{
    if (g_ports.voice_sfx) {
        (void)g_ports.voice_sfx(sfx, g_user);
    }
}

static void sync_debug_mode(void)
{
    g_view.debug_flags =
        (uint8_t)(g_debug_base_flags &
                  ~(STC_UI_DEBUG_TOUCH_BINS | STC_UI_DEBUG_REAL_HAND));
    g_view.debug_flags |= touch_bin_mode() ? STC_UI_DEBUG_TOUCH_BINS
                                           : STC_UI_DEBUG_REAL_HAND;
}

static void render(void)
{
    if (g_ports.render) {
        const stc_snapshot_t snap = stc_game_snapshot();
        g_ports.render(&snap, &g_view, g_user);
    }
}

static void set_fault(const char *code)
{
    g_view.fault_code = code;
    render();
}

static void sync_view(void)
{
    const stc_snapshot_t snap = stc_game_snapshot();
    g_view.started = snap.phase != STC_PH_IDLE;
}

static stc_line_t choose_feedback(stc_line_t a, stc_line_t b,
                                   uint32_t round_id) {
    return ((round_id >> 1) & 1u) != 0 ? b : a;
}

static void say_result(void)
{
    if (!g_ports.voice_say) {
        return;
    }
    const stc_snapshot_t snap = stc_game_snapshot();
    switch (snap.last_outcome) {
    case STC_ROUND_CORRECT: {
        g_wrong_streak = 0;
        play_sfx(STC_SFX_CORRECT);
        const stc_line_t line =
            snap.streak >= 2
                ? choose_feedback(STC_LINE_CHEER_CORRECT_A,
                                  STC_LINE_CHEER_CORRECT_B, snap.round_id)
                : STC_LINE_CORRECT;
        (void)g_ports.voice_say(line, g_user);
        break;
    }
    case STC_ROUND_WRONG: {
        if (g_wrong_streak < UINT8_MAX) {
            g_wrong_streak++;
        }
        play_sfx(STC_SFX_WRONG);
        const stc_line_t line =
            g_wrong_streak >= 2
                ? choose_feedback(STC_LINE_ENCOURAGE_WRONG_A,
                                  STC_LINE_ENCOURAGE_WRONG_B, snap.round_id)
                : STC_LINE_WRONG;
        (void)g_ports.voice_say(line, g_user);
        break;
    }
    case STC_ROUND_TIMEOUT:
        g_wrong_streak = 0;
        (void)g_ports.voice_say(STC_LINE_TIMEOUT, g_user);
        break;
    case STC_ROUND_ABSTAIN:
    case STC_ROUND_NO_SCAN:
        g_wrong_streak = 0;
        play_sfx(STC_SFX_RETRY);
        (void)g_ports.voice_say(STC_LINE_RETRY, g_user);
        break;
    default:
        break;
    }
}

static void act_on_phase(void)
{
    const stc_snapshot_t snap = stc_game_snapshot();
    switch (snap.phase) {
    case STC_PH_OPEN:
        if (g_open_sent_for != snap.round_id) {
            g_open_sent_for = snap.round_id;
            if (touch_bin_mode()) {
                g_view.awaiting_open_ack = false;
                g_view.lid_open = true;
                g_view.fault_code = NULL;
                break;
            }
            g_view.awaiting_open_ack = true;
            g_view.fault_code = NULL;
            if (!g_view.hand_online) {
                set_fault("HAND_OFFLINE");
            } else if (!g_ports.hand_open_all ||
                       g_ports.hand_open_all(snap.round, snap.round_id, g_user) !=
                           STC_OK) {
                set_fault("HAND_OPEN_FAILED");
            }
        }
        break;
    case STC_PH_WAIT_DROP:
        if (!touch_bin_mode() && !g_view.lid_open) {
            set_fault("HAND_NOT_OPENED");
        } else if (g_drop_prompt_for != snap.round_id) {
            g_drop_prompt_for = snap.round_id;
            if (g_ports.voice_say) {
                (void)g_ports.voice_say(STC_LINE_OPEN, g_user);
            }
        }
        break;
    case STC_PH_CLOSE:
        if (g_close_sent_for != snap.round_id) {
            g_close_sent_for = snap.round_id;
            if (touch_bin_mode()) {
                break;
            }
            if (!g_ports.hand_close_all ||
                g_ports.hand_close_all(snap.round, snap.round_id, g_user) !=
                    STC_OK) {
                set_fault("HAND_CLOSE_FAILED");
            }
        }
        break;
    case STC_PH_LEARNING:
        if (g_feedback_for != snap.round_id) {
            g_feedback_for = snap.round_id;
            say_result();
        }
        break;
    case STC_PH_SUMMARY:
        if (!g_summary_announced) {
            g_summary_announced = true;
            play_sfx(STC_SFX_ROUND);
            if (g_ports.voice_say) {
                (void)g_ports.voice_say(STC_LINE_SUMMARY, g_user);
                (void)g_ports.voice_say(STC_LINE_SUMMARY_TIP, g_user);
            }
            g_view.started = true;
        }
        if (!touch_bin_mode() && g_summary_hold_for != snap.round_id) {
            g_summary_hold_for = snap.round_id;
            g_view.awaiting_open_ack = true;
            g_view.fault_code = NULL;
            if (!g_view.hand_online) {
                set_fault("HAND_OFFLINE");
            } else if (!g_ports.hand_hold_all ||
                       g_ports.hand_hold_all(snap.round, snap.round_id,
                                             g_user) != STC_OK) {
                set_fault("HAND_OPEN_FAILED");
            }
        }
        break;
    default:
        break;
    }
    render();
}

void stc_game_controller_init(const stc_game_controller_ports_t *ports,
                              void *user_ctx)
{
    memset(&g_ports, 0, sizeof g_ports);
    memset(&g_view, 0, sizeof g_view);
    if (ports) {
        g_ports = *ports;
    }
    g_user = user_ctx;
    g_view.hand_online = true;
    g_open_sent_for = 0;
    g_drop_prompt_for = 0;
    g_close_sent_for = 0;
    g_feedback_for = 0;
    g_summary_hold_for = 0;
    g_start_close_round_id = 0;
    g_start_close_pending = false;
    g_start_close_sent = false;
    g_summary_announced = false;
    g_wrong_streak = 0;
    g_debug_base_flags = 0;
    sync_debug_mode();
}

void stc_game_controller_set_debug_flags(uint8_t flags)
{
    g_debug_base_flags = flags;
    sync_debug_mode();
    render();
}

void stc_game_controller_set_hand_online(bool online, uint32_t now_ms)
{
    (void)now_ms;
    g_view.hand_online = online;
    sync_debug_mode();
    if (!online && g_view.awaiting_open_ack) {
        g_view.fault_code = "HAND_OFFLINE";
    } else if (online && g_view.fault_code &&
               strcmp(g_view.fault_code, "HAND_OFFLINE") == 0) {
        g_view.fault_code = NULL;
    }
    render();
}

stc_err_t stc_game_controller_ui_action(stc_ui_action_t action,
                                        uint32_t now_ms)
{
    const stc_snapshot_t before = stc_game_snapshot();
    stc_err_t err = STC_ERR_NOT_READY;

    switch (action) {
    case STC_UI_ACTION_START:
        if (before.phase == STC_PH_IDLE || before.phase == STC_PH_SUMMARY) {
            if (!touch_bin_mode()) {
                if (g_view.awaiting_open_ack) {
                    g_view.fault_code = "HAND_OPEN_PENDING";
                    render();
                    return STC_ERR_NOT_READY;
                }
                if (!g_view.hand_online) {
                    g_view.fault_code = "HAND_OFFLINE";
                    render();
                    return STC_ERR_NOT_READY;
                }
                if (!g_ports.hand_close_all) {
                    g_view.fault_code = "HAND_CLOSE_FAILED";
                    render();
                    return STC_ERR_NOT_READY;
                }
                const stc_err_t close =
                    g_ports.hand_close_all(before.round, before.round_id,
                                           g_user);
                if (close == STC_OK) {
                    g_start_close_pending = true;
                    g_start_close_sent = true;
                    g_start_close_round_id = before.round_id;
                } else if (close == STC_ERR_NOT_READY) {
                    // The previous OPEN_ALL_HOLD command may still be waiting
                    // for its ACK. Do not fail the round: send CLOSE_ALL as
                    // soon as the hand link becomes idle.
                    g_start_close_pending = true;
                    g_start_close_sent = false;
                    g_start_close_round_id = before.round_id;
                } else {
                    g_view.fault_code = "HAND_CLOSE_FAILED";
                    render();
                    return STC_ERR_NOT_READY;
                }
                g_view.awaiting_open_ack = false;
            }
            err = stc_game_start(now_ms);
            if (err == STC_OK) {
                const stc_snapshot_t after = stc_game_snapshot();
                if (g_start_close_pending && !g_start_close_sent) {
                    g_start_close_round_id = after.round_id;
                }
                g_open_sent_for = 0;
                g_drop_prompt_for = 0;
                g_close_sent_for = 0;
                g_feedback_for = 0;
                g_summary_hold_for = 0;
                g_summary_announced = false;
                g_wrong_streak = 0;
                g_view.started = true;
                g_view.fault_code = NULL;
                g_view.awaiting_open_ack = false;
            }
        }
        break;
    case STC_UI_ACTION_SCAN: {
        if (before.phase != STC_PH_PROMPT) {
            break;
        }
        stc_event_t ev = {0};
        ev.type = STC_EV_SCAN_REQUEST;
        ev.round_id = before.round_id;
        ev.t_ms = now_ms;
        err = stc_game_input(&ev);
        if (err == STC_OK) {
            play_sfx(STC_SFX_SCAN);
            const stc_snapshot_t after = stc_game_snapshot();
            if (!g_ports.request_scan ||
                g_ports.request_scan(after.round, after.round_id, g_user) != STC_OK) {
                g_view.fault_code = "SCAN_START_FAILED";
            }
        }
        break;
    }
    case STC_UI_ACTION_ASK: {
        /* Hand the screen to the voice assistant. Builds without a chat shell
         * leave the port NULL and the tap does nothing. */
        if (g_ports.request_chat) {
            g_ports.request_chat(g_user);
        }
        break;
    }
    case STC_UI_ACTION_NEXT: {
        if (before.phase != STC_PH_LEARNING) {
            break;
        }
        stc_event_t ev = {0};
        ev.type = STC_EV_CONTINUE;
        ev.round_id = before.round_id;
        ev.t_ms = now_ms;
        err = stc_game_input(&ev);
        break;
    }
    case STC_UI_ACTION_HOME: {
        const bool hand_online = g_view.hand_online;
        const bool held_open =
            g_view.lid_open && g_summary_hold_for != 0;
        err = stc_game_reset();
        if (err == STC_OK) {
            memset(&g_view, 0, sizeof g_view);
            g_view.hand_online = hand_online;
            g_view.lid_open = held_open;
            sync_debug_mode();
            g_open_sent_for = 0;
            g_close_sent_for = 0;
            g_feedback_for = 0;
            g_start_close_pending = false;
            g_start_close_sent = false;
            g_summary_announced = false;
            g_wrong_streak = 0;
        }
        break;
    }
    default:
        break;
    }

    act_on_phase();
    return err;
}

stc_err_t stc_game_controller_on_label(const stc_event_t *event,
                                       const stc_burst_detail_t *detail,
                                       uint32_t now_ms)
{
    (void)detail;
    (void)now_ms;
    if (!event || event->type != STC_EV_LABEL) {
        return STC_ERR_CONTRACT;
    }
    const stc_err_t err = stc_game_input(event);
    if (err == STC_OK) {
        if (event->u.label.abstain) {
            play_sfx(STC_SFX_RETRY);
            if (g_ports.voice_say) {
                (void)g_ports.voice_say(STC_LINE_RETRY, g_user);
            }
        } else {
            play_sfx(STC_SFX_DECIDE);
        }
        g_view.fault_code = NULL;
        act_on_phase();
    }
    return err;
}

stc_err_t stc_game_controller_on_drop(const stc_drop_t *drop,
                                      uint32_t round_id, uint32_t now_ms)
{
    if (!drop) {
        return STC_ERR_CONTRACT;
    }
    stc_event_t ev = {0};
    ev.type = STC_EV_DROP;
    ev.round_id = round_id;
    ev.t_ms = now_ms;
    ev.u.drop = *drop;
    const stc_err_t err = stc_game_input(&ev);
    if (err == STC_OK) {
        act_on_phase();
    }
    return err;
}

void stc_game_controller_on_scan_failed(stc_err_t error,
                                        uint32_t round_id, uint32_t now_ms)
{
    const stc_snapshot_t snap = stc_game_snapshot();
    if (snap.phase != STC_PH_SCAN || snap.round_id != round_id) {
        return;
    }
    if (error == STC_ERR_TIMEOUT) {
        stc_game_tick(now_ms + stc_game_get_config().scan_timeout_ms);
    } else {
        g_view.fault_code = "VISION_IO";
        render();
    }
}

void stc_game_controller_on_hand_ack(uint32_t round_id,
                                     stc_hand_action_t action, bool success,
                                     uint32_t now_ms)
{
    (void)now_ms;
    const stc_snapshot_t snap = stc_game_snapshot();
    if (action == STC_HAND_ACTION_CLOSE_ALL && g_start_close_pending &&
        round_id == g_start_close_round_id) {
        g_start_close_pending = false;
        g_start_close_sent = false;
        g_view.awaiting_open_ack = false;
        g_view.lid_open = !success && g_view.lid_open;
        if (!success) {
            g_view.fault_code = "HAND_CLOSE_FAILED";
        } else if (g_view.fault_code &&
                   strncmp(g_view.fault_code, "HAND_", 5) == 0) {
            g_view.fault_code = NULL;
        }
        render();
        return;
    }
    if (action == STC_HAND_ACTION_OPEN_ALL_HOLD) {
        if (snap.phase != STC_PH_SUMMARY ||
            round_id != g_summary_hold_for) {
            return;
        }
        g_view.awaiting_open_ack = false;
        g_view.lid_open = success;
        g_view.fault_code = success ? NULL : "HAND_OPEN_NACK";
        render();
        return;
    }
    if (round_id != snap.round_id) {
        return;
    }
    if (action == STC_HAND_ACTION_OPEN_ALL) {
        g_view.awaiting_open_ack = false;
        g_view.lid_open = success;
        if (!success) {
            g_view.fault_code = "HAND_OPEN_NACK";
        } else if (g_view.fault_code &&
                   strcmp(g_view.fault_code, "HAND_NOT_OPENED") == 0) {
            g_view.fault_code = NULL;
        }
    } else if (action == STC_HAND_ACTION_CLOSE_ALL) {
        if (success) {
            g_view.lid_open = false;
            if (g_view.fault_code &&
                strncmp(g_view.fault_code, "HAND_", 5) == 0) {
                g_view.fault_code = NULL;
            }
        } else {
            g_view.fault_code = "HAND_CLOSE_NACK";
        }
    }
    render();
}

void stc_game_controller_tick(uint32_t now_ms)
{
    if (!touch_bin_mode() && g_start_close_pending &&
        !g_start_close_sent) {
        const stc_snapshot_t snap = stc_game_snapshot();
        if (!g_view.hand_online) {
            g_view.fault_code = "HAND_OFFLINE";
        } else if (!g_ports.hand_close_all) {
            g_view.fault_code = "HAND_CLOSE_FAILED";
        } else {
            const stc_err_t close =
                g_ports.hand_close_all(snap.round, snap.round_id, g_user);
            if (close == STC_OK) {
                g_start_close_sent = true;
                g_start_close_round_id = snap.round_id;
            } else if (close != STC_ERR_NOT_READY) {
                g_start_close_pending = false;
                g_view.fault_code = "HAND_CLOSE_FAILED";
            }
        }
        render();
    }
    const stc_snapshot_t before = stc_game_snapshot();
    stc_game_tick(now_ms);
    const stc_snapshot_t after = stc_game_snapshot();
    sync_view();
    sync_debug_mode();
    if (before.phase != after.phase ||
        before.round_id != after.round_id) {
        act_on_phase();
    }
}

const stc_game_view_t *stc_game_controller_view(void) { return &g_view; }

stc_err_t stc_game_controller_clear_fault(uint32_t now_ms)
{
    (void)now_ms;
    g_view.fault_code = NULL;
    render();
    return STC_OK;
}

void stc_game_controller_set_fault(const char *code, uint32_t now_ms)
{
    (void)now_ms;
    g_view.fault_code = code;
    render();
}
