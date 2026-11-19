/* stc_game_touch_profile.h — timing profile for local touch-bin play. */
#pragma once

#include "stc_game.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STC_GAME_TOUCH_SCAN_TIMEOUT_MS 5000u

stc_game_config_t stc_game_touch_bin_config(stc_game_config_t base);

#ifdef __cplusplus
}
#endif
