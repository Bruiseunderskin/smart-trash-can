#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "stc_tof_drop.h"

static const char *TAG = "TOF4";

#define NODE_SDA GPIO_NUM_8
#define NODE_SCL GPIO_NUM_9
#define SENSOR_COUNT 4
#define TOF_DEFAULT_ADDR 0x29
#define TOF_FIRST_ADDR 0x30

#define REG_MODEL_ID 0x000
#define REG_MODE_GPIO1 0x011
#define REG_INTERRUPT_CONFIG 0x014
#define REG_INTERRUPT_CLEAR 0x015
#define REG_FRESH_OUT_OF_RESET 0x016
#define REG_RANGE_START 0x018
#define REG_INTERMEASUREMENT_PERIOD 0x01B
#define REG_MAX_CONVERGENCE_TIME 0x01C
#define REG_CROSSTALK_VALID_HEIGHT 0x021
#define REG_PART_TO_PART_OFFSET 0x024
#define REG_VHV_RECALIBRATE 0x02E
#define REG_RANGE_CHECK_ENABLES 0x02D
#define REG_VHV_REPEAT_RATE 0x031
#define REG_RANGE_STATUS 0x04D
#define REG_INTERRUPT_STATUS 0x04F
#define REG_RANGE_VAL 0x062
#define REG_RANGE_RAW 0x064
#define REG_RANGE_RETURN_RATE 0x066
#define REG_RANGE_REFERENCE_RATE 0x068
#define REG_RANGE_SCALER 0x096
#define REG_AVERAGING_SAMPLE_PERIOD 0x10A
#define REG_FIRMWARE_RESULT_SCALER 0x120
#define REG_DEVICE_ADDRESS 0x212
#define REG_INTERLEAVED_MODE 0x2A3

typedef struct {
    gpio_num_t xshut;
    uint8_t address;
    i2c_master_dev_handle_t dev;
    uint8_t model_id;
    uint8_t fresh;
    bool ready;
} tof_sensor_t;

typedef struct {
    uint32_t min_us;
    uint32_t max_us;
    uint32_t p50_us;
    uint32_t p95_us;
    uint32_t avg_us;
    uint32_t ready_rounds;
    uint32_t valid_rounds;
    uint32_t attempts;
} bench_result_t;

static i2c_master_bus_handle_t s_bus;
static tof_sensor_t s_sensors[SENSOR_COUNT] = {
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
    return i2c_master_transmit_receive(dev, address, sizeof address,
                                       value, 1, 50);
}

static esp_err_t read_reg16(i2c_master_dev_handle_t dev, uint16_t reg,
                            uint16_t *value)
{
    const uint8_t address[2] = {(uint8_t)(reg >> 8), (uint8_t)reg};
    uint8_t data[2] = {0};
    esp_err_t err = i2c_master_transmit_receive(
        dev, address, sizeof address, data, sizeof data, 50);
    if (err == ESP_OK) {
        *value = (uint16_t)((uint16_t)data[0] << 8) | data[1];
    }
    return err;
}

static esp_err_t write_checked(i2c_master_dev_handle_t dev, uint16_t reg,
                               uint8_t value)
{
    esp_err_t err = write_reg(dev, reg, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "write reg 0x%03x failed: %s", reg,
                 esp_err_to_name(err));
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
        esp_err_t err = write_checked(dev, sequence[i].reg,
                                      sequence[i].value);
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

    /* 1x scaling gives 1 mm/LSB and enables early convergence. */
    if ((err = write_reg16(dev, REG_RANGE_SCALER, 0x0000)) != ESP_OK ||
        (err = write_checked(dev, REG_PART_TO_PART_OFFSET, ptp_offset)) != ESP_OK ||
        (err = write_checked(dev, REG_CROSSTALK_VALID_HEIGHT, 20)) != ESP_OK) {
        return err;
    }
    uint8_t range_checks = 0;
    if (read_reg(dev, REG_RANGE_CHECK_ENABLES, &range_checks) == ESP_OK) {
        err = write_checked(dev, REG_RANGE_CHECK_ENABLES,
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
                             period_reg)) != ESP_OK ||
        (err = write_checked(dev, REG_INTERRUPT_CLEAR, 0x01)) != ESP_OK) {
        return err;
    }
    return ESP_OK;
}

static void park_all_sensors(void)
{
    for (int i = 0; i < SENSOR_COUNT; ++i) {
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
    if (err != ESP_OK) {
        (void)i2c_master_bus_rm_device(default_dev);
        return err;
    }
    sensor->model_id = model;
    if (model != 0xB4) {
        ESP_LOGE(TAG,
                 "TOF_ID_FAIL xshut=%d addr=0x%02x model=0x%02x expected=0xB4",
                 sensor->xshut, sensor->address, model);
        (void)i2c_master_bus_rm_device(default_dev);
        return ESP_ERR_NOT_FOUND;
    }
    if ((err = read_reg(default_dev, REG_FRESH_OUT_OF_RESET,
                        &sensor->fresh)) != ESP_OK) {
        (void)i2c_master_bus_rm_device(default_dev);
        return err;
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
        ESP_LOGE(TAG, "TOF_VERIFY_FAIL xshut=%d addr=0x%02x model=0x%02x",
                 sensor->xshut, sensor->address, verify);
        (void)i2c_master_bus_rm_device(sensor->dev);
        sensor->dev = NULL;
        return err == ESP_OK ? ESP_ERR_INVALID_STATE : err;
    }
    sensor->ready = true;
    ESP_LOGI(TAG,
             "TOF_READY index=%d xshut=%d addr=0x%02x model=0x%02x fresh=%u",
             (int)(sensor - s_sensors), sensor->xshut, sensor->address,
             sensor->model_id, sensor->fresh);
    return ESP_OK;
}

static void scan_bus(void)
{
    char found[160] = {0};
    size_t used = 0;
    for (uint8_t address = 0x08; address < 0x78; ++address) {
        if (i2c_master_probe(s_bus, address, 20) == ESP_OK) {
            used += (size_t)snprintf(found + used, sizeof(found) - used,
                                     "%s0x%02x", used ? "," : "", address);
            if (used >= sizeof(found)) {
                break;
            }
        }
    }
    ESP_LOGI(TAG, "TOF_I2C_SCAN found=%s", used ? found : "none");
}

static int compare_u32(const void *a, const void *b)
{
    const uint32_t av = *(const uint32_t *)a;
    const uint32_t bv = *(const uint32_t *)b;
    return av < bv ? -1 : av > bv ? 1 : 0;
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

static void summarize(uint32_t *values, uint32_t count, bench_result_t *result)
{
    if (count == 0) {
        return;
    }
    qsort(values, count, sizeof values[0], compare_u32);
    uint64_t total = 0;
    for (uint32_t i = 0; i < count; ++i) {
        total += values[i];
    }
    result->min_us = values[0];
    result->max_us = values[count - 1];
    result->p50_us = values[count / 2];
    result->p95_us =
        values[(uint32_t)(((uint64_t)(count - 1) * 95u) / 100u)];
    result->avg_us = (uint32_t)(total / count);
}

static bool start_continuous(uint16_t max_convergence_ms, uint16_t period_ms)
{
    bool all = true;
    for (int i = 0; i < SENSOR_COUNT; ++i) {
        if (!s_sensors[i].ready) {
            all = false;
            continue;
        }
        (void)write_reg(s_sensors[i].dev, REG_RANGE_START, 0x01);
    }
    vTaskDelay(pdMS_TO_TICKS(max_convergence_ms + 10));
    for (int i = 0; i < SENSOR_COUNT; ++i) {
        if (s_sensors[i].ready) {
            (void)write_reg(s_sensors[i].dev, REG_INTERRUPT_CLEAR, 0x01);
        }
    }
    for (int i = 0; i < SENSOR_COUNT; ++i) {
        if (!s_sensors[i].ready) {
            continue;
        }
        esp_err_t err =
            configure_range(s_sensors[i].dev, max_convergence_ms, period_ms);
        if (err == ESP_OK) {
            uint16_t range_scaler = 0xFFFF;
            uint16_t result_scaler = 0xFFFF;
            (void)read_reg16(s_sensors[i].dev, REG_RANGE_SCALER,
                             &range_scaler);
            (void)read_reg16(s_sensors[i].dev,
                             REG_FIRMWARE_RESULT_SCALER,
                             &result_scaler);
            ESP_LOGI(TAG,
                     "TOF_SCALER index=%d range_scaler=%u "
                     "result_scaler=%u expected_1x=0",
                     i, range_scaler, result_scaler);
            err = write_reg(s_sensors[i].dev, REG_RANGE_START, 0x03);
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG,
                     "TOF_CONTINUOUS_START_FAIL index=%d maxconv=%u period=%u "
                     "err=%s",
                     i, max_convergence_ms, period_ms,
                     esp_err_to_name(err));
            all = false;
        }
    }
    return all;
}

static void print_round(uint32_t index, uint32_t dt_us, const uint8_t *range,
                        const uint8_t *status, const uint8_t *raw,
                        const uint16_t *return_rate,
                        const uint16_t *reference_rate)
{
    ESP_LOGI(TAG,
             "TOF_ROUND n=%lu dt_us=%lu "
             "d0=%u/s%d/raw%d/rr%u/ref%u "
             "d1=%u/s%d/raw%d/rr%u/ref%u "
             "d2=%u/s%d/raw%d/rr%u/ref%u "
             "d3=%u/s%d/raw%d/rr%u/ref%u",
             (unsigned long)index, (unsigned long)dt_us,
             range[0], status[0], raw[0], return_rate[0], reference_rate[0],
             range[1], status[1], raw[1], return_rate[1], reference_rate[1],
             range[2], status[2], raw[2], return_rate[2], reference_rate[2],
             range[3], status[3], raw[3], return_rate[3], reference_rate[3]);
}

static bench_result_t run_benchmark(uint16_t max_convergence_ms,
                                    uint16_t period_ms)
{
    bench_result_t result = {0};
    if (!start_continuous(max_convergence_ms, period_ms)) {
        return result;
    }

    enum { MAX_ROUNDS = 100 };
    static uint32_t samples[MAX_ROUNDS];
    for (int warmup = 0; warmup < 3; ++warmup) {
        vTaskDelay(pdMS_TO_TICKS(period_ms + 5));
        for (int i = 0; i < SENSOR_COUNT; ++i) {
            if (s_sensors[i].ready) {
                (void)write_reg(s_sensors[i].dev, REG_INTERRUPT_CLEAR, 0x01);
            }
        }
    }

    for (int round = 0; round < MAX_ROUNDS; ++round) {
        bool have[SENSOR_COUNT] = {false};
        uint8_t range[SENSOR_COUNT] = {0};
        uint8_t status[SENSOR_COUNT] = {0};
        uint8_t raw[SENSOR_COUNT] = {0};
        uint16_t return_rate[SENSOR_COUNT] = {0};
        uint16_t reference_rate[SENSOR_COUNT] = {0};
        bool valid = true;
        const int64_t started = esp_timer_get_time();
        const int64_t deadline =
            started + (int64_t)(period_ms + 100u) * 1000;
        while (esp_timer_get_time() < deadline) {
            bool complete = true;
            for (int i = 0; i < SENSOR_COUNT; ++i) {
                if (!s_sensors[i].ready || have[i]) {
                    continue;
                }
                uint8_t interrupts = 0;
                if (read_reg(s_sensors[i].dev, REG_INTERRUPT_STATUS,
                             &interrupts) != ESP_OK ||
                    (interrupts & 0x07) != 0x04) {
                    complete = false;
                    continue;
                }
                (void)read_reg(s_sensors[i].dev, REG_RANGE_VAL, &range[i]);
                uint8_t range_status = 0xFF;
                (void)read_reg(s_sensors[i].dev, REG_RANGE_STATUS,
                               &range_status);
                if (round < 5) {
                    (void)read_reg(s_sensors[i].dev, REG_RANGE_RAW, &raw[i]);
                    (void)read_reg16(s_sensors[i].dev,
                                     REG_RANGE_RETURN_RATE,
                                     &return_rate[i]);
                    (void)read_reg16(s_sensors[i].dev,
                                     REG_RANGE_REFERENCE_RATE,
                                     &reference_rate[i]);
                }
                status[i] = range_status >> 4;
                valid = valid && status[i] == 0;
                (void)write_reg(s_sensors[i].dev, REG_INTERRUPT_CLEAR, 0x01);
                have[i] = true;
            }
            for (int i = 0; i < SENSOR_COUNT; ++i) {
                complete = complete && have[i];
            }
            if (complete) {
                const uint32_t dt =
                    (uint32_t)(esp_timer_get_time() - started);
                samples[result.ready_rounds++] = dt;
                if (valid) {
                    result.valid_rounds++;
                }
                if (round < 5) {
                    print_round((uint32_t)round, dt, range, status, raw,
                                return_rate, reference_rate);
                }
                break;
            }
        }
        result.attempts++;
        if (result.attempts >= 5 && result.ready_rounds == 0) {
            break;
        }
    }
    summarize(samples, result.ready_rounds, &result);
    ESP_LOGI(TAG,
             "TOF_BENCH maxconv_ms=%u period_ms=%u attempts=%lu ready=%lu "
             "valid=%lu min_us=%lu p50_us=%lu p95_us=%lu max_us=%lu avg_us=%lu",
             max_convergence_ms, period_ms,
             (unsigned long)result.attempts,
             (unsigned long)result.ready_rounds,
             (unsigned long)result.valid_rounds,
             (unsigned long)result.min_us, (unsigned long)result.p50_us,
             (unsigned long)result.p95_us, (unsigned long)result.max_us,
             (unsigned long)result.avg_us);
    return result;
}

static bool read_detection_round(
    stc_tof_sample_t samples[STC_TOF_SENSOR_COUNT], uint32_t timeout_ms,
    uint8_t *missing_mask)
{
    bool have[STC_TOF_SENSOR_COUNT] = {false};
    if (missing_mask) {
        *missing_mask = 0;
    }
    for (int i = 0; i < SENSOR_COUNT; ++i) {
        samples[i].status = 0xFF;
    }
    const int64_t deadline =
        esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (esp_timer_get_time() < deadline) {
        for (int i = 0; i < SENSOR_COUNT; ++i) {
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
            samples[i].status = err == ESP_OK ? (uint8_t)(status >> 4) : 0xFF;
            (void)write_reg(s_sensors[i].dev, REG_INTERRUPT_CLEAR, 0x01);
            have[i] = true;
        }
        bool complete = true;
        for (int i = 0; i < SENSOR_COUNT; ++i) {
            complete = complete && have[i];
        }
        if (complete) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    uint8_t missing = 0;
    uint8_t present = 0;
    for (int i = 0; i < SENSOR_COUNT; ++i) {
        if (have[i]) {
            present++;
        } else {
            missing |= (uint8_t)(1u << i);
        }
    }
    if (missing_mask) {
        *missing_mask = missing;
    }
    return present > 0;
}

static void run_drop_detector(void)
{
    if (!start_continuous(10, 10)) {
        ESP_LOGE(TAG, "OBJECT_DETECTOR=FAIL reason=START_CONTINUOUS");
        return;
    }

    stc_tof_sample_t samples[STC_TOF_SENSOR_COUNT] = {0};
    for (int i = 0; i < 3; ++i) {
        (void)read_detection_round(samples, 100, NULL);
    }

    enum { BASELINE_TARGET = 30 };
    uint16_t range_history[STC_TOF_SENSOR_COUNT][BASELINE_TARGET] = {{0}};
    uint16_t rate_history[STC_TOF_SENSOR_COUNT][BASELINE_TARGET] = {{0}};
    uint32_t valid[STC_TOF_SENSOR_COUNT] = {0};
    int attempts = 0;
    while (attempts < BASELINE_TARGET * 3) {
        uint8_t missing = 0;
        if (!read_detection_round(samples, 100, &missing) || missing != 0) {
            ESP_LOGW(TAG, "DETECTOR_BASELINE_SAMPLE=FAIL read");
            return;
        }
        bool all_valid = true;
        for (int i = 0; i < SENSOR_COUNT; ++i) {
            if (samples[i].status == 0 &&
                valid[i] < BASELINE_TARGET) {
                range_history[i][valid[i]] = samples[i].range_mm;
                rate_history[i][valid[i]] = samples[i].return_rate;
                valid[i]++;
            }
            all_valid = all_valid && valid[i] >= BASELINE_TARGET;
        }
        attempts++;
        if (all_valid) {
            break;
        }
    }

    stc_tof_baseline_t baseline[STC_TOF_SENSOR_COUNT] = {0};
    for (int i = 0; i < SENSOR_COUNT; ++i) {
        if (valid[i] < BASELINE_TARGET) {
            ESP_LOGE(TAG,
                     "OBJECT_DETECTOR=FAIL reason=BASELINE sensor=%d valid=%lu",
                     i, (unsigned long)valid[i]);
            return;
        }
        baseline[i].range_mm =
            median_u16(range_history[i], BASELINE_TARGET);
        baseline[i].return_rate =
            median_u16(rate_history[i], BASELINE_TARGET);
        ESP_LOGI(TAG,
                 "DETECTOR_BASELINE sensor=%d gpio=%d addr=0x%02x "
                 "range_mm=%u return_rate=%u",
                 i, s_sensors[i].xshut, s_sensors[i].address,
                 baseline[i].range_mm, baseline[i].return_rate);
    }

    stc_tof_drop_detector_t detector;
    if (stc_tof_drop_init(&detector, baseline) != STC_OK) {
        ESP_LOGE(TAG, "OBJECT_DETECTOR=FAIL reason=INIT");
        return;
    }
    ESP_LOGI(TAG,
             "OBJECT_DETECTOR=ARMED sensors=4 sample_period_ms=10 "
             "confirm_samples=%u timeout=none",
             STC_TOF_CONFIRM_SAMPLES);

    uint32_t round = 0;
    uint8_t last_missing = 0;
    while (true) {
        uint8_t missing = 0;
        if (!read_detection_round(samples, 100, &missing)) {
            ESP_LOGW(TAG, "OBJECT_DETECTOR_SAMPLE=FAIL round=%lu",
                     (unsigned long)round);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (missing != last_missing) {
            ESP_LOGW(TAG,
                     "DETECTOR_SENSOR_MISS gpio_mask=0x%02x last=0x%02x",
                     missing, last_missing);
            last_missing = missing;
        }
        stc_tof_drop_event_t event;
        const bool detected =
            stc_tof_drop_update(&detector, samples, &event);
        if (!detected && detector.candidate < STC_TOF_SENSOR_COUNT &&
            detector.streak == 1) {
            const uint8_t candidate = detector.candidate;
            ESP_LOGW(TAG,
                     "DETECTOR_CANDIDATE sensor=%u gpio=%d addr=0x%02x "
                     "range_mm=%u baseline_mm=%u return_rate=%u "
                     "baseline_return=%u",
                     candidate, s_sensors[candidate].xshut,
                     s_sensors[candidate].address,
                     samples[candidate].range_mm,
                     baseline[candidate].range_mm,
                     samples[candidate].return_rate,
                     baseline[candidate].return_rate);
        }
        if (detected) {
            const tof_sensor_t *sensor = &s_sensors[event.sensor_index];
            ESP_LOGI(TAG,
                     "OBJECT_DROP_DETECTED gpio=%d sensor=%u addr=0x%02x "
                     "bin=%u range_mm=%u baseline_mm=%u return_rate=%u "
                     "baseline_return=%u streak=%u round=%lu",
                     sensor->xshut, event.sensor_index, sensor->address,
                     event.sensor_index, event.range_mm,
                     event.baseline_range_mm, event.return_rate,
                     event.baseline_return_rate, event.streak,
                     (unsigned long)round);
            ESP_LOGI(TAG, "OBJECT_DETECTOR=STOPPED");
            return;
        }
        round++;
        if ((round % 100u) == 0) {
            ESP_LOGI(TAG, "OBJECT_DETECTOR=WAIT elapsed_ms=%lu",
                     (unsigned long)(round * 10u));
        }
    }
}

void app_main(void)
{
    const gpio_config_t xshut_cfg = {
        .pin_bit_mask = (1ULL << GPIO_NUM_4) | (1ULL << GPIO_NUM_5) |
                        (1ULL << GPIO_NUM_6) | (1ULL << GPIO_NUM_7),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&xshut_cfg));
    park_all_sensors();

    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = NODE_SDA,
        .scl_io_num = NODE_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_bus));
    ESP_LOGI(TAG,
             "TOF4_PROBE pins sda=%d scl=%d xshut=4,5,6,7 addrs=0x30..0x33",
             NODE_SDA, NODE_SCL);

    for (int i = 0; i < SENSOR_COUNT; ++i) {
        gpio_set_level(s_sensors[i].xshut, 1);
        vTaskDelay(pdMS_TO_TICKS(5));
        esp_err_t err = assign_sensor_address(&s_sensors[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG,
                     "TOF_ASSIGN_FAIL index=%d xshut=%d addr=0x%02x err=%s",
                     i, s_sensors[i].xshut, s_sensors[i].address,
                     esp_err_to_name(err));
            gpio_set_level(s_sensors[i].xshut, 0);
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
    scan_bus();

    int ready_count = 0;
    for (int i = 0; i < SENSOR_COUNT; ++i) {
        ready_count += s_sensors[i].ready ? 1 : 0;
    }
    ESP_LOGI(TAG, "TOF_INIT_SUMMARY ready=%d/%d", ready_count, SENSOR_COUNT);
    if (ready_count != SENSOR_COUNT) {
        ESP_LOGE(TAG, "TOF4_PROBE=FAIL reason=SENSOR_INIT");
        return;
    }

#if STC_TOF_BENCHMARK
    const uint16_t configs[][2] = {
        {50, 50}, {30, 30}, {20, 20}, {10, 10},
    };
    bench_result_t results[sizeof configs / sizeof configs[0]] = {0};
    for (size_t i = 0; i < sizeof configs / sizeof configs[0]; ++i) {
        results[i] = run_benchmark(configs[i][0], configs[i][1]);
    }

    int fastest_ready = -1;
    int fastest_valid = -1;
    for (size_t i = 0; i < sizeof results / sizeof results[0]; ++i) {
        if (results[i].ready_rounds > 0 &&
            (fastest_ready < 0 ||
             results[i].p95_us < results[fastest_ready].p95_us)) {
            fastest_ready = (int)i;
        }
        if (results[i].valid_rounds > 0 &&
            (fastest_valid < 0 ||
             results[i].p95_us < results[fastest_valid].p95_us)) {
            fastest_valid = (int)i;
        }
    }
    if (fastest_ready >= 0) {
        ESP_LOGI(TAG,
                 "TOF_FASTEST_READY maxconv_ms=%u period_ms=%u min_us=%lu "
                 "p50_us=%lu p95_us=%lu valid=%lu/%lu",
                 configs[fastest_ready][0], configs[fastest_ready][1],
                 (unsigned long)results[fastest_ready].min_us,
                 (unsigned long)results[fastest_ready].p50_us,
                 (unsigned long)results[fastest_ready].p95_us,
                 (unsigned long)results[fastest_ready].valid_rounds,
                 (unsigned long)results[fastest_ready].ready_rounds);
    }
    if (fastest_valid >= 0) {
        ESP_LOGI(TAG,
                 "TOF_FASTEST_VALID maxconv_ms=%u period_ms=%u min_us=%lu "
                 "p50_us=%lu p95_us=%lu valid=%lu/%lu",
                 configs[fastest_valid][0], configs[fastest_valid][1],
                 (unsigned long)results[fastest_valid].min_us,
                 (unsigned long)results[fastest_valid].p50_us,
                 (unsigned long)results[fastest_valid].p95_us,
                 (unsigned long)results[fastest_valid].valid_rounds,
                 (unsigned long)results[fastest_valid].ready_rounds);
        ESP_LOGI(TAG, "TOF4_PROBE=PASS");
    } else {
        ESP_LOGW(TAG,
                 "TOF4_PROBE=PARTIAL reason=NO_ALL_SENSOR_VALID_ROUND "
                 "ready=4/4");
    }
#else
    run_drop_detector();
#endif
}
