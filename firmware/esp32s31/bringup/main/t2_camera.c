// T2 — Camera（OV3660，DVP）：初始化 → 抓 30 幀 → 存 3 張 JPEG → 印 fps。
//
// 狀態：**未實作**。這塊板在 ESP-IDF 6.1 上應該用 esp-video（V4L2）而不是舊的
// esp32-camera 驅動。要接的順序如下，執行者請照做並把實際 API 貼回 issue。
//
// TODO(camera):
//   1) 加入依賴：esp_video（espressif/esp_video）與 sensor 驅動（OV3660）
//   2) 建立 video device：esp_video_init() 或 V4L2 的 /dev/video0
//   3) open() → VIDIOC_S_FMT（800×640 或 640×480，V4L2_PIX_FMT_JPEG）
//   4) VIDIOC_REQBUFS / QBUF / DQBUF 抓 30 幀，計 fps
//   5) 把其中 3 幀寫入 SD（mount SD 需先確認腳位）
//
// 參考（同 repo）：xiaozhi-esp32/main/boards/common/esp_video.cc
// 參考（上游）：xiaozhi-esp32/main/boards/espressif/esp32-s31-korvo-1/config.h 的 CAMERA_PIN_*

#include "bringup_common.h"

#include "esp_log.h"

static const char *TAG = "t2_camera";

void t2_camera_run(void) {
  ESP_LOGW(TAG, "camera test not implemented yet");
  ESP_LOGW(TAG, "pins: XCLK=%d PCLK=%d VSYNC=%d HREF=%d D0..D7=%d,%d,%d,%d,%d,%d,%d,%d",
           BRINGUP_CAM_XCLK, BRINGUP_CAM_PCLK, BRINGUP_CAM_VSYNC, BRINGUP_CAM_HREF,
           BRINGUP_CAM_D0, BRINGUP_CAM_D1, BRINGUP_CAM_D2, BRINGUP_CAM_D3,
           BRINGUP_CAM_D4, BRINGUP_CAM_D5, BRINGUP_CAM_D6, BRINGUP_CAM_D7);
  BRINGUP_BLOCKED("T2", "esp_video init not implemented (see TODO in t2_camera.c)");
}
