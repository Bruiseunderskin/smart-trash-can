/* test_e2e.c — ten one-round cases: eight normal and two abstains. */
#include <stdio.h>
#include <string.h>

#include "stc_game.h"
#include "stc_lid.h"
#include "stc_mode.h"

static int scan_request(uint32_t t)
{
    stc_event_t ev;
    memset(&ev, 0, sizeof ev);
    ev.type = STC_EV_SCAN_REQUEST;
    ev.round_id = stc_game_snapshot().round_id;
    ev.t_ms = t;
    return stc_game_input(&ev) == STC_OK ? 0 : 1;
}

static int label_event(stc_bin_t bin, bool abstain, uint32_t t)
{
    stc_event_t ev;
    memset(&ev, 0, sizeof ev);
    ev.type = STC_EV_LABEL;
    ev.round_id = stc_game_snapshot().round_id;
    ev.u.label.bin = abstain ? STC_BIN_COUNT : bin;
    ev.u.label.conf = abstain ? 0.4f : 0.88f;
    ev.u.label.abstain = abstain;
    ev.u.label.src = STC_SRC_STUB;
    ev.t_ms = t;
    return stc_game_input(&ev) == STC_OK ? 0 : 1;
}

static int run_case(stc_bin_t truth, bool correct, bool abstain, int expect_score)
{
    if (stc_game_init() != STC_OK) return 1;
    stc_mode_init();
    (void)stc_mode_input(STC_IN_PLAY, 0);
    if (stc_mode_input(STC_IN_PLAY, 1) != STC_OK) return 1;
    if (scan_request(5)) return 1;
    if (label_event(truth, abstain, 10)) return 1;

    uint32_t t = 20;
    if (abstain) {
        if (stc_game_snapshot().phase != STC_PH_PROMPT) return 1;
        if (scan_request(t)) return 1;
        if (label_event(STC_BIN_COUNT, true, t)) return 1;
        if (stc_game_snapshot().phase != STC_PH_LEARNING) return 1;
        return stc_game_result().score == expect_score ? 0 : 1;
    }

    t += 1500;
    stc_game_tick(t);
    t += 1500;
    stc_game_tick(t);

    stc_event_t drop;
    memset(&drop, 0, sizeof drop);
    drop.type = STC_EV_DROP;
    drop.round_id = stc_game_snapshot().round_id;
    drop.t_ms = t;
    drop.u.drop.bin = correct
                          ? truth
                          : ((truth == STC_BIN_OTHER) ? STC_BIN_FOOD : STC_BIN_OTHER);
    drop.u.drop.dist_mm = 45;
    if (stc_game_input(&drop) != STC_OK) return 1;

    t += 300;
    stc_game_tick(t);
    t += 1500;
    stc_game_tick(t);
    if (stc_game_snapshot().phase != STC_PH_LEARNING) return 1;
    return stc_game_result().score == expect_score ? 0 : 1;
}

int main(void)
{
    struct { stc_bin_t bin; bool correct; bool abstain; int score; } cases[10] = {
        {STC_BIN_FOOD, true, false, 10},
        {STC_BIN_FOOD, false, false, 0},
        {STC_BIN_HAZARD, true, false, 10},
        {STC_BIN_HAZARD, false, false, 0},
        {STC_BIN_OTHER, true, false, 10},
        {STC_BIN_OTHER, false, false, 0},
        {STC_BIN_RECYCLABLE, true, false, 10},
        {STC_BIN_RECYCLABLE, false, false, 0},
        {STC_BIN_COUNT, false, true, 0},
        {STC_BIN_COUNT, false, true, 0},
    };

    for (int i = 0; i < 10; ++i) {
        if (run_case(cases[i].bin, cases[i].correct,
                     cases[i].abstain, cases[i].score) != 0) {
            fprintf(stderr, "case %d failed (phase=%d)\n",
                    i, (int)stc_game_snapshot().phase);
            return 1;
        }
    }
    printf("test_e2e: 10/10 cases (8 normal, 2 abstain)\n");
    return 0;
}
