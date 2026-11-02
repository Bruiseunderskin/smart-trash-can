/*
 * SPDX-FileCopyrightText: 2026 Smart Trash Can contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "bsp/esp32_s31_korvo.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_jpeg_enc.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "linux/videodev2.h"

#define FRAME_COUNT         30
#define REQUIRED_VALID      28
#define JPEG_SAMPLE_COUNT   3
#define UYVY_PIXEL_BYTES    2
#define SERIAL_CAPTURE_BAUD 115200
#define JPEG_CAPTURE_QUALITY 80
#define JPEG_OUTPUT_CAPACITY (1024 * 1024)

static const char *TAG = "T2_CAMERA";
static bool s_sd_ready;
static const char *s_sd_mount_point;

static uint32_t pixel_format_fourcc(uint32_t pixelformat)
{
    return pixelformat;
}

static void fourcc_to_text(uint32_t fourcc, char text[5])
{
    text[0] = (char)(fourcc & 0xff);
    text[1] = (char)((fourcc >> 8) & 0xff);
    text[2] = (char)((fourcc >> 16) & 0xff);
    text[3] = (char)((fourcc >> 24) & 0xff);
    text[4] = '\0';
}

static bool find_jpeg_extent(const uint8_t *data, size_t size, size_t *jpeg_size)
{
    if (!data || size < 4) {
        return false;
    }

    size_t start = SIZE_MAX;
    size_t end = SIZE_MAX;
    for (size_t i = 0; i + 1 < size; i++) {
        if (data[i] == 0xff && data[i + 1] == 0xd8) {
            start = i;
            break;
        }
    }
    if (start == SIZE_MAX) {
        return false;
    }

    for (size_t i = start + 2; i + 1 < size; i++) {
        if (data[i] == 0xff && data[i + 1] == 0xd9) {
            end = i + 2;
        }
    }
    if (end == SIZE_MAX || end <= start) {
        return false;
    }

    *jpeg_size = end - start;
    return *jpeg_size > 1024;
}

static bool uyvy_frame_has_content(const uint8_t *data, size_t size)
{
    if (!data || size < UYVY_PIXEL_BYTES * 64) {
        return false;
    }

    uint8_t y_min = UINT8_MAX;
    uint8_t y_max = 0;
    uint64_t y_sum = 0;
    uint64_t y_sq_sum = 0;
    size_t samples = 0;
    const size_t end = size & ~((size_t)UYVY_PIXEL_BYTES - 1U);

    for (size_t i = 1; i < end; i += UYVY_PIXEL_BYTES * 7U) {
        const uint8_t y = data[i];
        y_min = y < y_min ? y : y_min;
        y_max = y > y_max ? y : y_max;
        y_sum += y;
        y_sq_sum += (uint64_t)y * y;
        samples++;
    }
    if (samples == 0 || y_max <= y_min) {
        return false;
    }

    const double mean = (double)y_sum / samples;
    const double variance = (double)y_sq_sum / samples - mean * mean;
    return (y_max - y_min) >= 20 && variance >= 16.0;
}

static bool save_uyvy_as_ppm(const char *path, const uint8_t *data, size_t size,
                             uint32_t width, uint32_t height)
{
    if (!s_sd_ready || !path || !data || width == 0 || height == 0) {
        return false;
    }
    if (size < (size_t)width * height * UYVY_PIXEL_BYTES) {
        ESP_LOGW(TAG, "UYVY frame is too short: %zu", size);
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file) {
        ESP_LOGE(TAG, "Open %s failed: errno=%d", path, errno);
        return false;
    }
    fprintf(file, "P6\n%" PRIu32 " %" PRIu32 "\n255\n", width, height);

    uint8_t row[800 * 3];
    bool ok = width <= sizeof(row) / 3;
    for (uint32_t y = 0; ok && y < height; y++) {
        const uint8_t *src = data + (size_t)y * width * UYVY_PIXEL_BYTES;
        for (uint32_t x = 0; ok && x < width; x += 2) {
            const int u = (int)src[x * 2 + 0] - 128;
            const int y0 = (int)src[x * 2 + 1] - 16;
            const int v = (int)src[x * 2 + 2] - 128;
            const int y1 = (int)src[x * 2 + 3] - 16;
            const int c0 = y0 * 298;
            const int c1 = y1 * 298;

            const int r0 = (c0 + 409 * v + 128) >> 8;
            const int g0 = (c0 - 100 * u - 208 * v + 128) >> 8;
            const int b0 = (c0 + 516 * u + 128) >> 8;
            const int r1 = (c1 + 409 * v + 128) >> 8;
            const int g1 = (c1 - 100 * u - 208 * v + 128) >> 8;
            const int b1 = (c1 + 516 * u + 128) >> 8;

            row[x * 3 + 0] = (uint8_t)(r0 < 0 ? 0 : (r0 > 255 ? 255 : r0));
            row[x * 3 + 1] = (uint8_t)(g0 < 0 ? 0 : (g0 > 255 ? 255 : g0));
            row[x * 3 + 2] = (uint8_t)(b0 < 0 ? 0 : (b0 > 255 ? 255 : b0));
            if (x + 1 < width) {
                row[x * 3 + 3] = (uint8_t)(r1 < 0 ? 0 : (r1 > 255 ? 255 : r1));
                row[x * 3 + 4] = (uint8_t)(g1 < 0 ? 0 : (g1 > 255 ? 255 : g1));
                row[x * 3 + 5] = (uint8_t)(b1 < 0 ? 0 : (b1 > 255 ? 255 : b1));
            }
        }
        if (fwrite(row, 1, (size_t)width * 3, file) != (size_t)width * 3) {
            ok = false;
        }
    }

    if (ok && fflush(file) != 0) {
        ok = false;
    }
    if (ok && fsync(fileno(file)) != 0) {
        ok = false;
    }
    if (fclose(file) != 0) {
        ok = false;
    }
    return ok;
}

static bool save_jpeg(const char *path, const uint8_t *data, size_t jpeg_size)
{
    if (!s_sd_ready || !path || !data || jpeg_size == 0) {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file) {
        ESP_LOGE(TAG, "Open %s failed: errno=%d", path, errno);
        return false;
    }
    bool ok = fwrite(data, 1, jpeg_size, file) == jpeg_size;
    if (ok) {
        ok = fflush(file) == 0;
    }
    if (ok) {
        ok = fsync(fileno(file)) == 0;
    }
    if (fclose(file) != 0) {
        ok = false;
    }
    return ok;
}

static bool save_camera_buffer(uint32_t fourcc, const bsp_camera_format_t *format,
                               const uint8_t *data, size_t size, int sample_index,
                               size_t jpeg_size)
{
    char path[96];
    if (fourcc == V4L2_PIX_FMT_JPEG) {
        snprintf(path, sizeof(path), "%s/camera_%d.jpg", s_sd_mount_point, sample_index);
        return save_jpeg(path, data, jpeg_size);
    }
    if (fourcc == V4L2_PIX_FMT_UYVY) {
        snprintf(path, sizeof(path), "%s/camera_%d.ppm", s_sd_mount_point, sample_index);
        return save_uyvy_as_ppm(path, data, size, format->width, format->height);
    }
    ESP_LOGW(TAG, "No image writer for actual format; frame was validated only");
    return false;
}

static bool encode_uyvy_to_jpeg(jpeg_enc_handle_t encoder, uint32_t width, uint32_t height,
                                const uint8_t *data, size_t size, uint8_t *output,
                                size_t output_capacity, int *output_size)
{
    const size_t expected_size = (size_t)width * height * UYVY_PIXEL_BYTES;
    if (!encoder || !data || !output || !output_size || size < expected_size ||
        expected_size > INT32_MAX || output_capacity > INT32_MAX) {
        return false;
    }

    const jpeg_error_t ret = jpeg_enc_process(
        encoder, data, (int)expected_size, output, (int)output_capacity, output_size);
    if (ret != JPEG_ERR_OK || *output_size <= 0) {
        ESP_LOGE(TAG, "JPEG encode failed: ret=%d size=%d", ret, *output_size);
        return false;
    }
    return true;
}

static bool stream_live_jpeg(bsp_camera_t *camera, const bsp_camera_format_t *format,
                             jpeg_enc_handle_t encoder, uint8_t *jpeg_output)
{
    for (int discarded = 0; discarded < 2; discarded++) {
        bsp_camera_frame_t frame = { 0 };
        if (bsp_camera_get_frame(camera, &frame) != ESP_OK) {
            ESP_LOGE(TAG, "Live capture discard failed");
            return false;
        }
        if (bsp_camera_return_frame(camera, &frame) != ESP_OK) {
            ESP_LOGE(TAG, "Live capture discard queue-back failed");
            return false;
        }
    }

    bsp_camera_frame_t frame = { 0 };
    if (bsp_camera_get_frame(camera, &frame) != ESP_OK) {
        ESP_LOGE(TAG, "Live capture dequeue failed");
        return false;
    }

    int jpeg_size = 0;
    const int64_t encode_start_us = esp_timer_get_time();
    const bool encoded = encode_uyvy_to_jpeg(
        encoder, format->width, format->height, frame.data, frame.size,
        jpeg_output, JPEG_OUTPUT_CAPACITY, &jpeg_size);
    const int64_t encode_us = esp_timer_get_time() - encode_start_us;
    const esp_err_t return_ret = bsp_camera_return_frame(camera, &frame);
    if (return_ret != ESP_OK) {
        ESP_LOGE(TAG, "Live capture queue-back failed: %s", esp_err_to_name(return_ret));
    }
    if (!encoded || return_ret != ESP_OK) {
        return false;
    }

    const uint32_t crc32 = esp_rom_crc32_le(0, jpeg_output, jpeg_size);
    printf("CAMERA_BEGIN index=%" PRIu32 " width=%" PRIu32 " height=%" PRIu32
           " bytes=%d crc32=%08" PRIx32 "\r\n",
           frame.index, format->width, format->height, jpeg_size, crc32);
    fflush(stdout);
    uart_wait_tx_done(UART_NUM_0, portMAX_DELAY);
    const int written = uart_write_bytes(
        UART_NUM_0, (const char *)jpeg_output, (size_t)jpeg_size);
    if (written != jpeg_size || uart_wait_tx_done(UART_NUM_0, portMAX_DELAY) != ESP_OK) {
        ESP_LOGE(TAG, "JPEG UART write failed: written=%d expected=%d", written, jpeg_size);
        return false;
    }
    printf("\r\nCAMERA_END\r\nCAMERA_READY\r\n");
    fflush(stdout);
    ESP_LOGI(TAG, "CAMERA_FRAME_SENT bytes=%d crc32=%08" PRIx32 " encode_us=%" PRId64,
             jpeg_size, crc32, encode_us);
    return true;
}

void app_main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    bsp_sdcard_config_t sd_config = BSP_SDCARD_DEFAULT_CONFIG();
    esp_err_t ret = bsp_sdcard_mount(&sd_config, NULL);
    s_sd_ready = ret == ESP_OK;
    s_sd_mount_point = bsp_sdcard_get_mount_point();
    ESP_LOGI(TAG, "SD mount=%s path=%s",
             s_sd_ready ? "OK" : esp_err_to_name(ret), s_sd_mount_point);

    const bsp_camera_config_t camera_config = {
        .width = 0,
        .height = 0,
        .pixel_format = BSP_CAMERA_PIXEL_FORMAT_JPEG,
        .xclk_freq_hz = BSP_CAMERA_DEFAULT_XCLK_FREQ_HZ,
    };

    bsp_camera_t *camera = NULL;
    ret = bsp_camera_open(&camera_config, &camera);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "T2_AUTO_CHECK=FAIL reason=camera_open error=%s", esp_err_to_name(ret));
        return;
    }

    bsp_camera_format_t format = { 0 };
    ret = bsp_camera_get_format(camera, &format);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "T2_AUTO_CHECK=FAIL reason=get_format error=%s", esp_err_to_name(ret));
        bsp_camera_close(camera);
        return;
    }

    char fourcc_text[5];
    fourcc_to_text(pixel_format_fourcc(format.pixelformat), fourcc_text);
    ESP_LOGI(TAG, "T2_FORMAT width=%" PRIu32 " height=%" PRIu32
             " fourcc=%s stride=%" PRIu32 " sizeimage=%" PRIu32,
             format.width, format.height, fourcc_text, format.bytesperline, format.sizeimage);
    if (format.pixelformat != V4L2_PIX_FMT_JPEG) {
        ESP_LOGW(TAG, "T2_FORMAT_MISMATCH expected=OV3660/JPEG actual=%s", fourcc_text);
    }

    ret = bsp_camera_start(camera);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "T2_AUTO_CHECK=FAIL reason=camera_start error=%s", esp_err_to_name(ret));
        bsp_camera_close(camera);
        return;
    }

    int valid_frames = 0;
    int saved_images = 0;
    size_t first_size = 0;
    size_t last_size = 0;
    const int64_t capture_start_us = esp_timer_get_time();

    for (int i = 0; i < FRAME_COUNT; i++) {
        bsp_camera_frame_t frame = { 0 };
        ret = bsp_camera_get_frame(camera, &frame);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Frame %d dequeue failed: %s", i + 1, esp_err_to_name(ret));
            continue;
        }

        size_t jpeg_size = 0;
        bool valid = false;
        const uint8_t *data = (const uint8_t *)frame.data;
        if (format.pixelformat == V4L2_PIX_FMT_JPEG) {
            valid = find_jpeg_extent(data, frame.size, &jpeg_size);
        } else if (format.pixelformat == V4L2_PIX_FMT_UYVY) {
            valid = uyvy_frame_has_content(data, frame.size);
            jpeg_size = frame.size;
        }

        if (valid) {
            valid_frames++;
        }
        if (i == 0) {
            first_size = frame.size;
        }
        last_size = frame.size;
        ESP_LOGI(TAG, "Frame %02d/%u size=%zu valid=%s", i + 1, FRAME_COUNT,
                 frame.size, valid ? "yes" : "no");

        if (valid && saved_images < JPEG_SAMPLE_COUNT) {
            if (save_camera_buffer(format.pixelformat, &format, data, frame.size,
                                   saved_images + 1, jpeg_size)) {
                saved_images++;
            }
        }

        ret = bsp_camera_return_frame(camera, &frame);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Frame %d queue-back failed: %s", i + 1, esp_err_to_name(ret));
        }
    }

    const double elapsed_s = (esp_timer_get_time() - capture_start_us) / 1000000.0;
    const double fps = elapsed_s > 0 ? FRAME_COUNT / elapsed_s : 0;
    const bool count_pass = valid_frames >= REQUIRED_VALID;
    const bool jpeg_pass = format.pixelformat == V4L2_PIX_FMT_JPEG;
    const bool save_pass = saved_images == JPEG_SAMPLE_COUNT;
    ESP_LOGI(TAG, "T2_STATS fps=%.2f valid=%d/%u first_bytes=%zu last_bytes=%zu saved=%d",
             fps, valid_frames, FRAME_COUNT, first_size, last_size, saved_images);
    ESP_LOGI(TAG, "T2_AUTO_CHECK=%s count=%s sensor_format=%s saved_images=%d/3",
             count_pass && jpeg_pass && save_pass ? "PASS" : "FAIL",
             count_pass ? "pass" : "fail",
             jpeg_pass ? "OV3660/JPEG-expected" : "SC101IOT/UYVY-observed",
             saved_images);

    if (format.pixelformat != V4L2_PIX_FMT_UYVY || !count_pass) {
        ESP_LOGE(TAG, "Live JPEG capture requires a valid SC101IOT/UYVY stream");
        bsp_camera_stop(camera);
        bsp_camera_close(camera);
        if (s_sd_ready) {
            bsp_sdcard_unmount();
        }
        return;
    }

    jpeg_enc_config_t encoder_config = DEFAULT_JPEG_ENC_CONFIG();
    encoder_config.width = (int)format.width;
    encoder_config.height = (int)format.height;
    encoder_config.src_type = JPEG_PIXEL_FORMAT_CbYCrY;
    encoder_config.subsampling = JPEG_SUBSAMPLE_422;
    encoder_config.quality = JPEG_CAPTURE_QUALITY;
    encoder_config.rotate = JPEG_ROTATE_0D;
    encoder_config.task_enable = false;
    jpeg_enc_handle_t encoder = NULL;
    jpeg_error_t jpeg_ret = jpeg_enc_open(&encoder_config, &encoder);
    if (jpeg_ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "JPEG encoder open failed: ret=%d", jpeg_ret);
        bsp_camera_stop(camera);
        bsp_camera_close(camera);
        if (s_sd_ready) {
            bsp_sdcard_unmount();
        }
        return;
    }

    uint8_t *jpeg_output = heap_caps_malloc(
        JPEG_OUTPUT_CAPACITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!jpeg_output) {
        ESP_LOGE(TAG, "No PSRAM for JPEG output buffer");
        jpeg_enc_close(encoder);
        bsp_camera_stop(camera);
        bsp_camera_close(camera);
        if (s_sd_ready) {
            bsp_sdcard_unmount();
        }
        return;
    }

    esp_err_t uart_ret = uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);
    if (uart_ret != ESP_OK && uart_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "UART command driver install failed: %s", esp_err_to_name(uart_ret));
        heap_caps_free(jpeg_output);
        jpeg_enc_close(encoder);
        bsp_camera_stop(camera);
        bsp_camera_close(camera);
        if (s_sd_ready) {
            bsp_sdcard_unmount();
        }
        return;
    }
    ESP_ERROR_CHECK(uart_set_baudrate(UART_NUM_0, SERIAL_CAPTURE_BAUD));
    uart_ret = uart_flush_input(UART_NUM_0);
    if (uart_ret != ESP_OK) {
        ESP_LOGW(TAG, "UART input flush failed: %s", esp_err_to_name(uart_ret));
    }
    ESP_LOGI(TAG, "CAMERA_CAPTURE_READY sensor=SC101IOT format=UYVY baudrate=%d",
             SERIAL_CAPTURE_BAUD);
    printf("CAMERA_READY\r\n");
    fflush(stdout);

    uint8_t command;
    while (true) {
        const int read_len = uart_read_bytes(
            UART_NUM_0, &command, 1, pdMS_TO_TICKS(1000));
        if (read_len < 0) {
            ESP_LOGE(TAG, "UART command read failed: %d", read_len);
            break;
        }
        if (read_len == 0) {
            continue;
        }
        if (command == 'c' || command == 'C') {
            if (!stream_live_jpeg(camera, &format, encoder, jpeg_output)) {
                printf("CAMERA_ERROR\r\nCAMERA_READY\r\n");
                fflush(stdout);
            }
        } else if (command == 'q' || command == 'Q') {
            break;
        } else if (command != '\r' && command != '\n') {
            printf("CAMERA_UNKNOWN_COMMAND\r\nCAMERA_READY\r\n");
            fflush(stdout);
        }
    }

    heap_caps_free(jpeg_output);
    jpeg_enc_close(encoder);
    bsp_camera_stop(camera);
    bsp_camera_close(camera);
    if (s_sd_ready) {
        bsp_sdcard_unmount();
    }
    ESP_LOGI(TAG, "CAMERA_CAPTURE_STOPPED");
}
