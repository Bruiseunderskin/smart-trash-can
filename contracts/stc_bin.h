/* stc_bin.h — 手部：舵機／ToF／ESP-NOW（contract v1.2.0）
 *
 * 感測器實作：4 × TOF050C（VL6180X）經 TCA9548A（0x70）分 CH0–CH3，
 * 四粒全部停留在預設地址 0x29；切 channel 即選中對應桶。
 * 通道 i 對應 stc_bin_t = i（0 可回收物 … 3 其他垃圾）。
 *
 * 本模組不得改分數；失敗時回錯誤並由 stc_game 決定如何處理。
 */
#pragma once
#include "stc_types.h"

#define STC_BIN_CONTRACT_VERSION_MAJOR 1
#define STC_BIN_CONTRACT_VERSION_MINOR 2
#define STC_BIN_CONTRACT_VERSION_PATCH 0
#define STC_BIN_CONTRACT_VERSION_STRING "1.2.0"

#ifdef __cplusplus
extern "C" {
#endif

stc_err_t stc_bin_init(void);          /* mux 初始化、四粒 init、載入 NVS 校正值 */
stc_err_t stc_bin_selftest(void);      /* 四粒各讀一次，回第一個失敗碼；全通回 STC_OK */
uint8_t   stc_bin_fault_mask(void);    /* bit i = 1 → bin i 故障（位元對應 stc_bin_t） */

stc_err_t stc_bin_open_all(void);      /* 四蓋錯開開啟（80–120 ms 間隔） */
stc_err_t stc_bin_close_all(void);

stc_err_t stc_bin_start_monitor(uint16_t timeout_ms);
stc_err_t stc_bin_stop_monitor(void);

/* 1 = 有事件（填 ev），0 = 無事件，負 = 錯誤 */
int stc_bin_poll(stc_drop_t *ev);

uint16_t stc_bin_distance(stc_bin_t bin);  /* mm，0 = 無效 */

#ifdef __cplusplus
}
#endif
