/* stc_frame_source.h — 幀來源介面（#53 I2）
 * real（S31 相機）／replay（host 重播檔案）／stub 共用同一組呼叫。 */
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STC_FRAME_BYTES (640 * 480 * 2)

typedef struct {
    const char *name;
    void *ctx;
    stc_err_t (*open)(void *ctx, uint32_t timeout_ms);
    /* 取一幀 UYVY；沒有更多幀回 STC_ERR_IO */
    stc_err_t (*capture)(void *ctx, uint8_t *out_uyvy, size_t capacity, uint32_t *t_ms);
    void (*close)(void *ctx);
} stc_frame_source_t;

/* host 重播：由 dir 讀 *.uyvy，按檔名排序循環 */
const stc_frame_source_t *stc_frame_source_replay(const char *dir);
/* S31 相機：I6 才實作，現時一律 STC_ERR_UNSUPPORTED */
const stc_frame_source_t *stc_frame_source_esp(void);

#ifdef __cplusplus
}
#endif
