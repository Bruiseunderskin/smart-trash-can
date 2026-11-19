#include "stc_game_touch_profile.h"

stc_game_config_t stc_game_touch_bin_config(stc_game_config_t base)
{
    base.scan_timeout_ms = STC_GAME_TOUCH_SCAN_TIMEOUT_MS;
    base.result_ms = 600;
    base.open_timeout_ms = 200;
    base.wait_drop_ms = 30000;
    base.settle_ms = 200;
    base.close_timeout_ms = 0;
    return base;
}
