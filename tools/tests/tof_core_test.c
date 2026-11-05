#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "calibration.h"
#include "drop_detector.h"

static int failures;

#define CHECK(expression)                                                        \
    do {                                                                         \
        if (!(expression)) {                                                     \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__,     \
                    #expression);                                                \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

static void feed(stc_drop_detector_t *detector, uint16_t bin0, uint16_t bin1,
                 uint16_t bin2, uint16_t bin3, uint32_t now_ms,
                 stc_drop_t *event) {
    const uint16_t distances[STC_DROP_BIN_COUNT] = {bin0, bin1, bin2, bin3};
    const bool valid[STC_DROP_BIN_COUNT] = {true, true, true, true};
    const int result =
        stc_drop_detector_feed(detector, distances, valid, now_ms, event);
    CHECK(result == 0 || result == 1);
}

static void establish_baseline(stc_drop_detector_t *detector) {
    stc_drop_t event = {0};
    for (uint32_t i = 0; i < STC_DROP_HISTORY_SIZE; ++i) {
        feed(detector, 100, 100, 100, 100, i * 10U, &event);
    }
}

static void test_three_samples_and_recovery_emit_one_event(void) {
    stc_drop_detector_t detector;
    stc_drop_detector_reset(&detector);
    establish_baseline(&detector);
    stc_drop_detector_set_monitor(&detector, true, 200);

    stc_drop_t event = {0};
    feed(&detector, 100, 100, 100, 100, 500, &event);
    feed(&detector, 70, 100, 100, 100, 530, &event);
    feed(&detector, 68, 100, 100, 100, 560, &event);
    feed(&detector, 65, 100, 100, 100, 590, &event);
    feed(&detector, 100, 100, 100, 100, 620, &event);
    CHECK(event.bin == STC_BIN_RECYCLABLE);
    CHECK(event.dist_mm == 65);
    CHECK(event.t_ms == 620);

    event = (stc_drop_t){0};
    feed(&detector, 60, 100, 100, 100, 650, &event);
    feed(&detector, 60, 100, 100, 100, 680, &event);
    feed(&detector, 60, 100, 100, 100, 710, &event);
    feed(&detector, 100, 100, 100, 100, 740, &event);
    CHECK(event.bin == STC_BIN_RECYCLABLE);
    CHECK(event.dist_mm == 0); /* one event per monitor arm */
}

static void test_two_samples_are_not_enough(void) {
    stc_drop_detector_t detector;
    stc_drop_detector_reset(&detector);
    establish_baseline(&detector);
    stc_drop_detector_set_monitor(&detector, true, 200);

    stc_drop_t event = {0};
    feed(&detector, 70, 100, 100, 100, 500, &event);
    feed(&detector, 68, 100, 100, 100, 530, &event);
    feed(&detector, 100, 100, 100, 100, 560, &event);
    CHECK(event.dist_mm == 0);
}

static void test_events_are_ignored_for_first_300ms(void) {
    stc_drop_detector_t detector;
    stc_drop_detector_reset(&detector);
    establish_baseline(&detector);
    stc_drop_detector_set_monitor(&detector, true, 200);

    stc_drop_t event = {0};
    feed(&detector, 60, 100, 100, 100, 210, &event);
    feed(&detector, 60, 100, 100, 100, 260, &event);
    feed(&detector, 60, 100, 100, 100, 310, &event);
    feed(&detector, 100, 100, 100, 100, 340, &event);
    CHECK(event.dist_mm == 0);
}

static void test_slow_recovery_expires(void) {
    stc_drop_detector_t detector;
    stc_drop_detector_reset(&detector);
    establish_baseline(&detector);
    stc_drop_detector_set_monitor(&detector, true, 200);

    stc_drop_t event = {0};
    feed(&detector, 70, 100, 100, 100, 500, &event);
    feed(&detector, 68, 100, 100, 100, 530, &event);
    feed(&detector, 65, 100, 100, 100, 560, &event);
    feed(&detector, 70, 100, 100, 100, 1061, &event);
    feed(&detector, 100, 100, 100, 100, 1090, &event);
    CHECK(event.dist_mm == 0);
}

static void test_largest_simultaneous_drop_wins(void) {
    stc_drop_detector_t detector;
    stc_drop_detector_reset(&detector);
    establish_baseline(&detector);
    stc_drop_detector_set_monitor(&detector, true, 200);

    stc_drop_t event = {0};
    feed(&detector, 70, 40, 100, 100, 500, &event);
    feed(&detector, 68, 40, 100, 100, 530, &event);
    feed(&detector, 65, 40, 100, 100, 560, &event);
    feed(&detector, 100, 100, 100, 100, 590, &event);
    CHECK(event.bin == STC_BIN_FOOD);
    CHECK(event.dist_mm == 40);
}

static void test_new_monitor_arm_rearms_detector(void) {
    stc_drop_detector_t detector;
    stc_drop_detector_reset(&detector);
    establish_baseline(&detector);
    stc_drop_detector_set_monitor(&detector, true, 200);

    stc_drop_t event = {0};
    feed(&detector, 70, 100, 100, 100, 500, &event);
    feed(&detector, 68, 100, 100, 100, 530, &event);
    feed(&detector, 65, 100, 100, 100, 560, &event);
    feed(&detector, 100, 100, 100, 100, 590, &event);
    CHECK(event.dist_mm == 65);

    stc_drop_detector_set_monitor(&detector, true, 1000);
    event = (stc_drop_t){0};
    feed(&detector, 65, 100, 100, 100, 1300, &event);
    feed(&detector, 65, 100, 100, 100, 1330, &event);
    feed(&detector, 65, 100, 100, 100, 1360, &event);
    feed(&detector, 100, 100, 100, 100, 1390, &event);
    CHECK(event.bin == STC_BIN_RECYCLABLE);
    CHECK(event.dist_mm == 65);
}

static void test_offset_calibration(void) {
    int8_t corrected = 0;
    CHECK(stc_offset_correct(3, 57, 50, &corrected));
    CHECK(corrected == 10);

    CHECK(stc_offset_correct(-60, 20, 50, &corrected));
    CHECK(corrected == -90);

    CHECK(stc_offset_correct(-120, 20, 50, &corrected));
    CHECK(corrected == -128);

    CHECK(stc_offset_correct(120, 200, 50, &corrected));
    CHECK(corrected == 127);

    CHECK(!stc_offset_correct(0, 0, 50, &corrected));
    CHECK(!stc_offset_correct(0, 50, 0, &corrected));
    CHECK(!stc_offset_correct(0, 50, 50, NULL));
}

int main(void) {
    test_three_samples_and_recovery_emit_one_event();
    test_two_samples_are_not_enough();
    test_events_are_ignored_for_first_300ms();
    test_slow_recovery_expires();
    test_largest_simultaneous_drop_wins();
    test_new_monitor_arm_rearms_detector();
    test_offset_calibration();
    if (failures) {
        fprintf(stderr, "%d TOF core checks failed\n", failures);
        return 1;
    }
    puts("TOF core checks passed");
    return 0;
}
