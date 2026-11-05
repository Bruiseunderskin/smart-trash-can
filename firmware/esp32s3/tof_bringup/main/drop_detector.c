#include "drop_detector.h"

#define STC_DROP_TRIGGER_MM 25
#define STC_DROP_RECOVERY_MM 15
#define STC_DROP_CANDIDATE_SAMPLES 3
#define STC_DROP_CANDIDATE_MIN_MS 60
#define STC_DROP_RECOVERY_WINDOW_MS 500
#define STC_DROP_IGNORE_AFTER_OPEN_MS 300
#define STC_DROP_BASELINE_REFRESH_MS 2000

static void reset_candidate(stc_drop_detector_t *detector, size_t bin) {
    detector->candidate_count[bin] = 0;
    detector->candidate_start_ms[bin] = 0;
    detector->last_near_ms[bin] = 0;
    detector->candidate_min_mm[bin] = 0;
    detector->waiting_recovery[bin] = false;
}

static uint16_t median_mm(const uint16_t *values, size_t count) {
    uint16_t sorted[STC_DROP_HISTORY_SIZE];
    for (size_t i = 0; i < count; ++i) {
        uint16_t value = values[i];
        size_t position = i;
        while (position > 0 && sorted[position - 1] > value) {
            sorted[position] = sorted[position - 1];
            --position;
        }
        sorted[position] = value;
    }
    if (count % 2U) {
        return sorted[count / 2U];
    }
    return (uint16_t)(((uint32_t)sorted[count / 2U - 1U] +
                       sorted[count / 2U]) /
                      2U);
}

static void observe_baseline(stc_drop_detector_t *detector, size_t bin,
                             uint16_t distance_mm, uint32_t now_ms) {
    stc_drop_detector_t *state = detector;
    if (state->baseline_mm[bin] != 0 &&
        distance_mm + STC_DROP_TRIGGER_MM <= state->baseline_mm[bin]) {
        return;
    }

    const size_t index = state->history_index[bin];
    state->history_mm[bin][index] = distance_mm;
    state->history_index[bin] =
        (uint8_t)((index + 1U) % STC_DROP_HISTORY_SIZE);
    if (state->history_count[bin] < STC_DROP_HISTORY_SIZE) {
        ++state->history_count[bin];
    }

    const bool first_baseline = state->baseline_mm[bin] == 0;
    const bool refresh_due =
        now_ms - state->baseline_update_ms[bin] >= STC_DROP_BASELINE_REFRESH_MS;
    if (first_baseline || refresh_due) {
        state->baseline_mm[bin] =
            median_mm(state->history_mm[bin], state->history_count[bin]);
        state->baseline_update_ms[bin] = now_ms;
    }
}

static bool recovered_to_baseline(const stc_drop_detector_t *detector,
                                  size_t bin, uint16_t distance_mm) {
    const int32_t delta =
        (int32_t)distance_mm - (int32_t)detector->baseline_mm[bin];
    return delta >= -STC_DROP_RECOVERY_MM &&
           delta <= STC_DROP_RECOVERY_MM;
}

static bool is_near_candidate(const stc_drop_detector_t *detector, size_t bin,
                              uint16_t distance_mm) {
    return detector->baseline_mm[bin] != 0 &&
           distance_mm + STC_DROP_TRIGGER_MM <= detector->baseline_mm[bin];
}

void stc_drop_detector_reset(stc_drop_detector_t *detector) {
    if (detector) {
        *detector = (stc_drop_detector_t){0};
    }
}

void stc_drop_detector_set_monitor(stc_drop_detector_t *detector, bool enabled,
                                   uint32_t now_ms) {
    if (!detector) {
        return;
    }
    detector->monitor_enabled = enabled;
    detector->event_emitted = false;
    detector->monitor_start_ms = now_ms;
    for (size_t bin = 0; bin < STC_DROP_BIN_COUNT; ++bin) {
        reset_candidate(detector, bin);
    }
}

int stc_drop_detector_feed(stc_drop_detector_t *detector,
                           const uint16_t distance_mm[STC_DROP_BIN_COUNT],
                           const bool valid[STC_DROP_BIN_COUNT], uint32_t now_ms,
                           stc_drop_t *event) {
    if (!detector || !distance_mm || !valid) {
        return -1;
    }
    if (event) {
        *event = (stc_drop_t){0};
    }

    uint16_t best_drop_mm = 0;
    uint16_t best_candidate_mm = 0;
    size_t best_bin = 0;
    bool best_found = false;
    const bool detection_allowed =
        detector->monitor_enabled && !detector->event_emitted &&
        now_ms - detector->monitor_start_ms >= STC_DROP_IGNORE_AFTER_OPEN_MS;

    for (size_t bin = 0; bin < STC_DROP_BIN_COUNT; ++bin) {
        if (!valid[bin] || distance_mm[bin] == 0) {
            continue;
        }

        observe_baseline(detector, bin, distance_mm[bin], now_ms);
        if (!detection_allowed) {
            continue;
        }

        if (detector->waiting_recovery[bin]) {
            if (now_ms - detector->last_near_ms[bin] >
                STC_DROP_RECOVERY_WINDOW_MS) {
                reset_candidate(detector, bin);
                continue;
            }
            if (is_near_candidate(detector, bin, distance_mm[bin])) {
                detector->last_near_ms[bin] = now_ms;
                if (distance_mm[bin] < detector->candidate_min_mm[bin]) {
                    detector->candidate_min_mm[bin] = distance_mm[bin];
                }
                continue;
            }
            if (recovered_to_baseline(detector, bin, distance_mm[bin])) {
                const uint16_t drop_mm =
                    (uint16_t)(detector->baseline_mm[bin] -
                               detector->candidate_min_mm[bin]);
                if (!best_found || drop_mm > best_drop_mm) {
                    best_found = true;
                    best_bin = bin;
                    best_drop_mm = drop_mm;
                    best_candidate_mm = detector->candidate_min_mm[bin];
                }
                reset_candidate(detector, bin);
            }
            continue;
        }

        if (!is_near_candidate(detector, bin, distance_mm[bin])) {
            reset_candidate(detector, bin);
            continue;
        }

        if (detector->candidate_count[bin] == 0) {
            detector->candidate_start_ms[bin] = now_ms;
            detector->candidate_min_mm[bin] = distance_mm[bin];
        } else if (distance_mm[bin] < detector->candidate_min_mm[bin]) {
            detector->candidate_min_mm[bin] = distance_mm[bin];
        }
        detector->last_near_ms[bin] = now_ms;
        if (detector->candidate_count[bin] < UINT8_MAX) {
            ++detector->candidate_count[bin];
        }
        if (detector->candidate_count[bin] >= STC_DROP_CANDIDATE_SAMPLES &&
            now_ms - detector->candidate_start_ms[bin] >=
                STC_DROP_CANDIDATE_MIN_MS) {
            detector->waiting_recovery[bin] = true;
        }
    }

    if (!best_found) {
        return 0;
    }
    detector->event_emitted = true;
    if (event) {
        event->bin = (stc_bin_t)best_bin;
        event->dist_mm = best_candidate_mm;
        event->t_ms = now_ms;
    }
    for (size_t bin = 0; bin < STC_DROP_BIN_COUNT; ++bin) {
        reset_candidate(detector, bin);
    }
    return 1;
}
