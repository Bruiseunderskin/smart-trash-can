/* test_app_wiring.c — controller snapshot/view to UI payload. */
#include <stdio.h>
#include <string.h>

#include "stc_app.h"

static stc_snapshot_t base(void)
{
    stc_snapshot_t s;
    memset(&s, 0, sizeof s);
    s.rounds_total = 5;
    s.round = 2;
    s.round_id = 17;
    s.truth = STC_BIN_HAZARD;
    s.dropped = STC_BIN_FOOD;
    s.last_outcome = STC_ROUND_WRONG;
    s.round_delta = -5;
    s.conf = 0.77f;
    s.score = 30;
    s.streak = 2;
    s.agreement = 5;
    s.lid_open = true;
    return s;
}

int main(void)
{
    stc_ui_payload_t p;
    stc_game_view_t view;
    memset(&view, 0, sizeof view);
    view.hand_online = true;
    view.lid_open = true;
    view.debug_flags =
        STC_UI_DEBUG_REPLAY | STC_UI_DEBUG_REAL_HAND |
        STC_UI_DEBUG_LIVE_CAMERA | STC_UI_DEBUG_DEV_FLOW;

    stc_snapshot_t s = base();
    s.phase = STC_PH_PROMPT;
    if (!stc_app_payload_from_state(&s, &view, &p)) return 1;
    if (p.state != STC_UI_SCAN_READY) {
        fprintf(stderr, "prompt state=%d\n", (int)p.state);
        return 1;
    }
    if (p.round != 3 || p.rounds_total != 5) {
        fprintf(stderr, "round fields\n");
        return 1;
    }
    if (p.debug_flags !=
        (STC_UI_DEBUG_REPLAY | STC_UI_DEBUG_REAL_HAND |
         STC_UI_DEBUG_LIVE_CAMERA | STC_UI_DEBUG_DEV_FLOW)) {
        fprintf(stderr, "debug flags\n");
        return 1;
    }

    s.phase = STC_PH_SCAN;
    if (!stc_app_payload_from_state(&s, &view, &p)) return 1;
    if (p.state != STC_UI_CAPTURING) { fprintf(stderr, "scan state\n"); return 1; }

    s.phase = STC_PH_RESULT;
    if (!stc_app_payload_from_state(&s, &view, &p)) return 1;
    if (p.state != STC_UI_CLASSIFYING) { fprintf(stderr, "classifying state\n"); return 1; }

    s.phase = STC_PH_OPEN;
    if (!stc_app_payload_from_state(&s, &view, &p)) return 1;
    if (p.state != STC_UI_CLASSIFYING) {
        fprintf(stderr, "open should remain busy\n");
        return 1;
    }

    s.phase = STC_PH_WAIT_DROP;
    if (!stc_app_payload_from_state(&s, &view, &p)) return 1;
    if (p.state != STC_UI_BIN_CHOICE ||
        p.category_id != (int32_t)STC_BIN_HAZARD ||
        p.confidence < 0.76f || p.confidence > 0.78f ||
        p.score != 30u || p.streak != 2u) {
        fprintf(stderr, "bin choice fields\n");
        return 1;
    }

    s.phase = STC_PH_LEARNING;
    if (!stc_app_payload_from_state(&s, &view, &p) ||
        p.state != STC_UI_LEARNING) {
        fprintf(stderr, "learning\n");
        return 1;
    }
    if (p.learning.scanned_bin != STC_BIN_HAZARD ||
        p.learning.dropped_bin != STC_BIN_FOOD ||
        p.learning.correct_bin != STC_BIN_HAZARD ||
        p.learning.outcome != STC_UI_ROUND_WRONG ||
        p.learning.round_delta != -5) {
        fprintf(stderr, "learning detail\n");
        return 1;
    }

    s.phase = STC_PH_SUMMARY;
    if (!stc_app_payload_from_state(&s, &view, &p) ||
        p.state != STC_UI_SUMMARY) {
        fprintf(stderr, "summary\n");
        return 1;
    }

    s.phase = STC_PH_SCAN;
    view.fault_code = "HAND_OFFLINE";
    if (!stc_app_payload_from_state(&s, &view, &p) ||
        p.state != STC_UI_ERROR ||
        strcmp(p.error_code, "HAND_OFFLINE") != 0) {
        fprintf(stderr, "fault payload\n");
        return 1;
    }
    view.fault_code = NULL;

    s.phase = (stc_phase_t)99;
    if (stc_app_payload_from_state(&s, &view, &p)) {
        fprintf(stderr, "invalid phase accepted\n");
        return 1;
    }

    printf("test_app_wiring: states, learning data and debug flags mapped\n");
    return 0;
}
