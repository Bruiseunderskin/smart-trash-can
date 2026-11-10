/* test_app_wiring.c — 四個狀態 → payload 逐欄對應（#53 I4） */
#include <stdio.h>
#include <string.h>

#include "stc_app.h"

static stc_snapshot_t base(void)
{
    stc_snapshot_t s;
    memset(&s, 0, sizeof s);
    s.rounds_total = 5;
    s.truth = STC_BIN_HAZARD;
    s.conf = 0.77f;
    s.score = 30;
    s.agreement = 4;
    s.lid_open = true;
    return s;
}

int main(void)
{
    stc_ui_payload_t p;

    stc_snapshot_t s = base();
    s.phase = STC_PH_SCAN;
    if (!stc_app_payload_from_snapshot(&s, &p)) return 1;
    if (p.state != STC_UI_CAPTURING) { fprintf(stderr, "scan state\n"); return 1; }

    s.phase = STC_PH_RESULT;
    s.abstained = true;
    if (!stc_app_payload_from_snapshot(&s, &p)) return 1;
    if (p.state != STC_UI_RESULT_RETRY) { fprintf(stderr, "retry state\n"); return 1; }

    s.abstained = false;
    if (!stc_app_payload_from_snapshot(&s, &p)) return 1;
    if (p.state != STC_UI_CLASSIFYING) { fprintf(stderr, "classifying state\n"); return 1; }
    if (p.category_id != (int32_t)STC_BIN_HAZARD) { fprintf(stderr, "category\n"); return 1; }
    if (p.confidence < 0.76f || p.confidence > 0.78f) { fprintf(stderr, "conf\n"); return 1; }
    if (p.score != 30u || p.streak != 4u || !p.lid_open) { fprintf(stderr, "fields\n"); return 1; }

    s.phase = STC_PH_LEARNING;
    if (!stc_app_payload_from_snapshot(&s, &p) || p.state != STC_UI_LEARNING) {
        fprintf(stderr, "learning\n");
        return 1;
    }

    s.phase = STC_PH_SUMMARY;
    if (!stc_app_payload_from_snapshot(&s, &p) || p.state != STC_UI_SUMMARY) {
        fprintf(stderr, "summary\n");
        return 1;
    }

    s.phase = (stc_phase_t)99;
    if (stc_app_payload_from_snapshot(&s, &p)) { fprintf(stderr, "invalid phase accepted\n"); return 1; }

    printf("test_app_wiring: 4 states mapped, invalid phase rejected\n");
    return 0;
}
