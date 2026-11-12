/* tof.c — XSHUT 逐支改址 + 連續測距輪詢，其餘交給 nestorit/vl6180x 元件
 *
 * 為什麼還要自己寫一段：`nestorit/vl6180x` 管不到 XSHUT。四粒出廠都在 0x29，
 * 必須「全部拉低 → 逐粒放行 → 立刻改址」才不會撞址。改址本身用元件的
 * `vl6180x_set_address()`（它會寫 0x212 並重建 transport device），所以本檔
 * 沒有自己的 register map。
 *
 * 取樣率：連續模式週期 20 ms + MAX_CONVERGENCE_TIME 12 ms。
 * datasheet 2.4.4 的限制是 max_conv + 5 ≤ period × 0.9 → 17 ≤ 18，符合；
 * 名義取樣率 1 / 20 ms = 50 Hz／粒。
 */

#include "tof.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "vl6180x.h"

#define TOF_I2C_HZ 400000
#define TOF_PERIOD_MS 20
#define TOF_CONVERGENCE_MS 12
/* datasheet 頁 19 建議的 readout averaging；保留原廠建議值，只調週期。 */
#define TOF_AVERAGING_SAMPLE_PERIOD 48
/* 上電後 MCU boot ≤ 1 ms（datasheet t4）；給 5 ms 餘裕。 */
#define TOF_BOOT_MS 5

static const char *TAG = "tof";

static const gpio_num_t k_xshut[TOF_COUNT] = {
    CONFIG_STC_TOF_XSHUT_GPIO_0, CONFIG_STC_TOF_XSHUT_GPIO_1,
    CONFIG_STC_TOF_XSHUT_GPIO_2, CONFIG_STC_TOF_XSHUT_GPIO_3,
};

static i2c_master_bus_handle_t s_bus;
static vl6180x_handle_t s_tof[TOF_COUNT];
static uint8_t s_fault_mask;

static void xshut_all_low(void) {
  for (uint8_t i = 0; i < TOF_COUNT; ++i) {
    gpio_reset_pin(k_xshut[i]);
    gpio_set_direction(k_xshut[i], GPIO_MODE_OUTPUT);
    gpio_set_level(k_xshut[i], 0);
  }
}

/* 放行第 index 粒（此時它在 0x29）→ 改址成 0x30 + index → 開連續測距。 */
static esp_err_t claim_one(uint8_t index) {
  gpio_set_level(k_xshut[index], 1);
  vTaskDelay(pdMS_TO_TICKS(TOF_BOOT_MS));

  const vl6180x_config_t cfg = {
      .i2c_clock_hz = TOF_I2C_HZ,
      .max_convergence_time_ms = TOF_CONVERGENCE_MS,
      .averaging_sample_period = TOF_AVERAGING_SAMPLE_PERIOD,
      .minimal_init = false,
  };

  vl6180x_handle_t handle = NULL;
  ESP_RETURN_ON_ERROR(vl6180x_create(s_bus, VL6180X_DEFAULT_I2C_ADDR, &cfg, &handle), TAG,
                      "create at 0x29");

  const uint8_t address = (uint8_t)(TOF_FIRST_ADDR + index);
  esp_err_t err = vl6180x_set_address(handle, address);
  if (err == ESP_OK) {
    err = vl6180x_range_start_continuous(handle, TOF_PERIOD_MS);
  }
  if (err != ESP_OK) {
    (void)vl6180x_delete(&handle);
    return err;
  }

  s_tof[index] = handle;
  ESP_LOGI(TAG, "bin %u: VL6180X at 0x%02x, continuous %u ms (%u Hz)", index, address,
           TOF_PERIOD_MS, 1000u / TOF_PERIOD_MS);
  return ESP_OK;
}

esp_err_t tof_init(i2c_master_bus_handle_t bus) {
  s_bus = bus;
  s_fault_mask = 0;
  for (uint8_t i = 0; i < TOF_COUNT; ++i) {
    s_tof[i] = NULL;
  }

  xshut_all_low();
  vTaskDelay(pdMS_TO_TICKS(10));
  for (uint8_t i = 0; i < TOF_COUNT; ++i) {
    esp_err_t err = claim_one(i);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "bin %u failed: %s", i, esp_err_to_name(err));
      s_fault_mask |= (uint8_t)(1u << i);
    }
  }
  return s_fault_mask == 0 ? ESP_OK : ESP_ERR_NOT_FOUND;
}

uint8_t tof_fault_mask(void) { return s_fault_mask; }

esp_err_t tof_poll(uint8_t index, uint16_t *mm) {
  if (!mm || index >= TOF_COUNT || !s_tof[index]) {
    return ESP_ERR_INVALID_ARG;
  }
  vl6180x_handle_t handle = s_tof[index];

  vl6180x_int_status_t event;
  ESP_RETURN_ON_ERROR(vl6180x_get_interrupt_status(handle, &event), TAG, "int status");
  if (event.range_event != (uint8_t)VL6180X_INT_MODE_NEW_SAMPLE_READY) {
    return ESP_ERR_NOT_FINISHED;
  }

  /* 先把這個樣本的 range status 讀出來（bits [7:4] = 錯誤碼，0 才算有效），
   * 之後才讀值；這樣就不可能拿舊值配新狀態。 */
  vl6180x_range_status_t status;
  ESP_RETURN_ON_ERROR(vl6180x_range_get_status(handle, &status), TAG, "range status");

  uint8_t range = 0;
  if (status.error_code != (uint8_t)VL6180X_RANGE_ERROR_NONE) {
    /* 樣本無效：仍然讀一次把樣本消費掉並清中斷，值直接丟棄。 */
    (void)vl6180x_range_read_continuous(handle, &range);
    return ESP_ERR_INVALID_RESPONSE;
  }

  ESP_RETURN_ON_ERROR(vl6180x_range_read_continuous(handle, &range), TAG, "range read");
  *mm = range;
  return ESP_OK;
}
