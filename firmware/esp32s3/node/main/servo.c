/* servo.c — LEDC 50 Hz PWM，四個 GPIO 直出舵機訊號 */

#include "servo.h"

#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "sdkconfig.h"

#define SERVO_FREQ_HZ 50
#define SERVO_PERIOD_US 20000
/* ESP32-S3 的 LEDC 只有 14-bit 解析度：80 MHz / 16384 / 50 Hz ≈ 97.6 分頻，
 * 仍在可分頻範圍內；每格 1.22 µs，對舵機（500–2500 µs）足夠。 */
#define SERVO_RESOLUTION LEDC_TIMER_14_BIT
#define SERVO_DUTY_MAX ((1u << 14) - 1u)
#define SERVO_TIMER LEDC_TIMER_0
#define SERVO_MODE LEDC_LOW_SPEED_MODE

static const gpio_num_t k_pins[STC_SERVO_COUNT] = {
    CONFIG_STC_SERVO_GPIO_0, CONFIG_STC_SERVO_GPIO_1,
    CONFIG_STC_SERVO_GPIO_2, CONFIG_STC_SERVO_GPIO_3,
};

static bool s_ready;

static uint32_t pulse_to_duty(uint16_t pulse_us) {
  return (uint32_t)(((uint64_t)pulse_us * SERVO_DUTY_MAX) / SERVO_PERIOD_US);
}

esp_err_t servo_init(void) {
  ledc_timer_config_t timer = {
      .speed_mode = SERVO_MODE,
      .duty_resolution = SERVO_RESOLUTION,
      .timer_num = SERVO_TIMER,
      .freq_hz = SERVO_FREQ_HZ,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), "servo", "timer");

  for (uint8_t i = 0; i < STC_SERVO_COUNT; ++i) {
    ledc_channel_config_t channel = {
        .gpio_num = k_pins[i],
        .speed_mode = SERVO_MODE,
        .channel = (ledc_channel_t)i,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = SERVO_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), "servo", "channel");
  }
  s_ready = true;
  return servo_close_all();
}

esp_err_t servo_set_us(uint8_t bin, uint16_t pulse_us) {
  if (!s_ready || bin >= STC_SERVO_COUNT) {
    return ESP_ERR_INVALID_ARG;
  }
  if (pulse_us < 500 || pulse_us > 2500) {
    return ESP_ERR_INVALID_ARG;
  }
  ESP_RETURN_ON_ERROR(
      ledc_set_duty(SERVO_MODE, (ledc_channel_t)bin, pulse_to_duty(pulse_us)), "servo", "duty");
  return ledc_update_duty(SERVO_MODE, (ledc_channel_t)bin);
}

esp_err_t servo_open(uint8_t bin) { return servo_set_us(bin, CONFIG_STC_SERVO_OPEN_US); }

esp_err_t servo_close_all(void) {
  for (uint8_t i = 0; i < STC_SERVO_COUNT; ++i) {
    ESP_RETURN_ON_ERROR(servo_set_us(i, CONFIG_STC_SERVO_CLOSED_US), "servo", "close");
  }
  return ESP_OK;
}
