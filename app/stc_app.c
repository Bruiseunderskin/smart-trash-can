#include "stc_app.h"

#include <string.h>

static bool map_phase(stc_phase_t phase, stc_snapshot_t snap, stc_ui_state_t *out)
{
    switch (phase) {
    case STC_PH_IDLE:
    case STC_PH_PROMPT:
        *out = STC_UI_IDLE;
        return true;
    case STC_PH_SCAN:
        *out = STC_UI_CAPTURING;
        return true;
    case STC_PH_RESULT:
        *out = snap.abstained ? STC_UI_RESULT_RETRY : STC_UI_CLASSIFYING;
        return true;
    case STC_PH_OPEN:
    case STC_PH_WAIT_DROP:
    case STC_PH_SETTLE:
    case STC_PH_CLOSE:
        *out = STC_UI_RESULT_OK;
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

bool stc_app_payload_from_snapshot(const stc_snapshot_t *snap, stc_ui_payload_t *out)
{
    if (!snap || !out) {
        return false;
    }
    memset(out, 0, sizeof *out);

    stc_ui_state_t state;
    if (!map_phase(snap->phase, *snap, &state)) {
        return false;
    }
    out->state = state;
    out->category_id = (snap->truth == STC_BIN_COUNT) ? -1 : (int32_t)snap->truth;
    out->confidence = snap->conf;
    out->candidates[0].category_id = out->category_id;
    out->candidates[0].p = snap->conf;
    out->score = (snap->score > 0) ? (uint32_t)snap->score : 0u;
    out->streak = snap->agreement;
    out->lid_open = snap->lid_open;
    out->error_code = NULL;
    return true;
}
