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

static bool phase_time_window(stc_phase_t phase,
                              const stc_game_config_t *cfg,
                              uint32_t *before_ms,
                              uint32_t *total_ms)
{
    *before_ms = 0;
    *total_ms = 0;
    switch (phase) {
    case STC_PH_PROMPT:
        /* S31 shows this phase for the five-second auto-scan wait. */
        *total_ms = 5000u;
        return true;
    case STC_PH_SCAN:
        *total_ms = cfg->scan_timeout_ms;
        return true;
    case STC_PH_RESULT:
        *total_ms = (uint32_t)cfg->result_ms + cfg->open_timeout_ms;
        return true;
    case STC_PH_OPEN:
        *before_ms = cfg->result_ms;
        *total_ms = (uint32_t)cfg->result_ms + cfg->open_timeout_ms;
        return true;
    case STC_PH_WAIT_DROP:
        *total_ms = cfg->wait_drop_ms;
        return true;
    case STC_PH_SETTLE:
        *total_ms = (uint32_t)cfg->settle_ms + cfg->close_timeout_ms +
                    cfg->learning_ms;
        return true;
    case STC_PH_CLOSE:
        *before_ms = cfg->settle_ms;
        *total_ms = (uint32_t)cfg->settle_ms + cfg->close_timeout_ms +
                    cfg->learning_ms;
        return true;
    case STC_PH_LEARNING:
        *before_ms = (uint32_t)cfg->settle_ms + cfg->close_timeout_ms;
        *total_ms = (uint32_t)cfg->settle_ms + cfg->close_timeout_ms +
                    cfg->learning_ms;
        return true;
    default:
        return false;
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
    const stc_game_config_t cfg = stc_game_get_config();
    uint32_t time_before = 0;
    if (phase_time_window(snap->phase, &cfg, &time_before,
                          &out->time_total_ms)) {
        const uint32_t elapsed = time_before + snap->phase_elapsed_ms;
        out->time_left_ms =
            elapsed >= out->time_total_ms ? 0u
                                          : out->time_total_ms - elapsed;
    }
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
