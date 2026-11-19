#include <cerrno>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "bsp/esp32_s31_korvo_1.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_video_device.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "linux/videodev2.h"

#include "replay_classifier.hpp"
#include "stc_preprocess.h"

#if defined(STC_CAMERA_PROBE) && STC_CAMERA_PROBE

static const char *TAG = "stc_camera_probe";
static constexpr int kBufferCount = 2;

extern "C" int stc_camera_probe_run(void)
{
    int fd = -1;
    bool stream_on = false;
    void *buffers[kBufferCount] = {};
    size_t lengths[kBufferCount] = {};
    uint32_t count = 0;
    bool got_frame = false;
    uint8_t *rgb = nullptr;
    float probs[STC_MODEL_CLASSES] = {};
    int prediction = -1;
    struct v4l2_format fmt = {};
    struct v4l2_requestbuffers req = {};
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ESP_LOGI(TAG, "initializing ESP-DL model");
    if (stc_game_model_init() != STC_OK) {
        ESP_LOGE(TAG, "CAMERA_PROBE=FAIL reason=MODEL_INIT");
        return 1;
    }

    ESP_LOGI(TAG, "starting BSP camera (SC101IOT DVP)");
    esp_err_t err = bsp_camera_start(nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CAMERA_PROBE=FAIL reason=BSP_CAMERA_START err=%s",
                 esp_err_to_name(err));
        return 2;
    }

    fd = open(BSP_CAMERA_DEVICE, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        ESP_LOGE(TAG, "CAMERA_PROBE=FAIL reason=OPEN errno=%d", errno);
        return 3;
    }

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = STC_FRAME_W;
    fmt.fmt.pix.height = STC_FRAME_H;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) != 0 ||
        fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_UYVY) {
        ESP_LOGE(TAG, "CAMERA_PROBE=FAIL reason=FORMAT fmt=%08x",
                 (unsigned)fmt.fmt.pix.pixelformat);
        goto cleanup;
    }
    ESP_LOGI(TAG, "format=%ux%u UYVY", (unsigned)fmt.fmt.pix.width,
             (unsigned)fmt.fmt.pix.height);

    req.count = kBufferCount;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0 || req.count < 2) {
        ESP_LOGE(TAG, "CAMERA_PROBE=FAIL reason=REQBUFS count=%u",
                 (unsigned)req.count);
        goto cleanup;
    }
    count = req.count < kBufferCount ? req.count : kBufferCount;

    for (uint32_t i = 0; i < count; ++i) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "CAMERA_PROBE=FAIL reason=QUERYBUF index=%u",
                     (unsigned)i);
            goto cleanup;
        }
        buffers[i] = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, buf.m.offset);
        lengths[i] = buf.length;
        if (buffers[i] == MAP_FAILED) {
            buffers[i] = nullptr;
            ESP_LOGE(TAG, "CAMERA_PROBE=FAIL reason=MMAP index=%u",
                     (unsigned)i);
            goto cleanup;
        }
        if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "CAMERA_PROBE=FAIL reason=QBUF index=%u",
                     (unsigned)i);
            goto cleanup;
        }
    }

    if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "CAMERA_PROBE=FAIL reason=STREAMON");
        goto cleanup;
    }
    stream_on = true;
    rgb = static_cast<uint8_t *>(
        heap_caps_malloc(STC_INPUT_SIZE * STC_INPUT_SIZE * 3,
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!rgb) {
        ESP_LOGE(TAG, "CAMERA_PROBE=FAIL reason=RGB_ALLOC");
        goto cleanup;
    }

    for (int attempt = 0; attempt < 50 && !got_frame; ++attempt) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(fd, VIDIOC_DQBUF, &buf) != 0) {
            if (errno == EAGAIN || errno == EINTR) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            ESP_LOGE(TAG, "CAMERA_PROBE=FAIL reason=DQBUF errno=%d", errno);
            goto cleanup;
        }

        const size_t minimum = (size_t)STC_FRAME_W * STC_FRAME_H * 2u;
        if (buf.bytesused >= minimum && buf.index < count &&
            buffers[buf.index]) {
            if (stc_preprocess_uyvy_to_rgb(
                    static_cast<const uint8_t *>(buffers[buf.index]),
                    STC_FRAME_W, STC_FRAME_H, rgb, STC_INPUT_SIZE) != STC_OK) {
                ESP_LOGE(TAG, "CAMERA_PROBE=FAIL reason=PREPROCESS");
            } else if (!stc_game_classify_rgb160(rgb, probs)) {
                ESP_LOGE(TAG, "CAMERA_PROBE=FAIL reason=INFERENCE");
            } else {
                got_frame = true;
            }
        } else {
            ESP_LOGW(TAG, "drop frame index=%u bytes=%u",
                     (unsigned)buf.index, (unsigned)buf.bytesused);
        }
        (void)ioctl(fd, VIDIOC_QBUF, &buf);
    }

    if (got_frame) {
        prediction = 0;
        for (int i = 1; i < STC_MODEL_CLASSES; ++i) {
            if (probs[i] > probs[prediction]) {
                prediction = i;
            }
        }
        ESP_LOGI(TAG,
                 "CAMERA_PROBE=PASS frame=%ux%u UYVY bytes=%u class=%d conf=%.6f",
                 STC_FRAME_W, STC_FRAME_H,
                 (unsigned)(STC_FRAME_W * STC_FRAME_H * 2),
                 prediction, (double)probs[prediction]);
        fflush(stdout);
        printf("CAMERA_RGB160_HEX_BEGIN\n");
        constexpr uint32_t rgb_bytes = STC_INPUT_SIZE * STC_INPUT_SIZE * 3;
        uint32_t line_no = 0;
        for (uint32_t i = 0; i < rgb_bytes; i += 64, ++line_no) {
            printf("RGB%05x:", (unsigned)i);
            const uint32_t end = (i + 64 < rgb_bytes) ? i + 64 : rgb_bytes;
            for (uint32_t j = i; j < end; ++j) {
                printf("%02x", rgb[j]);
            }
            printf("\n");
            if ((line_no & 31u) == 31u) {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
        fflush(stdout);
        printf("\nCAMERA_RGB160_HEX_END\n");
        goto cleanup;
    }

    ESP_LOGE(TAG, "CAMERA_PROBE=FAIL reason=NO_VALID_FRAME");

cleanup:
    if (stream_on) {
        (void)ioctl(fd, VIDIOC_STREAMOFF, &type);
    }
    for (uint32_t i = 0; i < count; ++i) {
        if (buffers[i] && buffers[i] != MAP_FAILED) {
            (void)munmap(buffers[i], lengths[i]);
        }
    }
    if (fd >= 0) {
        (void)close(fd);
    }
    heap_caps_free(rgb);
    return got_frame ? 0 : 4;
}

#else

extern "C" int stc_camera_probe_run(void)
{
    return 0;
}

#endif
