#include "camera_capture.h"

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "camera_capture_config.h"
#include "esp_log.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "linux/videodev2.h"

#include "stc_preprocess.h"

static const char *TAG = "stc_camera";
static constexpr uint32_t kFrameBytes = STC_FRAME_W * STC_FRAME_H * 2u;
static constexpr int kMaxBuffers = 2;

static bool s_video_ready;
static i2c_master_bus_handle_t s_i2c_bus;
static int s_fd = -1;
static uint32_t s_buffer_count;
static void *s_buffers[kMaxBuffers];
static size_t s_lengths[kMaxBuffers];
static bool s_streaming;
static const char *s_device_name;

static void close_stream(void)
{
    if (s_streaming) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        (void)ioctl(s_fd, VIDIOC_STREAMOFF, &type);
        s_streaming = false;
    }
    for (uint32_t i = 0; i < s_buffer_count; ++i) {
        if (s_buffers[i] && s_buffers[i] != MAP_FAILED) {
            (void)munmap(s_buffers[i], s_lengths[i]);
        }
        s_buffers[i] = nullptr;
        s_lengths[i] = 0;
    }
    s_buffer_count = 0;
    if (s_fd >= 0) {
        (void)close(s_fd);
        s_fd = -1;
    }
}

extern "C" stc_err_t stc_camera_capture_begin(void)
{
    if (s_fd >= 0) {
        return STC_OK;
    }
    if (!s_video_ready) {
        if (!s_i2c_bus) {
            ESP_LOGE(TAG, "camera SCCB bus was not supplied");
            return STC_ERR_NOT_READY;
        }
        esp_cam_ctlr_dvp_pin_config_t dvp_pin_config = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io =
                {
                    [0] = STC_CAMERA_PIN_D0,
                    [1] = STC_CAMERA_PIN_D1,
                    [2] = STC_CAMERA_PIN_D2,
                    [3] = STC_CAMERA_PIN_D3,
                    [4] = STC_CAMERA_PIN_D4,
                    [5] = STC_CAMERA_PIN_D5,
                    [6] = STC_CAMERA_PIN_D6,
                    [7] = STC_CAMERA_PIN_D7,
                },
            .vsync_io = STC_CAMERA_PIN_VSYNC,
            .de_io = STC_CAMERA_PIN_HREF,
            .pclk_io = STC_CAMERA_PIN_PCLK,
            .xclk_io = STC_CAMERA_PIN_XCLK,
        };
        esp_video_init_sccb_config_t sccb_config = {
            .init_sccb = false,
            .i2c_handle = s_i2c_bus,
            .freq = STC_CAMERA_SCCB_FREQ_HZ,
        };
        esp_video_init_dvp_config_t camera_config = {
            .sccb_config = sccb_config,
            .reset_pin = STC_CAMERA_PIN_RESET,
            .pwdn_pin = STC_CAMERA_PIN_PWDN,
            .dvp_pin = dvp_pin_config,
            .xclk_freq = STC_CAMERA_XCLK_FREQ_HZ,
        };
        esp_video_init_config_t video_config = {
            .dvp = &camera_config,
        };
        const esp_err_t err = esp_video_init(&video_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_video_init failed: %s", esp_err_to_name(err));
            return STC_ERR_IO;
        }
        s_device_name = ESP_VIDEO_DVP_DEVICE_NAME;
        s_video_ready = true;
    }

    s_fd = open(s_device_name, O_RDWR | O_NONBLOCK);
    if (s_fd < 0) {
        ESP_LOGE(TAG, "open %s failed errno=%d", s_device_name, errno);
        return STC_ERR_IO;
    }

    struct v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = STC_FRAME_W;
    fmt.fmt.pix.height = STC_FRAME_H;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(s_fd, VIDIOC_S_FMT, &fmt) != 0 ||
        fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_UYVY) {
        ESP_LOGE(TAG, "UYVY 640x480 format unsupported");
        close_stream();
        return STC_ERR_IO;
    }

    struct v4l2_requestbuffers req = {};
    req.count = kMaxBuffers;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_fd, VIDIOC_REQBUFS, &req) != 0 || req.count < 2) {
        ESP_LOGE(TAG, "VIDIOC_REQBUFS failed");
        close_stream();
        return STC_ERR_IO;
    }
    s_buffer_count = req.count < kMaxBuffers ? req.count : kMaxBuffers;

    for (uint32_t i = 0; i < s_buffer_count; ++i) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(s_fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "VIDIOC_QUERYBUF failed");
            close_stream();
            return STC_ERR_IO;
        }
        s_buffers[i] = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE,
                            MAP_SHARED, s_fd, buf.m.offset);
        s_lengths[i] = buf.length;
        if (s_buffers[i] == MAP_FAILED) {
            s_buffers[i] = nullptr;
            close_stream();
            return STC_ERR_IO;
        }
        if (ioctl(s_fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "VIDIOC_QBUF failed");
            close_stream();
            return STC_ERR_IO;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(s_fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "VIDIOC_STREAMON failed");
        close_stream();
        return STC_ERR_IO;
    }
    s_streaming = true;
    ESP_LOGI(TAG, "SC101IOT stream on, 640x480 UYVY");
    return STC_OK;
}

extern "C" stc_err_t stc_camera_capture_frame(uint8_t *rgb, uint32_t timeout_ms)
{
    if (!rgb || s_fd < 0 || !s_streaming) {
        return STC_ERR_NOT_READY;
    }
    const uint32_t step_ms = 20;
    uint32_t waited = 0;
    while (waited <= timeout_ms) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(s_fd, VIDIOC_DQBUF, &buf) != 0) {
            if (errno != EAGAIN && errno != EINTR) {
                ESP_LOGE(TAG, "VIDIOC_DQBUF failed errno=%d", errno);
                return STC_ERR_IO;
            }
            vTaskDelay(pdMS_TO_TICKS(step_ms));
            waited += step_ms;
            continue;
        }

        const bool valid = buf.bytesused >= kFrameBytes &&
                           buf.index < s_buffer_count && s_buffers[buf.index];
        if (valid &&
            stc_preprocess_uyvy_to_rgb(
                static_cast<const uint8_t *>(s_buffers[buf.index]),
                STC_FRAME_W, STC_FRAME_H, rgb, STC_INPUT_SIZE) == STC_OK) {
            (void)ioctl(s_fd, VIDIOC_QBUF, &buf);
            return STC_OK;
        }
        (void)ioctl(s_fd, VIDIOC_QBUF, &buf);
        waited += step_ms;
    }
    return STC_ERR_IO;
}

extern "C" void stc_camera_capture_end(void)
{
    close_stream();
}

extern "C" void stc_camera_capture_set_i2c_bus(void *i2c_bus)
{
    if (s_fd < 0) {
        s_i2c_bus = static_cast<i2c_master_bus_handle_t>(i2c_bus);
    }
}
