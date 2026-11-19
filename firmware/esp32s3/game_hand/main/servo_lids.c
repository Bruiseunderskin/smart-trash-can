#include "servo_lids.h"

#include <inttypes.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"

#define SERVO_FREQ_HZ 50
#define SERVO_PERIOD_US 20000
#define SERVO_MIN_US 1000
#define SERVO_MAX_US 2000
#define SERVO_DUTY_BITS 14
#define SERVO_DUTY_MAX ((1u << SERVO_DUTY_BITS) - 1u)
#define SERVO_MODE LEDC_LOW_SPEED_MODE
#define SERVO_TIMER LEDC_TIMER_0

static const char *TAG = "SERVO_LIDS";

static const gpio_num_t s_gpio[STC_LID_COUNT] = {
    GPIO_NUM_12,
    GPIO_NUM_13,
    GPIO_NUM_14,
    GPIO_NUM_15,
};

static const ledc_channel_t s_channel[STC_LID_COUNT] = {
    LEDC_CHANNEL_0,
    LEDC_CHANNEL_1,
    LEDC_CHANNEL_2,
    LEDC_CHANNEL_3,
};

static const uint32_t s_close_us[STC_LID_COUNT] = {
    1450, 1450, 1450, 1450,
};

static const uint32_t s_open_us[STC_LID_COUNT] = {
    1575, 1575, 1575, 1575,
};

static esp_err_t set_pulse_us(int bin, uint32_t pulse_us)
{
    if (bin < 0 || bin >= STC_LID_COUNT ||
        pulse_us < SERVO_MIN_US || pulse_us > SERVO_MAX_US) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint32_t duty =
        (pulse_us * SERVO_DUTY_MAX + SERVO_PERIOD_US / 2u) / SERVO_PERIOD_US;
    ESP_RETURN_ON_ERROR(
        ledc_set_duty(SERVO_MODE, s_channel[bin], duty),
        TAG, "set duty failed");
    return ledc_update_duty(SERVO_MODE, s_channel[bin]);
}

esp_err_t stc_servo_lids_init(void)
{
    const ledc_timer_config_t timer_config = {
        .speed_mode = SERVO_MODE,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .timer_num = SERVO_TIMER,
        .freq_hz = SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_config), TAG,
                        "timer config failed");

    for (int bin = 0; bin < STC_LID_COUNT; ++bin) {
        const ledc_channel_config_t channel_config = {
            .gpio_num = s_gpio[bin],
            .speed_mode = SERVO_MODE,
            .channel = s_channel[bin],
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = SERVO_TIMER,
            .duty = 0,
            .hpoint = 0,
        };
        ESP_RETURN_ON_ERROR(
            ledc_channel_config(&channel_config), TAG,
            "channel config failed");
    }
    ESP_LOGI(TAG, "ready gpio=12,13,14,15 close_us=1450 open_us=1575");
    return ESP_OK;
}

esp_err_t stc_servo_lids_open_all(void)
{
    for (int bin = 0; bin < STC_LID_COUNT; ++bin) {
        ESP_RETURN_ON_ERROR(
            set_pulse_us(bin, s_open_us[bin]), TAG, "open bin %d failed", bin);
        ESP_LOGI(TAG, "OPEN bin=%d gpio=%d pulse_us=%" PRIu32,
                 bin, (int)s_gpio[bin], s_open_us[bin]);
    }
    return ESP_OK;
}

esp_err_t stc_servo_lids_close_all(void)
{
    for (int bin = 0; bin < STC_LID_COUNT; ++bin) {
        ESP_RETURN_ON_ERROR(
            set_pulse_us(bin, s_close_us[bin]), TAG, "close bin %d failed", bin);
        ESP_LOGI(TAG, "CLOSE bin=%d gpio=%d pulse_us=%" PRIu32,
                 bin, (int)s_gpio[bin], s_close_us[bin]);
    }
    return ESP_OK;
}

esp_err_t stc_servo_lids_stop_all(void)
{
    for (int bin = 0; bin < STC_LID_COUNT; ++bin) {
        ESP_RETURN_ON_ERROR(
            ledc_stop(SERVO_MODE, s_channel[bin], 0),
            TAG, "stop bin %d failed", bin);
    }
    ESP_LOGW(TAG, "OUTPUTS_STOPPED");
    return ESP_OK;
}
