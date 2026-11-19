#include "stc_tof_drop.h"

#include <string.h>

static uint16_t distance_delta(uint16_t a, uint16_t b)
{
    return a > b ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static uint32_t candidate_score(const stc_tof_baseline_t *baseline,
                                const stc_tof_sample_t *sample)
{
    if (sample->status != 0) {
        return 0;
    }

    uint32_t score = 0;
    if (baseline->return_rate >= 100) {
        const uint32_t scaled =
            (uint32_t)sample->return_rate * 100u;
        if (scaled >=
            (uint32_t)baseline->return_rate * STC_TOF_HIGH_RETURN_PERCENT) {
            score = scaled / baseline->return_rate;
        }
        if (sample->return_rate >= 10 &&
            scaled <=
                (uint32_t)baseline->return_rate * STC_TOF_LOW_RETURN_PERCENT) {
            const uint32_t inverse =
                (uint32_t)baseline->return_rate * 100u /
                sample->return_rate;
            if (inverse > score) {
                score = inverse;
            }
        }
    }

    const uint16_t delta =
        distance_delta(sample->range_mm, baseline->range_mm);
    if (delta >= STC_TOF_DISTANCE_DELTA_MM) {
        const uint32_t distance_score = 100u + (uint32_t)delta * 5u;
        if (distance_score > score) {
            score = distance_score;
        }
    }
    return score;
}

stc_err_t stc_tof_drop_init(
    stc_tof_drop_detector_t *detector,
    const stc_tof_baseline_t baseline[STC_TOF_SENSOR_COUNT])
{
    if (!detector || !baseline) {
        return STC_ERR_CONTRACT;
    }
    memset(detector, 0, sizeof *detector);
    memcpy(detector->baseline, baseline, sizeof detector->baseline);
    detector->candidate = 0xFF;
    return STC_OK;
}

bool stc_tof_drop_update(
    stc_tof_drop_detector_t *detector,
    const stc_tof_sample_t samples[STC_TOF_SENSOR_COUNT],
    stc_tof_drop_event_t *event)
{
    if (!detector || !samples || detector->detected) {
        return false;
    }

    uint8_t best_sensor = 0xFF;
    uint32_t best_score = 0;
    for (uint8_t i = 0; i < STC_TOF_SENSOR_COUNT; ++i) {
        const uint32_t score = candidate_score(&detector->baseline[i],
                                               &samples[i]);
        if (score > best_score) {
            best_score = score;
            best_sensor = i;
        }
    }

    if (best_sensor == 0xFF) {
        detector->candidate = 0xFF;
        detector->streak = 0;
        return false;
    }
    if (best_sensor == detector->candidate) {
        if (detector->streak < UINT8_MAX) {
            detector->streak++;
        }
    } else {
        detector->candidate = best_sensor;
        detector->streak = 1;
    }
    if (detector->streak < STC_TOF_CONFIRM_SAMPLES) {
        return false;
    }

    detector->detected = true;
    if (event) {
        memset(event, 0, sizeof *event);
        event->sensor_index = best_sensor;
        event->streak = detector->streak;
        event->range_mm = samples[best_sensor].range_mm;
        event->return_rate = samples[best_sensor].return_rate;
        event->baseline_range_mm =
            detector->baseline[best_sensor].range_mm;
        event->baseline_return_rate =
            detector->baseline[best_sensor].return_rate;
    }
    return true;
}
