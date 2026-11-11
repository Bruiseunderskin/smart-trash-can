/* stc_lid.h — 蓋控制介面（#53 I5／I7）
 * 高層只說「開邊個蓋／全關／等事件」，CDC 細節在實作內。 */
#pragma once
#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    stc_err_t (*open_bin)(stc_bin_t bin);
    stc_err_t (*close_all)(void);
    /* 1 = 有事件、0 = 無、負 = 錯誤（與 stc_bin_poll 同語義） */
    int       (*poll)(stc_drop_t *ev);
    void      (*tick)(uint32_t now_ms);
    uint16_t  (*distance_mm)(stc_bin_t bin);
    const char *name;
} stc_lid_ops_t;

const stc_lid_ops_t *stc_lid_loopback(void);   /* host 用：記命令、產生假 DROP */
const stc_lid_ops_t *stc_lid_stub(void);       /* 一律 UNSUPPORTED */

/* 測試輔助：注入一個假投放事件（只對 loopback 有效） */
void stc_lid_loopback_inject_drop(stc_bin_t bin, uint16_t dist_mm);

#ifdef __cplusplus
}
#endif
