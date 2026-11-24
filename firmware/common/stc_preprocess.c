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

static uint8_t clamp_i32(int32_t value)
{
    if (value < 0) return 0u;
    if (value > 255) return 255u;
    return (uint8_t)value;
}

/* Preview-only BT.601 conversion. Fixed point avoids the soft-float cost that
 * dominates a full-frame live preview on S31. */
static void yuv_to_rgb_fast(uint8_t y, uint8_t u, uint8_t v, uint8_t *r,
                            uint8_t *g, uint8_t *b)
{
    const int32_t c = (int32_t)y - 16;
    const int32_t d = (int32_t)u - 128;
    const int32_t e = (int32_t)v - 128;
    *r = clamp_i32((298 * c + 409 * e + 128) >> 8);
    *g = clamp_i32((298 * c - 100 * d - 208 * e + 128) >> 8);
    *b = clamp_i32((298 * c + 516 * d + 128) >> 8);
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

static void uyvy_pixel_to_yuv(const uint8_t *row, int x, uint8_t *y,
                              uint8_t *u, uint8_t *v)
{
    const uint8_t *pair = row + (size_t)(x & ~1) * 2u;
    *y = (x & 1) ? pair[3] : pair[1];
    *u = pair[0];
    *v = pair[2];
}

stc_err_t stc_preprocess_uyvy_to_rgb_preview(const uint8_t *uyvy, int width,
                                             int height, uint8_t *rgb_out,
                                             int out_size)
{
    if (!uyvy || !rgb_out || width <= 0 || height <= 0 || out_size <= 0) {
        return STC_ERR_CONTRACT;
    }
    if ((width % 2) != 0) {
        return STC_ERR_CONTRACT;
    }

    const int side = (width < height) ? width : height;
    const int ox = (width - side) / 2;
    const int oy = (height - side) / 2;
    const float scale = (float)side / (float)out_size;

    /* The live preview normally runs 480 -> 240. Use a 2x2 box filter for
     * integer ratios: it is cheaper than four floating-point samples and
     * avoids the shimmer caused by dropping every other source pixel. */
    if ((side % out_size) == 0) {
        const int step = side / out_size;
        const int samples = step * step;
        for (int y = 0; y < out_size; ++y) {
            const int sy = oy + y * step;
            for (int x = 0; x < out_size; ++x) {
                const int sx = ox + x * step;
                uint32_t y_sum = 0;
                uint32_t u_sum = 0;
                uint32_t v_sum = 0;
                for (int dy = 0; dy < step; ++dy) {
                    const uint8_t *row =
                        uyvy +
                        (size_t)(sy + dy) * (size_t)width * 2u;
                    for (int dx = 0; dx < step; ++dx) {
                        uint8_t syc, uc, vc;
                        uyvy_pixel_to_yuv(row, sx + dx, &syc, &uc, &vc);
                        y_sum += syc;
                        u_sum += uc;
                        v_sum += vc;
                    }
                }
                uint8_t *out =
                    rgb_out +
                    ((size_t)y * (size_t)out_size + (size_t)x) * 3u;
                yuv_to_rgb_fast((uint8_t)(y_sum / samples),
                                (uint8_t)(u_sum / samples),
                                (uint8_t)(v_sum / samples), out, out + 1,
                                out + 2);
            }
        }
        return STC_OK;
    }

    for (int y = 0; y < out_size; ++y) {
        const float sy = (float)y * scale;
        const int y0 = (int)sy;
        const int y1 = (y0 + 1 < side) ? y0 + 1 : side - 1;
        const float fy = sy - (float)y0;
        const uint8_t *row0 =
            uyvy + (size_t)(oy + y0) * (size_t)width * 2u;
        const uint8_t *row1 =
            uyvy + (size_t)(oy + y1) * (size_t)width * 2u;

        for (int x = 0; x < out_size; ++x) {
            const float sx = (float)x * scale;
            const int x0 = (int)sx;
            const int x1 = (x0 + 1 < side) ? x0 + 1 : side - 1;
            const float fx = sx - (float)x0;
            uint8_t y00, u00, v00;
            uint8_t y01, u01, v01;
            uint8_t y10, u10, v10;
            uint8_t y11, u11, v11;
            uyvy_pixel_to_yuv(row0, ox + x0, &y00, &u00, &v00);
            uyvy_pixel_to_yuv(row0, ox + x1, &y01, &u01, &v01);
            uyvy_pixel_to_yuv(row1, ox + x0, &y10, &u10, &v10);
            uyvy_pixel_to_yuv(row1, ox + x1, &y11, &u11, &v11);

            const float y0f =
                (float)y00 * (1.0f - fx) + (float)y01 * fx;
            const float y1f =
                (float)y10 * (1.0f - fx) + (float)y11 * fx;
            const float u0f =
                (float)u00 * (1.0f - fx) + (float)u01 * fx;
            const float u1f =
                (float)u10 * (1.0f - fx) + (float)u11 * fx;
            const float v0f =
                (float)v00 * (1.0f - fx) + (float)v01 * fx;
            const float v1f =
                (float)v10 * (1.0f - fx) + (float)v11 * fx;
            const uint8_t yf = clamp_u8(
                y0f * (1.0f - fy) + y1f * fy);
            const uint8_t uf = clamp_u8(
                u0f * (1.0f - fy) + u1f * fy);
            const uint8_t vf = clamp_u8(
                v0f * (1.0f - fy) + v1f * fy);

            uint8_t *out =
                rgb_out + ((size_t)y * (size_t)out_size + (size_t)x) * 3u;
            yuv_to_rgb_fast(yf, uf, vf, out, out + 1, out + 2);
        }
    }
    return STC_OK;
}

stc_err_t stc_preprocess_rgb_to_rgb565(const uint8_t *rgb, int pixels,
                                      uint16_t *rgb565_out)
{
    if (!rgb || !rgb565_out || pixels <= 0) {
        return STC_ERR_CONTRACT;
    }
    for (int i = 0; i < pixels; ++i) {
        const uint8_t r = rgb[i * 3 + 0];
        const uint8_t g = rgb[i * 3 + 1];
        const uint8_t b = rgb[i * 3 + 2];
        rgb565_out[i] = (uint16_t)(((uint16_t)(r >> 3) << 11) |
                                  ((uint16_t)(g >> 2) << 5) |
                                  (uint16_t)(b >> 3));
    }
    return STC_OK;
}
