#include <stdio.h>

#include "stc_game_touch_profile.h"

int main(void)
{
    const stc_game_config_t base = {
        .rounds_total = 5,
        .total_time_ms = 180000,
        .prompt_timeout_ms = 20000,
        .scan_timeout_ms = 1500,
        .result_ms = 1500,
        .open_timeout_ms = 1500,
        .wait_drop_ms = 15000,
        .settle_ms = 300,
        .close_timeout_ms = 1500,
        .learning_ms = 6000,
        .score_correct = 10,
        .score_wrong = -5,
        .score_floor = 0,
        .max_retry = 1,
    };
    const stc_game_config_t touch = stc_game_touch_bin_config(base);
    if (touch.scan_timeout_ms != 5000 ||
        touch.result_ms != 600 ||
        touch.open_timeout_ms != 200 ||
        touch.wait_drop_ms != 30000 ||
        touch.settle_ms != 200 ||
        touch.close_timeout_ms != 0) {
        fprintf(stderr, "touch profile timing mismatch\n");
        return 1;
    }
    if (base.scan_timeout_ms != 1500) {
        fprintf(stderr, "base profile was mutated\n");
        return 1;
    }
    puts("test_game_touch_profile: five-second scan budget OK");
    return 0;
}
