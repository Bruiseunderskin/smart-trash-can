/* One-shot, thread-safe GAME/CHAT requests for the outer shell. */
#pragma once

#include <stdbool.h>

#include "stc_mode.h"

#ifdef __cplusplus
extern "C" {
#endif

void stc_mode_request_init(void);
stc_err_t stc_mode_request_post(stc_mode_t mode);
bool stc_mode_request_take(stc_mode_t *mode_out);

#ifdef __cplusplus
}
#endif
