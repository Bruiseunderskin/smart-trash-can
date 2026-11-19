#include <stdio.h>

#include "stc_tof_drop.h"

static void stable(stc_tof_sample_t samples[STC_TOF_SENSOR_COUNT])
{
    for (int i = 0; i < STC_TOF_SENSOR_COUNT; ++i) {
        samples[i].status = 0;
    }
    samples[0].range_mm = 14;
    samples[0].return_rate = 2600;
    samples[1].range_mm = 18;
    samples[1].return_rate = 4300;
    samples[2].range_mm = 11;
    samples[2].return_rate = 2000;
    samples[3].range_mm = 19;
    samples[3].return_rate = 2800;
}

int main(void)
{
    const stc_tof_baseline_t baseline[STC_TOF_SENSOR_COUNT] = {
        {14, 2600}, {18, 4300}, {11, 2000}, {19, 2800},
    };
    stc_tof_drop_detector_t detector;
    stc_tof_drop_event_t event;
    stc_tof_sample_t samples[STC_TOF_SENSOR_COUNT];

    if (stc_tof_drop_init(&detector, baseline) != STC_OK) return 1;
    stable(samples);
    for (int i = 0; i < 10; ++i) {
        if (stc_tof_drop_update(&detector, samples, &event)) return 1;
    }

    samples[2].return_rate = 4000;
    if (stc_tof_drop_update(&detector, samples, &event)) return 1;
    if (stc_tof_drop_update(&detector, samples, &event)) return 1;
    if (!stc_tof_drop_update(&detector, samples, &event)) return 1;
    if (event.sensor_index != 2 || event.streak != 3) {
        fprintf(stderr, "wrong event sensor=%u streak=%u\n",
                event.sensor_index, event.streak);
        return 1;
    }

    if (stc_tof_drop_update(&detector, samples, &event)) {
        fprintf(stderr, "detector did not latch\n");
        return 1;
    }
    puts("test_tof_drop: three-sample confirmation and latch OK");
    return 0;
}
