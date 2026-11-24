/* test_game_controller.c — five-round orchestration without hardware. */
#include <stdio.h>
#include <string.h>

#include "stc_game_controller.h"

static int g_scan_requests;
static int g_open_requests;
static int g_hold_requests;
static int g_close_requests;
static int g_close_not_ready;
static uint32_t g_close_round_id;
static int g_voice_lines;
static int g_open_voice_lines;
static int g_retry_voice_lines;
static int g_cheer_correct_lines;
static int g_encourage_wrong_lines;
static int g_summary_tip_lines;
static int g_sfx_events;
static int g_retry_sfx_events;
static int g_correct_sfx_events;
static int g_wrong_sfx_events;
static int g_round_sfx_events;
static int g_scan_sfx_events;
static int g_decide_sfx_events;
static int g_renders;
static uint32_t g_scan_round_id;
static uint32_t g_hold_round_id;
static stc_hand_action_t g_last_action;

static stc_err_t fake_scan(uint8_t round, uint32_t round_id, void *ctx)
{
    (void)ctx;
    if (round > 4) return STC_ERR_CONTRACT;
    g_scan_requests++;
    g_scan_round_id = round_id;
    return STC_OK;
}

static stc_err_t fake_open(uint8_t round, uint32_t round_id, void *ctx)
{
    (void)ctx;
    (void)round_id;
    if (round > 4) return STC_ERR_CONTRACT;
    g_open_requests++;
    g_last_action = STC_HAND_ACTION_OPEN_ALL;
    return STC_OK;
}

static stc_err_t fake_close(uint8_t round, uint32_t round_id, void *ctx)
{
    (void)ctx;
    (void)round_id;
    if (round > 4) return STC_ERR_CONTRACT;
    g_close_requests++;
    g_last_action = STC_HAND_ACTION_CLOSE_ALL;
    g_close_round_id = round_id;
    if (g_close_not_ready > 0) {
        g_close_not_ready--;
        return STC_ERR_NOT_READY;
    }
    return STC_OK;
}

static stc_err_t fake_hold(uint8_t round, uint32_t round_id, void *ctx)
{
    (void)ctx;
    (void)round;
    g_hold_requests++;
    g_hold_round_id = round_id;
    g_last_action = STC_HAND_ACTION_OPEN_ALL_HOLD;
    return STC_OK;
}

static stc_err_t fake_voice(stc_line_t line, void *ctx)
{
    (void)ctx;
    g_voice_lines++;
    if (line == STC_LINE_OPEN) g_open_voice_lines++;
    if (line == STC_LINE_RETRY) g_retry_voice_lines++;
    if (line == STC_LINE_CHEER_CORRECT_A ||
        line == STC_LINE_CHEER_CORRECT_B) {
        g_cheer_correct_lines++;
    }
    if (line == STC_LINE_ENCOURAGE_WRONG_A ||
        line == STC_LINE_ENCOURAGE_WRONG_B) {
        g_encourage_wrong_lines++;
    }
    if (line == STC_LINE_SUMMARY_TIP) g_summary_tip_lines++;
    return STC_OK;
}

static stc_err_t fake_sfx(stc_sfx_t sfx, void *ctx)
{
    (void)ctx;
    g_sfx_events++;
    if (sfx == STC_SFX_SCAN) g_scan_sfx_events++;
    if (sfx == STC_SFX_DECIDE) g_decide_sfx_events++;
    if (sfx == STC_SFX_CORRECT) g_correct_sfx_events++;
    if (sfx == STC_SFX_WRONG) g_wrong_sfx_events++;
    if (sfx == STC_SFX_RETRY) g_retry_sfx_events++;
    if (sfx == STC_SFX_ROUND) g_round_sfx_events++;
    return STC_OK;
}

static void fake_render(const stc_snapshot_t *snap,
                        const stc_game_view_t *view, void *ctx)
{
    (void)snap;
    (void)view;
    (void)ctx;
    g_renders++;
}

static stc_event_t label_event(stc_bin_t bin, uint32_t round_id, uint32_t now)
{
    stc_event_t ev;
    memset(&ev, 0, sizeof ev);
    ev.type = STC_EV_LABEL;
    ev.round_id = round_id;
    ev.t_ms = now;
    ev.u.label.bin = bin;
    ev.u.label.conf = 0.91f;
    ev.u.label.src = STC_SRC_REAL;
    return ev;
}

static int test_touch_bin_mode(void)
{
    stc_game_init();
    const stc_game_controller_ports_t ports = {
        .request_scan = fake_scan,
        .voice_say = fake_voice,
        .render = fake_render,
    };
    stc_game_controller_init(&ports, NULL);
    stc_game_controller_set_hand_online(true, 1000);
    if (stc_game_controller_ui_action(STC_UI_ACTION_START, 1000) != STC_OK) {
        fprintf(stderr, "touch mode start failed\n");
        return 1;
    }
    if (stc_game_controller_ui_action(STC_UI_ACTION_SCAN, 1001) != STC_OK) {
        fprintf(stderr, "touch mode scan failed\n");
        return 1;
    }
    const uint32_t round_id = stc_game_snapshot().round_id;
    stc_event_t label = label_event(STC_BIN_FOOD, round_id, 1002);
    if (stc_game_controller_on_label(&label, NULL, 1002) != STC_OK) {
        fprintf(stderr, "touch mode label failed\n");
        return 1;
    }

    uint32_t now = 1002;
    for (int i = 0; i < 3 && stc_game_snapshot().phase != STC_PH_WAIT_DROP; ++i) {
        now += 1600;
        stc_game_controller_tick(now);
    }
    if (stc_game_snapshot().phase != STC_PH_WAIT_DROP) {
        fprintf(stderr, "touch mode did not reach WAIT_DROP\n");
        return 1;
    }
    if (stc_game_controller_view()->fault_code != NULL) {
        fprintf(stderr, "touch mode hand fault: %s\n",
                stc_game_controller_view()->fault_code);
        return 1;
    }

    const stc_drop_t selected = {
        .bin = STC_BIN_FOOD,
        .dist_mm = 0,
        .t_ms = now,
    };
    if (stc_game_controller_on_drop(&selected, round_id, now) != STC_OK) {
        fprintf(stderr, "touch bin selection rejected\n");
        return 1;
    }
    if (stc_game_snapshot().dropped != STC_BIN_FOOD ||
        stc_game_snapshot().last_outcome != STC_ROUND_CORRECT) {
        fprintf(stderr, "touch bin did not score\n");
        return 1;
    }
    return 0;
}

static int test_unknown_plays_retry_guide(void)
{
    g_voice_lines = 0;
    g_open_voice_lines = 0;
    g_retry_voice_lines = 0;
    g_cheer_correct_lines = 0;
    g_encourage_wrong_lines = 0;
    g_summary_tip_lines = 0;
    g_sfx_events = 0;
    g_retry_sfx_events = 0;
    stc_game_init();
    const stc_game_controller_ports_t ports = {
        .request_scan = fake_scan,
        .voice_say = fake_voice,
        .voice_sfx = fake_sfx,
        .render = fake_render,
    };
    stc_game_controller_init(&ports, NULL);
    stc_game_controller_set_hand_online(false, 10);
    if (stc_game_controller_ui_action(STC_UI_ACTION_START, 10) != STC_OK) {
        return 1;
    }
    if (stc_game_controller_ui_action(STC_UI_ACTION_SCAN, 11) != STC_OK) {
        return 1;
    }
    const uint32_t round_id = stc_game_snapshot().round_id;
    stc_event_t unknown = label_event(STC_BIN_COUNT,
                                      round_id, 12);
    unknown.u.label.conf = 0.2f;
    unknown.u.label.abstain = true;
    if (stc_game_controller_on_label(&unknown, NULL, 12) != STC_OK) {
        return 1;
    }
    if (stc_game_snapshot().phase != STC_PH_PROMPT ||
        g_retry_voice_lines != 1 || g_retry_sfx_events != 1) {
        fprintf(stderr,
                "unknown guide phase=%d voice=%d sfx=%d\n",
                (int)stc_game_snapshot().phase,
                g_retry_voice_lines, g_retry_sfx_events);
        return 1;
    }
    return 0;
}

static int test_start_waits_for_busy_hand(void)
{
    g_close_requests = 0;
    g_close_not_ready = 1;
    stc_game_init();
    const stc_game_controller_ports_t ports = {
        .request_scan = fake_scan,
        .hand_open_all = fake_open,
        .hand_hold_all = fake_hold,
        .hand_close_all = fake_close,
        .voice_say = fake_voice,
        .render = fake_render,
    };
    stc_game_controller_init(&ports, NULL);
    stc_game_controller_set_hand_online(true, 0);

    if (stc_game_controller_ui_action(STC_UI_ACTION_START, 0) != STC_OK) {
        fprintf(stderr, "busy hand blocked game start\n");
        return 1;
    }
    if (stc_game_snapshot().phase != STC_PH_PROMPT ||
        g_close_requests != 1) {
        fprintf(stderr, "busy hand start did not defer close\n");
        return 1;
    }
    stc_game_controller_tick(10);
    if (g_close_requests != 2 || !g_close_round_id) {
        fprintf(stderr, "deferred close was not retried\n");
        return 1;
    }
    stc_game_controller_on_hand_ack(g_close_round_id,
                                    STC_HAND_ACTION_CLOSE_ALL, true, 11);
    if (stc_game_controller_view()->fault_code != NULL) {
        fprintf(stderr, "deferred close left fault=%s\n",
                stc_game_controller_view()->fault_code);
        return 1;
    }
    return 0;
}

static int test_offline_hand_uses_touch_bins(void)
{
    g_open_requests = 0;
    g_close_requests = 0;
    stc_game_init();
    const stc_game_controller_ports_t ports = {
        .request_scan = fake_scan,
        .hand_open_all = fake_open,
        .hand_hold_all = fake_hold,
        .hand_close_all = fake_close,
        .voice_say = fake_voice,
        .render = fake_render,
    };
    stc_game_controller_init(&ports, NULL);
    stc_game_controller_set_hand_online(false, 100);

    if (stc_game_controller_ui_action(STC_UI_ACTION_START, 100) != STC_OK) {
        fprintf(stderr, "offline hand blocked game start\n");
        return 1;
    }
    if (g_close_requests != 0 ||
        (stc_game_controller_view()->debug_flags &
         STC_UI_DEBUG_TOUCH_BINS) == 0) {
        fprintf(stderr, "offline hand did not select touch fallback\n");
        return 1;
    }
    if (stc_game_controller_ui_action(STC_UI_ACTION_SCAN, 101) != STC_OK) {
        fprintf(stderr, "offline touch scan failed\n");
        return 1;
    }
    const uint32_t round_id = stc_game_snapshot().round_id;
    const stc_event_t label = label_event(STC_BIN_OTHER, round_id, 102);
    if (stc_game_controller_on_label(&label, NULL, 102) != STC_OK) {
        return 1;
    }
    uint32_t now = 102;
    for (int i = 0; i < 3 && stc_game_snapshot().phase != STC_PH_WAIT_DROP;
         ++i) {
        now += 1600;
        stc_game_controller_tick(now);
    }
    if (stc_game_snapshot().phase != STC_PH_WAIT_DROP ||
        !stc_game_controller_view()->lid_open ||
        g_open_requests != 0) {
        fprintf(stderr, "offline touch bins were not shown without hand IO\n");
        return 1;
    }
    return 0;
}

int main(void)
{
    if (test_touch_bin_mode() != 0) return 1;
    if (test_unknown_plays_retry_guide() != 0) return 1;
    if (test_start_waits_for_busy_hand() != 0) return 1;
    if (test_offline_hand_uses_touch_bins() != 0) return 1;
    g_scan_requests = 0;
    g_open_requests = 0;
    g_hold_requests = 0;
    g_close_requests = 0;
    g_voice_lines = 0;
    g_open_voice_lines = 0;
    g_retry_voice_lines = 0;
    g_cheer_correct_lines = 0;
    g_encourage_wrong_lines = 0;
    g_summary_tip_lines = 0;
    g_sfx_events = 0;
    g_retry_sfx_events = 0;
    g_correct_sfx_events = 0;
    g_wrong_sfx_events = 0;
    g_round_sfx_events = 0;
    g_scan_sfx_events = 0;
    g_decide_sfx_events = 0;
    g_renders = 0;
    stc_game_init();
    stc_game_controller_ports_t ports = {
        .request_scan = fake_scan,
        .hand_open_all = fake_open,
        .hand_hold_all = fake_hold,
        .hand_close_all = fake_close,
        .voice_say = fake_voice,
        .voice_sfx = fake_sfx,
        .render = fake_render,
    };
    stc_game_controller_init(&ports, NULL);
    stc_game_controller_set_hand_online(true, 0);
    if (stc_game_controller_ui_action(STC_UI_ACTION_START, 0) != STC_OK) return 1;
    const int closes_before_rounds = g_close_requests;
    if (closes_before_rounds != 1) {
        fprintf(stderr, "initial start did not close hold baseline\n");
        return 1;
    }
    stc_game_controller_on_hand_ack(0, STC_HAND_ACTION_CLOSE_ALL, true, 0);

    uint32_t now = 1;
    const stc_bin_t answer[5] = {
        STC_BIN_FOOD, STC_BIN_HAZARD, STC_BIN_OTHER,
        STC_BIN_RECYCLABLE, STC_BIN_FOOD,
    };
    const stc_bin_t dropped[5] = {
        STC_BIN_OTHER, STC_BIN_OTHER, STC_BIN_OTHER,
        STC_BIN_RECYCLABLE, STC_BIN_FOOD,
    };

    for (int round = 0; round < 5; ++round) {
        if (stc_game_controller_ui_action(STC_UI_ACTION_SCAN, now) != STC_OK) return 1;
        if (g_scan_requests != round + 1) return 1;
        stc_event_t label = label_event(answer[round], g_scan_round_id, now);
        if (stc_game_controller_on_label(&label, NULL, now) != STC_OK) return 1;

        now += 1500;
        stc_game_controller_tick(now);
        if (g_open_requests != round + 1) {
            fprintf(stderr, "open request round %d\n", round);
            return 1;
        }
        if (g_open_voice_lines != round) {
            fprintf(stderr, "drop prompt played during opening round %d\n",
                    round);
            return 1;
        }
        if (stc_game_controller_view()->lid_open) {
            fprintf(stderr, "lid shown open before ACK\n");
            return 1;
        }
        stc_game_controller_on_hand_ack(g_scan_round_id + 0x100u,
                                        STC_HAND_ACTION_OPEN_ALL, true, now);
        if (stc_game_controller_view()->lid_open) {
            fprintf(stderr, "stale ACK opened lid\n");
            return 1;
        }
        stc_game_controller_on_hand_ack(g_scan_round_id,
                                        STC_HAND_ACTION_OPEN_ALL, true, now);
        if (!stc_game_controller_view()->lid_open) return 1;

        now += 1500;
        stc_game_controller_tick(now);
        if (g_open_voice_lines != round + 1) {
            fprintf(stderr, "drop prompt missing after lids opened round %d\n",
                    round);
            return 1;
        }
        stc_drop_t drop = {.bin = dropped[round], .dist_mm = 40};
        if (stc_game_controller_on_drop(&drop, g_scan_round_id + 0x100u,
                                        now) != STC_ERR_NOT_READY) {
            fprintf(stderr, "stale DROP accepted\n");
            return 1;
        }
        if (stc_game_controller_on_drop(&drop,
                                        stc_game_snapshot().round_id, now) != STC_OK) {
            return 1;
        }

        now += 300;
        stc_game_controller_tick(now);
        now += 1500;
        stc_game_controller_tick(now);
        if (g_close_requests != closes_before_rounds + round + 1) {
            fprintf(stderr, "close request round %d\n", round);
            return 1;
        }
        stc_game_controller_on_hand_ack(stc_game_snapshot().round_id,
                                        STC_HAND_ACTION_CLOSE_ALL, true, now);
        if (stc_game_controller_view()->lid_open) return 1;
        if (stc_game_snapshot().phase != STC_PH_LEARNING) return 1;
        if (stc_game_controller_ui_action(STC_UI_ACTION_NEXT, now) != STC_OK) return 1;
        now += 1;
    }

    if (stc_game_snapshot().phase != STC_PH_SUMMARY) {
        fprintf(stderr, "controller did not reach summary\n");
        return 1;
    }
    if (g_hold_requests != 1) {
        fprintf(stderr, "summary hold requests=%d\n", g_hold_requests);
        return 1;
    }
    if (g_last_action != STC_HAND_ACTION_OPEN_ALL_HOLD) return 1;
    if (g_scan_sfx_events != 5 || g_decide_sfx_events != 5 ||
        g_correct_sfx_events != 3 || g_wrong_sfx_events != 2 ||
        g_round_sfx_events != 1) {
        fprintf(stderr,
                "sfx sequence scan=%d decide=%d correct=%d wrong=%d "
                "round=%d\n",
                g_scan_sfx_events, g_decide_sfx_events,
                g_correct_sfx_events, g_wrong_sfx_events,
                g_round_sfx_events);
        return 1;
    }
    stc_game_controller_on_hand_ack(g_hold_round_id,
                                    STC_HAND_ACTION_OPEN_ALL_HOLD, true,
                                    now);
    if (!stc_game_controller_view()->lid_open) {
        fprintf(stderr, "summary hold ACK did not open lids\n");
        return 1;
    }

    const int closes_before_restart = g_close_requests;
    if (stc_game_controller_ui_action(STC_UI_ACTION_START, now + 1) != STC_OK) {
        fprintf(stderr, "restart after summary failed\n");
        return 1;
    }
    if (g_close_requests != closes_before_restart + 1) {
        fprintf(stderr, "restart did not request close before game\n");
        return 1;
    }
    if (stc_game_snapshot().phase != STC_PH_PROMPT) return 1;
    stc_game_controller_on_hand_ack(g_hold_round_id,
                                    STC_HAND_ACTION_CLOSE_ALL, true,
                                    now + 2);
    if (stc_game_controller_view()->lid_open) {
        fprintf(stderr, "restart close ACK left lids open\n");
        return 1;
    }
    if (g_cheer_correct_lines != 2 || g_encourage_wrong_lines != 1 ||
        g_summary_tip_lines != 1) {
        fprintf(stderr, "streak voice cheer=%d encourage=%d tip=%d\n",
                g_cheer_correct_lines, g_encourage_wrong_lines,
                g_summary_tip_lines);
        return 1;
    }
    if (g_voice_lines < 11) {
        fprintf(stderr, "voice lines=%d\n", g_voice_lines);
        return 1;
    }
    if (g_renders == 0) return 1;
    printf("test_game_controller: five-round orchestration and summary hold OK\n");
    return 0;
}
