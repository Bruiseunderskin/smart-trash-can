/* test_e2e.c — 10 個 case 走完一回合（8 個正常 + 2 個拒判）（#53 I5／H6） */
#include <stdio.h>
#include <string.h>

#include "stc_game.h"
#include "stc_lid.h"
#include "stc_mode.h"

static int run_case(stc_bin_t truth, bool correct, bool abstain, int expect_score)
{
    if (stc_game_init() != STC_OK) return 1;
    stc_mode_init();
    (void)stc_mode_input(STC_IN_PLAY, 0);          /* BOOT → IDLE */
    if (stc_mode_input(STC_IN_PLAY, 1) != STC_OK) return 1;  /* IDLE → ROUND */

    stc_event_t l;
    memset(&l, 0, sizeof l);
    l.type = STC_EV_LABEL;
    l.u.label.bin = abstain ? STC_BIN_COUNT : truth;
    l.u.label.conf = abstain ? 0.4f : 0.88f;
    l.u.label.abstain = abstain;
    l.t_ms = 10;
    if (stc_game_input(&l) != STC_OK) return 1;

    uint32_t t = 20;
    stc_game_tick(t);
    if (abstain) {
        /* 第二次拒判才算數 */
        stc_event_t l2 = l;
        l2.t_ms = t;
        if (stc_game_input(&l2) != STC_OK) return 1;
        stc_game_tick(t + 10);
        if (stc_game_snapshot().phase != STC_PH_LEARNING) return 1;
        return stc_game_result().score == expect_score ? 0 : 1;
    }

    t += 1500; stc_game_tick(t);                   /* RESULT → OPEN */
    t += 1500; stc_game_tick(t);                   /* OPEN → WAIT_DROP */

    stc_event_t d;
    memset(&d, 0, sizeof d);
    d.type = STC_EV_DROP;
    d.t_ms = t;
    d.u.drop.bin = correct ? truth : ((truth == STC_BIN_OTHER) ? STC_BIN_FOOD : STC_BIN_OTHER);
    d.u.drop.dist_mm = 45;
    if (stc_game_input(&d) != STC_OK) return 1;

    t += 300; stc_game_tick(t);
    t += 1500; stc_game_tick(t);
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
        if (run_case(cases[i].bin, cases[i].correct, cases[i].abstain, cases[i].score) != 0) {
            fprintf(stderr, "case %d failed (phase=%d)\n", i, (int)stc_game_snapshot().phase);
            return 1;
        }
    }
    printf("test_e2e: 10/10 cases (8 normal, 2 abstain)\n");
    return 0;
}
