#include "stc_preprocess.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint8_t clamp_u8(float v)
{
    if (v < 0.0f) return 0u;
    if (v > 255.0f) return 255u;
    return (uint8_t)(v + 0.5f);
}

/* BT.601 limited range: Y 16–235, C 16–240 */
static void yuv_to_rgb(uint8_t y, uint8_t u, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    const float yf = (float)(y - 16) * 1.164f;
    const float uf = (float)u - 128.0f;
    const float vf = (float)v - 128.0f;
    *r = clamp_u8(yf + 1.596f * vf);
    *g = clamp_u8(yf - 0.392f * uf - 0.813f * vf);
    *b = clamp_u8(yf + 2.017f * uf);
}

/* 1) UYVY → RGB888 全幅
 * 2) 中央正方形裁切（不用 letterbox）
 * 3) bilinear resize → out_size
 */
stc_err_t stc_preprocess_uyvy_to_rgb(const uint8_t *uyvy, int width, int height,
                                     uint8_t *rgb_out, int out_size)
{
    if (!uyvy || !rgb_out || width <= 0 || height <= 0 || out_size <= 0) {
        return STC_ERR_CONTRACT;
    }
    if ((width % 2) != 0) {
        return STC_ERR_CONTRACT;   /* UYVY 以 2 像素為單位 */
    }

    const size_t pixels = (size_t)width * (size_t)height;
    uint8_t *rgb = (uint8_t *)malloc(pixels * 3u);
    if (!rgb) {
        return STC_ERR_IO;
    }

    for (int row = 0; row < height; ++row) {
        const uint8_t *src = uyvy + (size_t)row * (size_t)width * 2u;
        uint8_t *dst = rgb + (size_t)row * (size_t)width * 3u;
        for (int x = 0; x < width; x += 2) {
            const uint8_t u = src[0], y0 = src[1], v = src[2], y1 = src[3];
            src += 4;
            yuv_to_rgb(y0, u, v, dst + 0, dst + 1, dst + 2);
            yuv_to_rgb(y1, u, v, dst + 3, dst + 4, dst + 5);
            dst += 6;
        }
    }

    const int side = (width < height) ? width : height;
    const int ox = (width - side) / 2;
    const int oy = (height - side) / 2;

    const float scale = (float)side / (float)out_size;
    for (int y = 0; y < out_size; ++y) {
        float sy = (float)y * scale;
        int y0 = (int)sy;
        int y1 = (y0 + 1 < side) ? y0 + 1 : side - 1;
        float fy = sy - (float)y0;
        for (int x = 0; x < out_size; ++x) {
            float sx = (float)x * scale;
            int x0 = (int)sx;
            int x1 = (x0 + 1 < side) ? x0 + 1 : side - 1;
            float fx = sx - (float)x0;

            const uint8_t *p00 = rgb + ((size_t)(oy + y0) * (size_t)width + (size_t)(ox + x0)) * 3u;
            const uint8_t *p01 = rgb + ((size_t)(oy + y0) * (size_t)width + (size_t)(ox + x1)) * 3u;
            const uint8_t *p10 = rgb + ((size_t)(oy + y1) * (size_t)width + (size_t)(ox + x0)) * 3u;
            const uint8_t *p11 = rgb + ((size_t)(oy + y1) * (size_t)width + (size_t)(ox + x1)) * 3u;

            uint8_t *out = rgb_out + ((size_t)y * (size_t)out_size + (size_t)x) * 3u;
            for (int c = 0; c < 3; ++c) {
                const float top = (float)p00[c] * (1.0f - fx) + (float)p01[c] * fx;
                const float bottom = (float)p10[c] * (1.0f - fx) + (float)p11[c] * fx;
                out[c] = clamp_u8(top * (1.0f - fy) + bottom * fy);
            }
        }
    }

    free(rgb);
    return STC_OK;
}
