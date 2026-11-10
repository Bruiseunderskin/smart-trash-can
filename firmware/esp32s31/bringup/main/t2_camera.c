// T2 — Camera（SC101IOT，DVP）：初始化 → 抓 30 幀 → 印 fps 與 RESULT 行。
//
// 用 esp-video 的 V4L2 介面（/dev/video0）。若 IDF 6.1 image 未帶 esp_video_init.h，
// 本檔仍然編譯，但會印 RESULT T2 FAIL code=2 並說明缺少依賴 —— 不會假裝成功。

#include "bringup_common.h"

#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "linux/videodev2.h"

#if __has_include("esp_video_init.h")
#include "esp_video_init.h"
#define HAVE_ESP_VIDEO 1
#else
#define HAVE_ESP_VIDEO 0
#endif

static const char *TAG = "t2_camera";
#define FRAMES 30

void t2_camera_run(void) {
  ESP_LOGI(TAG, "pins: XCLK=%d PCLK=%d VSYNC=%d HREF=%d D0..D7=%d,%d,%d,%d,%d,%d,%d,%d",
           BRINGUP_CAM_XCLK, BRINGUP_CAM_PCLK, BRINGUP_CAM_VSYNC, BRINGUP_CAM_HREF,
           BRINGUP_CAM_D0, BRINGUP_CAM_D1, BRINGUP_CAM_D2, BRINGUP_CAM_D3,
           BRINGUP_CAM_D4, BRINGUP_CAM_D5, BRINGUP_CAM_D6, BRINGUP_CAM_D7);

#if HAVE_ESP_VIDEO
  esp_video_init_config_t cfg = {0};
  esp_err_t err = esp_video_init(&cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_video_init failed: %s", esp_err_to_name(err));
    printf("RESULT T2 FAIL code=3\n");
    BRINGUP_BLOCKED("T2", "esp_video_init returned an error");
    return;
  }

  int fd = open("/dev/video0", O_RDWR);
  if (fd < 0) {
    ESP_LOGE(TAG, "open /dev/video0 failed");
    printf("RESULT T2 FAIL code=4\n");
    BRINGUP_BLOCKED("T2", "no /dev/video0");
    return;
  }

  struct v4l2_format fmt;
  memset(&fmt, 0, sizeof fmt);
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = 640;
  fmt.fmt.pix.height = 480;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
  fmt.fmt.pix.field = V4L2_FIELD_NONE;
  if (ioctl(fd, VIDIOC_S_FMT, &fmt) != 0) {
    ESP_LOGE(TAG, "VIDIOC_S_FMT UYVY 640x480 failed");
    close(fd);
    printf("RESULT T2 FAIL code=5\n");
    return;
  }

  struct v4l2_requestbuffers req;
  memset(&req, 0, sizeof req);
  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0 || req.count < 2) {
    ESP_LOGE(TAG, "VIDIOC_REQBUFS failed");
    close(fd);
    printf("RESULT T2 FAIL code=6\n");
    return;
  }

  static void *buffers[8];
  static size_t lengths[8];
  for (uint32_t i = 0; i < req.count && i < 8; ++i) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0) {
      close(fd);
      printf("RESULT T2 FAIL code=7\n");
      return;
    }
    buffers[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    lengths[i] = buf.length;
    if (buffers[i] == MAP_FAILED) {
      close(fd);
      printf("RESULT T2 FAIL code=8\n");
      return;
    }
    if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
      close(fd);
      printf("RESULT T2 FAIL code=9\n");
      return;
    }
  }

  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
    close(fd);
    printf("RESULT T2 FAIL code=10\n");
    return;
  }

  const int64_t t0 = esp_timer_get_time();
  int valid = 0;
  for (int i = 0; i < FRAMES; ++i) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &buf) != 0) {
      continue;
    }
    const size_t want = 640u * 480u * 2u;
    if (buf.bytesused >= want) {
      valid++;
    }
    (void)ioctl(fd, VIDIOC_QBUF, &buf);
  }
  const int64_t dt_us = esp_timer_get_time() - t0;
  const double fps = (dt_us > 0) ? (FRAMES * 1e6 / (double)dt_us) : 0.0;

  ioctl(fd, VIDIOC_STREAMOFF, &type);
  for (uint32_t i = 0; i < req.count && i < 8; ++i) {
    if (buffers[i] && buffers[i] != MAP_FAILED) {
      munmap(buffers[i], lengths[i]);
    }
  }
  close(fd);

  ESP_LOGI(TAG, "valid %d/%d frames, %.1f fps", valid, FRAMES, fps);
  if (valid >= 28) {
    printf("RESULT T2 PASS frames=%d/%d fps=%.1f\n", valid, FRAMES, fps);
  } else {
    printf("RESULT T2 FAIL code=11 frames=%d/%d\n", valid, FRAMES);
  }
#else
  ESP_LOGW(TAG, "esp_video_init.h not available in this IDF image");
  printf("RESULT T2 FAIL code=2\n");
  BRINGUP_BLOCKED("T2", "esp_video component missing (see TODO in t2_camera.c)");
#endif
}
