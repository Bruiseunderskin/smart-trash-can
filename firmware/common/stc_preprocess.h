/* stc_preprocess.h — UYVY → RGB → center crop → bilinear 160（#53 §6.2） */
#pragma once
#include <stdint.h>

#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STC_FRAME_W 640
#define STC_FRAME_H 480
#define STC_INPUT_SIZE 160

/* 輸入：UYVY（YUV422，每 2 像素 4 bytes），BT.601 有限範圍。
 * 輸出：RGB888，out_size × out_size，uint8 0–255，NHWC 佈局。 */
stc_err_t stc_preprocess_uyvy_to_rgb(const uint8_t *uyvy, int width, int height,
                                     uint8_t *rgb_out, int out_size);

#ifdef __cplusplus
}
#endif
