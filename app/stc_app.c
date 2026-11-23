#include "stc_app.h"

#include <string.h>

static bool map_phase(stc_phase_t phase, stc_ui_state_t *out)
{
    switch (phase) {
    case STC_PH_IDLE:
        *out = STC_UI_IDLE;
        return true;
    case STC_PH_PROMPT:
        *out = STC_UI_SCAN_READY;
        return true;
    case STC_PH_SCAN:
        *out = STC_UI_CAPTURING;
        return true;
    case STC_PH_RESULT:
    case STC_PH_OPEN:
        *out = STC_UI_RESULT_OK;
        return true;
    case STC_PH_WAIT_DROP:
        *out = STC_UI_BIN_CHOICE;
        return true;
    case STC_PH_SETTLE:
    case STC_PH_CLOSE:
        *out = STC_UI_LEARNING;
        return true;
    case STC_PH_LEARNING:
        *out = STC_UI_LEARNING;
        return true;
    case STC_PH_SUMMARY:
        *out = STC_UI_SUMMARY;
        return true;
    default:
        return false;
    }
}

static stc_ui_round_outcome_t map_outcome(stc_round_outcome_t outcome)
{
    switch (outcome) {
    case STC_ROUND_CORRECT: return STC_UI_ROUND_CORRECT;
    case STC_ROUND_WRONG: return STC_UI_ROUND_WRONG;
    case STC_ROUND_TIMEOUT: return STC_UI_ROUND_TIMEOUT;
    case STC_ROUND_ABSTAIN: return STC_UI_ROUND_ABSTAIN;
    case STC_ROUND_NO_SCAN: return STC_UI_ROUND_NO_SCAN;
    default: return STC_UI_ROUND_NONE;
    }
}

bool stc_app_payload_from_state(const stc_snapshot_t *snap,
                                const stc_game_view_t *view,
                                stc_ui_payload_t *out)
{
    if (!snap || !view || !out) {
        return false;
    }
    memset(out, 0, sizeof *out);

    stc_ui_state_t state;
    if (!map_phase(snap->phase, &state)) {
        return false;
    }
    if (view->fault_code) {
        state = STC_UI_ERROR;
    }
    out->state = state;
    out->category_id = (snap->truth == STC_BIN_COUNT) ? -1 : (int32_t)snap->truth;
    out->confidence = snap->conf;
    out->candidates[0].category_id = out->category_id;
    out->candidates[0].p = snap->conf;
    out->score = (snap->score > 0) ? (uint32_t)snap->score : 0u;
    out->streak = snap->streak;
    out->lid_open = view->lid_open;
    out->error_code = view->fault_code;
    out->round = (uint8_t)(snap->round + 1u);
    out->rounds_total = snap->rounds_total;
    out->debug_flags = view->debug_flags;
    out->learning.scanned_bin =
        snap->truth == STC_BIN_COUNT ? -1 : (int32_t)snap->truth;
    out->learning.dropped_bin =
        snap->dropped == STC_BIN_COUNT ? -1 : (int32_t)snap->dropped;
    out->learning.correct_bin =
        snap->truth == STC_BIN_COUNT ? -1 : (int32_t)snap->truth;
    out->learning.outcome = map_outcome(snap->last_outcome);
    out->learning.round_delta = snap->round_delta;
    return true;
}

bool stc_app_payload_from_snapshot(const stc_snapshot_t *snap,
                                   stc_ui_payload_t *out)
{
    if (!snap) {
        return false;
    }
    const stc_game_view_t view = {.lid_open = snap->lid_open};
    return stc_app_payload_from_state(snap, &view, out);
}
