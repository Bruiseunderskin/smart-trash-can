// Bring-up 入口：用 menuconfig 揀一個測試跑（一次只跑一個，方便定位問題）。
//
// Bring-up menu → Test to run → T1/T2/T3/T4/T5

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void t1_lcd_run(void);
void t2_camera_run(void);
void t3_mic_run(void);
void t4_speaker_run(void);
void t5_loopback_run(void);

static const char *TAG = "bringup";

void app_main(void) {
  ESP_LOGI(TAG, "ESP32-S31-Korvo-1 bring-up");
  ESP_LOGI(TAG, "GPIO: LCD PCLK=%d DE=%d VSYNC=%d HSYNC=%d | CAM XCLK=%d | PA=%d",
           40, 43, 45, 44, 55, 7);

#if CONFIG_BRINGUP_TEST_T1
  t1_lcd_run();
#elif CONFIG_BRINGUP_TEST_T2
  t2_camera_run();
#elif CONFIG_BRINGUP_TEST_T3
  t3_mic_run();
#elif CONFIG_BRINGUP_TEST_T4
  t4_speaker_run();
#elif CONFIG_BRINGUP_TEST_T5
  t5_loopback_run();
#else
  ESP_LOGW(TAG, "未選擇測試：idf.py menuconfig → Bring-up → Test to run");
#endif

  ESP_LOGI(TAG, "done — 把上面 RESULT 行貼回 issue #11");
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}
