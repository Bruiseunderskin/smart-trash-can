/* stc_bin.h — 手部：舵機／ToF／ESP-NOW（contract v1.0.0） */
#pragma once
#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

stc_err_t stc_bin_init(void);
stc_err_t stc_bin_open_all(void);          /* 四蓋錯開開啟 */
stc_err_t stc_bin_close_all(void);
stc_err_t stc_bin_start_monitor(uint16_t timeout_ms);
stc_err_t stc_bin_stop_monitor(void);

/* 1 = 有事件（填 ev），0 = 無事件，負 = 錯誤 */
int stc_bin_poll(stc_drop_t *ev);

uint16_t stc_bin_distance(stc_bin_t bin);  /* mm，0 = 無效 */

#ifdef __cplusplus
}
#endif
