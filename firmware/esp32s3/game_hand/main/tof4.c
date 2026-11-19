#include "tof4.h"

#include <stdbool.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TOF4";

#define NODE_SDA GPIO_NUM_8
#define NODE_SCL GPIO_NUM_9
#define TOF_DEFAULT_ADDR 0x29

#define REG_MODEL_ID 0x000
#define REG_INTERRUPT_CONFIG 0x014
#define REG_INTERRUPT_CLEAR 0x015
#define REG_FRESH_OUT_OF_RESET 0x016
#define REG_RANGE_START 0x018
#define REG_INTERMEASUREMENT_PERIOD 0x01B
#define REG_MAX_CONVERGENCE_TIME 0x01C
#define REG_CROSSTALK_VALID_HEIGHT 0x021
#define REG_PART_TO_PART_OFFSET 0x024
#define REG_RANGE_CHECK_ENABLES 0x02D
#define REG_VHV_RECALIBRATE 0x02E
#define REG_VHV_REPEAT_RATE 0x031
#define REG_RANGE_STATUS 0x04D
#define REG_INTERRUPT_STATUS 0x04F
#define REG_RANGE_VAL 0x062
#define REG_RANGE_RETURN_RATE 0x066
#define REG_RANGE_SCALER 0x096
#define REG_AVERAGING_SAMPLE_PERIOD 0x10A
#define REG_DEVICE_ADDRESS 0x212
#define REG_INTERLEAVED_MODE 0x2A3

typedef struct {
    gpio_num_t xshut;
    uint8_t address;
    i2c_master_dev_handle_t dev;
    bool ready;
} tof_sensor_t;

static i2c_master_bus_handle_t s_bus;
static tof_sensor_t s_sensors[STC_TOF4_SENSOR_COUNT] = {
    {.xshut = GPIO_NUM_4, .address = 0x30},
    {.xshut = GPIO_NUM_5, .address = 0x31},
    {.xshut = GPIO_NUM_6, .address = 0x32},
    {.xshut = GPIO_NUM_7, .address = 0x33},
};

static esp_err_t add_device(uint8_t address, i2c_master_dev_handle_t *out)
{
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 400000,
    };
    return i2c_master_bus_add_device(s_bus, &cfg, out);
}

static esp_err_t write_reg(i2c_master_dev_handle_t dev, uint16_t reg,
                           uint8_t value)
{
    const uint8_t data[3] = {
        (uint8_t)(reg >> 8), (uint8_t)reg, value,
    };
    return i2c_master_transmit(dev, data, sizeof data, 50);
}

static esp_err_t write_reg16(i2c_master_dev_handle_t dev, uint16_t reg,
                             uint16_t value)
{
    const uint8_t data[4] = {
        (uint8_t)(reg >> 8), (uint8_t)reg,
        (uint8_t)(value >> 8), (uint8_t)value,
    };
    return i2c_master_transmit(dev, data, sizeof data, 50);
}

static esp_err_t read_reg(i2c_master_dev_handle_t dev, uint16_t reg,
                          uint8_t *value)
{
    const uint8_t address[2] = {(uint8_t)(reg >> 8), (uint8_t)reg};
    return i2c_master_transmit_receive(
        dev, address, sizeof address, value, 1, 50);
}

static esp_err_t read_reg16(i2c_master_dev_handle_t dev, uint16_t reg,
                            uint16_t *value)
{
    const uint8_t address[2] = {(uint8_t)(reg >> 8), (uint8_t)reg};
    uint8_t data[2] = {0};
    const esp_err_t err = i2c_master_transmit_receive(
        dev, address, sizeof address, data, sizeof data, 50);
    if (err == ESP_OK) {
        *value = (uint16_t)((uint16_t)data[0] << 8) | data[1];
    }
    return err;
}

static esp_err_t write_checked(i2c_master_dev_handle_t dev, uint16_t reg,
                               uint8_t value)
{
    const esp_err_t err = write_reg(dev, reg, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "write reg 0x%03x failed: %s",
                 reg, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t private_init(i2c_master_dev_handle_t dev)
{
    static const struct {
        uint16_t reg;
        uint8_t value;
    } sequence[] = {
        {0x207, 0x01}, {0x208, 0x01}, {0x096, 0x00}, {0x097, 0xFD},
        {0x0E3, 0x01}, {0x0E4, 0x03}, {0x0E5, 0x02}, {0x0E6, 0x01},
        {0x0E7, 0x03}, {0x0F5, 0x02}, {0x0D9, 0x05}, {0x0DB, 0xCE},
        {0x0DC, 0x03}, {0x0DD, 0xF8}, {0x09F, 0x00}, {0x0A3, 0x3C},
        {0x0B7, 0x00}, {0x0BB, 0x3C}, {0x0B2, 0x09}, {0x0CA, 0x09},
        {0x198, 0x01}, {0x1B0, 0x17}, {0x1AD, 0x00}, {0x0FF, 0x05},
        {0x100, 0x05}, {0x199, 0x05}, {0x1A6, 0x1B}, {0x1AC, 0x3E},
        {0x1A7, 0x1F}, {0x030, 0x00},
    };
    for (size_t i = 0; i < sizeof sequence / sizeof sequence[0]; ++i) {
        const esp_err_t err =
            write_checked(dev, sequence[i].reg, sequence[i].value);
        if (err != ESP_OK) {
            return err;
        }
    }
    return write_checked(dev, REG_FRESH_OUT_OF_RESET, 0x00);
}

static esp_err_t configure_range(i2c_master_dev_handle_t dev,
                                 uint16_t max_convergence_ms,
                                 uint16_t period_ms)
{
    uint8_t ptp_offset = 0;
    esp_err_t err = read_reg(dev, REG_PART_TO_PART_OFFSET, &ptp_offset);
    if (err != ESP_OK) {
        return err;
    }
    uint8_t fresh = 0;
    err = read_reg(dev, REG_FRESH_OUT_OF_RESET, &fresh);
    if (err != ESP_OK) {
        return err;
    }
    if (fresh == 1) {
        err = private_init(dev);
        if (err != ESP_OK) {
            return err;
        }
    }

    if ((err = write_checked(dev, REG_AVERAGING_SAMPLE_PERIOD, 0x30)) != ESP_OK ||
        (err = write_checked(dev, REG_VHV_REPEAT_RATE, 0xFF)) != ESP_OK ||
        (err = write_checked(dev, REG_VHV_RECALIBRATE, 0x01)) != ESP_OK ||
        (err = write_checked(dev, REG_INTERRUPT_CONFIG, 0x04)) != ESP_OK ||
        (err = write_checked(dev, REG_INTERLEAVED_MODE, 0x00)) != ESP_OK) {
        return err;
    }

    if ((err = write_reg16(dev, REG_RANGE_SCALER, 0x0000)) != ESP_OK ||
        (err = write_checked(dev, REG_PART_TO_PART_OFFSET, ptp_offset)) != ESP_OK ||
        (err = write_checked(dev, REG_CROSSTALK_VALID_HEIGHT, 20)) != ESP_OK) {
        return err;
    }

    uint8_t range_checks = 0;
    if (read_reg(dev, REG_RANGE_CHECK_ENABLES, &range_checks) == ESP_OK) {
        err = write_checked(
            dev, REG_RANGE_CHECK_ENABLES,
            (uint8_t)((range_checks & 0xFE) | 0x01));
        if (err != ESP_OK) {
            return err;
        }
    }

    if (max_convergence_ms == 0 || max_convergence_ms > 63) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint8_t period_reg =
        period_ms < 10 ? 0 : (uint8_t)(period_ms / 10 - 1);
    if ((err = write_checked(dev, REG_MAX_CONVERGENCE_TIME,
                             (uint8_t)max_convergence_ms)) != ESP_OK ||
        (err = write_checked(dev, REG_INTERMEASUREMENT_PERIOD,
                             period_reg)) != ESP_OK) {
        return err;
    }
    return write_checked(dev, REG_INTERRUPT_CLEAR, 0x01);
}

static void park_all(void)
{
    for (int i = 0; i < STC_TOF4_SENSOR_COUNT; ++i) {
        gpio_set_level(s_sensors[i].xshut, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}

static esp_err_t assign_sensor_address(tof_sensor_t *sensor)
{
    i2c_master_dev_handle_t default_dev = NULL;
    esp_err_t err = add_device(TOF_DEFAULT_ADDR, &default_dev);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t model = 0;
    err = read_reg(default_dev, REG_MODEL_ID, &model);
    if (err != ESP_OK || model != 0xB4) {
        (void)i2c_master_bus_rm_device(default_dev);
        return err == ESP_OK ? ESP_ERR_NOT_FOUND : err;
    }

    err = write_reg(default_dev, REG_DEVICE_ADDRESS, sensor->address);
    (void)i2c_master_bus_rm_device(default_dev);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(2));

    err = add_device(sensor->address, &sensor->dev);
    if (err != ESP_OK) {
        return err;
    }
    uint8_t verify = 0;
    err = read_reg(sensor->dev, REG_MODEL_ID, &verify);
    if (err != ESP_OK || verify != 0xB4) {
        (void)i2c_master_bus_rm_device(sensor->dev);
        sensor->dev = NULL;
        return err == ESP_OK ? ESP_ERR_INVALID_STATE : err;
    }
    sensor->ready = true;
    ESP_LOGI(TAG, "READY sensor=%d gpio=%d addr=0x%02x",
             (int)(sensor - s_sensors), (int)sensor->xshut, sensor->address);
    return ESP_OK;
}

esp_err_t stc_tof4_init(void)
{
    const gpio_config_t xshut_cfg = {
        .pin_bit_mask = (1ULL << GPIO_NUM_4) | (1ULL << GPIO_NUM_5) |
                        (1ULL << GPIO_NUM_6) | (1ULL << GPIO_NUM_7),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&xshut_cfg), TAG, "xshut config failed");
    park_all();

    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = NODE_SDA,
        .scl_io_num = NODE_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), TAG,
                        "i2c init failed");

    for (int i = 0; i < STC_TOF4_SENSOR_COUNT; ++i) {
        gpio_set_level(s_sensors[i].xshut, 1);
        vTaskDelay(pdMS_TO_TICKS(5));
        const esp_err_t err = assign_sensor_address(&s_sensors[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "assign sensor=%d gpio=%d failed: %s",
                     i, (int)s_sensors[i].xshut, esp_err_to_name(err));
            gpio_set_level(s_sensors[i].xshut, 0);
        }
    }

    const int ready = stc_tof4_ready_count();
    ESP_LOGI(TAG, "INIT ready=%d/%d", ready, STC_TOF4_SENSOR_COUNT);
    return ready == STC_TOF4_SENSOR_COUNT ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t stc_tof4_start_continuous(void)
{
    for (int i = 0; i < STC_TOF4_SENSOR_COUNT; ++i) {
        if (!s_sensors[i].ready) {
            return ESP_ERR_INVALID_STATE;
        }
        ESP_RETURN_ON_ERROR(
            write_reg(s_sensors[i].dev, REG_RANGE_START, 0x01),
            TAG, "start sensor %d failed", i);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    for (int i = 0; i < STC_TOF4_SENSOR_COUNT; ++i) {
        (void)write_reg(s_sensors[i].dev, REG_INTERRUPT_CLEAR, 0x01);
        ESP_RETURN_ON_ERROR(
            configure_range(s_sensors[i].dev, 10, 10),
            TAG, "configure sensor %d failed", i);
        ESP_RETURN_ON_ERROR(
            write_reg(s_sensors[i].dev, REG_RANGE_START, 0x03),
            TAG, "continuous sensor %d failed", i);
    }
    return ESP_OK;
}

esp_err_t stc_tof4_read_round(
    stc_tof_sample_t samples[STC_TOF4_SENSOR_COUNT],
    uint32_t timeout_ms, uint8_t *missing_mask)
{
    if (!samples) {
        return ESP_ERR_INVALID_ARG;
    }
    bool have[STC_TOF4_SENSOR_COUNT] = {false};
    if (missing_mask) {
        *missing_mask = 0;
    }
    for (int i = 0; i < STC_TOF4_SENSOR_COUNT; ++i) {
        samples[i].status = 0xFF;
    }

    const TickType_t started = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(timeout_ms);
    while ((xTaskGetTickCount() - started) < timeout) {
        for (int i = 0; i < STC_TOF4_SENSOR_COUNT; ++i) {
            if (have[i]) {
                continue;
            }
            uint8_t interrupts = 0;
            if (read_reg(s_sensors[i].dev, REG_INTERRUPT_STATUS,
                         &interrupts) != ESP_OK ||
                (interrupts & 0x07) != 0x04) {
                continue;
            }
            uint8_t range = 0;
            uint8_t status = 0xFF;
            uint16_t return_rate = 0;
            esp_err_t err = read_reg(s_sensors[i].dev, REG_RANGE_VAL, &range);
            if (err == ESP_OK) {
                err = read_reg(s_sensors[i].dev, REG_RANGE_STATUS, &status);
            }
            if (err == ESP_OK) {
                err = read_reg16(s_sensors[i].dev, REG_RANGE_RETURN_RATE,
                                 &return_rate);
            }
            samples[i].range_mm = range;
            samples[i].return_rate = return_rate;
            samples[i].status =
                err == ESP_OK ? (uint8_t)(status >> 4) : 0xFF;
            (void)write_reg(s_sensors[i].dev, REG_INTERRUPT_CLEAR, 0x01);
            have[i] = true;
        }

        bool complete = true;
        for (int i = 0; i < STC_TOF4_SENSOR_COUNT; ++i) {
            complete = complete && have[i];
        }
        if (complete) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    int present = 0;
    for (int i = 0; i < STC_TOF4_SENSOR_COUNT; ++i) {
        if (have[i]) {
            present++;
        } else if (missing_mask) {
            *missing_mask |= (uint8_t)(1u << i);
        }
    }
    return present > 0 ? ESP_ERR_TIMEOUT : ESP_FAIL;
}

static int compare_u16(const void *a, const void *b)
{
    const uint16_t av = *(const uint16_t *)a;
    const uint16_t bv = *(const uint16_t *)b;
    return av < bv ? -1 : av > bv ? 1 : 0;
}

static uint16_t median_u16(uint16_t *values, uint32_t count)
{
    qsort(values, count, sizeof values[0], compare_u16);
    return values[count / 2];
}

esp_err_t stc_tof4_capture_baseline(
    stc_tof_baseline_t baseline[STC_TOF4_SENSOR_COUNT],
    uint32_t sample_timeout_ms)
{
    if (!baseline) {
        return ESP_ERR_INVALID_ARG;
    }

    stc_tof_sample_t samples[STC_TOF4_SENSOR_COUNT] = {0};
    for (int i = 0; i < 3; ++i) {
        (void)stc_tof4_read_round(samples, sample_timeout_ms, NULL);
    }

    uint16_t range_history[STC_TOF4_SENSOR_COUNT]
                          [STC_TOF4_BASELINE_SAMPLES] = {{0}};
    uint16_t rate_history[STC_TOF4_SENSOR_COUNT]
                         [STC_TOF4_BASELINE_SAMPLES] = {{0}};
    uint32_t valid[STC_TOF4_SENSOR_COUNT] = {0};
    int attempts = 0;

    while (attempts < STC_TOF4_BASELINE_SAMPLES * 3) {
        uint8_t missing = 0;
        (void)stc_tof4_read_round(samples, sample_timeout_ms, &missing);
        bool all_valid = true;
        for (int i = 0; i < STC_TOF4_SENSOR_COUNT; ++i) {
            if (samples[i].status == 0 &&
                valid[i] < STC_TOF4_BASELINE_SAMPLES) {
                range_history[i][valid[i]] = samples[i].range_mm;
                rate_history[i][valid[i]] = samples[i].return_rate;
                valid[i]++;
            }
            all_valid = all_valid &&
                        valid[i] >= STC_TOF4_BASELINE_SAMPLES;
        }
        attempts++;
        if (all_valid) {
            break;
        }
    }

    for (int i = 0; i < STC_TOF4_SENSOR_COUNT; ++i) {
        if (valid[i] < STC_TOF4_BASELINE_SAMPLES) {
            ESP_LOGE(TAG, "BASELINE_FAIL sensor=%d valid=%lu",
                     i, (unsigned long)valid[i]);
            return ESP_ERR_TIMEOUT;
        }
        baseline[i].range_mm =
            median_u16(range_history[i], STC_TOF4_BASELINE_SAMPLES);
        baseline[i].return_rate =
            median_u16(rate_history[i], STC_TOF4_BASELINE_SAMPLES);
        ESP_LOGI(TAG,
                 "BASELINE sensor=%d gpio=%d range_mm=%u return_rate=%u",
                 i, (int)s_sensors[i].xshut, baseline[i].range_mm,
                 baseline[i].return_rate);
    }
    return ESP_OK;
}

int stc_tof4_ready_count(void)
{
    int count = 0;
    for (int i = 0; i < STC_TOF4_SENSOR_COUNT; ++i) {
        count += s_sensors[i].ready ? 1 : 0;
    }
    return count;
}

int stc_tof4_gpio(int sensor_index)
{
    if (sensor_index < 0 || sensor_index >= STC_TOF4_SENSOR_COUNT) {
        return -1;
    }
    return (int)s_sensors[sensor_index].xshut;
}
