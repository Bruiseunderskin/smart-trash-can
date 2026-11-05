#include "tof_driver.h"

#include <stdbool.h>
#include <stddef.h>

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define STC_I2C_SDA_GPIO 8
#define STC_I2C_SCL_GPIO 9
#define STC_I2C_HZ 400000
#define STC_MUX_ADDRESS 0x70
#define STC_TOF_ADDRESS 0x29
#define STC_TOF_MODEL_ID 0xB4
#define STC_I2C_TIMEOUT_MS 100
#define STC_MUX_SETTLE_US 100

#define REG_MODEL_ID 0x0000
#define REG_SYSTEM_FRESH_OUT_OF_RESET 0x0016
#define REG_SYSTEM_INTERRUPT_CONFIG 0x0014
#define REG_SYSTEM_INTERRUPT_CLEAR 0x0015
#define REG_SYSTEM_INTERRUPT_STATUS 0x004F
#define REG_RESULT_RANGE_STATUS 0x004D
#define REG_RESULT_RANGE_MM 0x0062
#define REG_SYSRANGE_START 0x0018
#define REG_SYSRANGE_INTERMEASUREMENT 0x001B
#define REG_SYSRANGE_MAX_CONVERGENCE 0x001C
#define REG_SYSRANGE_PART_TO_PART_OFFSET 0x0024
#define REG_SYSRANGE_CROSSTALK_RATE 0x001E
#define REG_SYSRANGE_CROSSTALK_VALID_HEIGHT 0x0021
#define REG_SYSRANGE_RANGE_CHECK_ENABLES 0x002D
#define REG_SYSRANGE_VHV_RECALIBRATE 0x002E
#define REG_SYSRANGE_VHV_REPEAT_RATE 0x0031
#define REG_SYSALS_INTEGRATION_PERIOD 0x0040
#define REG_SYSALS_ANALOGUE_GAIN 0x003F
#define REG_SYSALS_INTERMEASUREMENT 0x003E
#define REG_INTERLEAVED_MODE 0x02A3
#define REG_READOUT_AVERAGING_PERIOD 0x010A
#define REG_RANGE_SCALER 0x0096

#define RANGE_INTERRUPT_NEW_SAMPLE 0x04
#define RANGE_INTERRUPT_MASK 0x07
#define RANGE_CLEAR 0x01

static const char *TAG = "tof_driver";

typedef struct {
    uint16_t reg;
    uint8_t value;
} register_value_t;

/*
 * AN4545 "SR03 settings - Mandatory private registers". The sequence matches
 * Pololu vl6180x-arduino's MIT-licensed implementation of ST's recommended
 * initialization.
 */
static const register_value_t mandatory_registers[] = {
    {0x0207, 0x01}, {0x0208, 0x01}, {0x0096, 0x00}, {0x0097, 0xFD},
    {0x00E3, 0x01}, {0x00E4, 0x03}, {0x00E5, 0x02}, {0x00E6, 0x01},
    {0x00E7, 0x03}, {0x00F5, 0x02}, {0x00D9, 0x05}, {0x00DB, 0xCE},
    {0x00DC, 0x03}, {0x00DD, 0xF8}, {0x009F, 0x00}, {0x00A3, 0x3C},
    {0x00B7, 0x00}, {0x00BB, 0x3C}, {0x00B2, 0x09}, {0x00CA, 0x09},
    {0x0198, 0x01}, {0x01B0, 0x17}, {0x01AD, 0x00}, {0x00FF, 0x05},
    {0x0100, 0x05}, {0x0199, 0x05}, {0x01A6, 0x1B}, {0x01AC, 0x3E},
    {0x01A7, 0x1F}, {0x0030, 0x00},
};

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t mux;
static i2c_master_dev_handle_t sensor;

static esp_err_t mux_select(uint8_t channel) {
    if (!mux || channel >= 4) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint8_t mask = (uint8_t)(1U << channel);
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(mux, &mask, sizeof(mask), STC_I2C_TIMEOUT_MS), TAG,
        "select mux channel %u", channel);
    esp_rom_delay_us(STC_MUX_SETTLE_US);
    return ESP_OK;
}

static esp_err_t write_reg(uint16_t reg, uint8_t value) {
    const uint8_t data[] = {(uint8_t)(reg >> 8), (uint8_t)reg, value};
    return i2c_master_transmit(sensor, data, sizeof(data),
                               STC_I2C_TIMEOUT_MS);
}

static esp_err_t write_word(uint16_t reg, uint16_t value) {
    const uint8_t data[] = {
        (uint8_t)(reg >> 8), (uint8_t)reg, (uint8_t)(value >> 8), (uint8_t)value};
    return i2c_master_transmit(sensor, data, sizeof(data),
                               STC_I2C_TIMEOUT_MS);
}

static esp_err_t read_reg(uint16_t reg, uint8_t *value) {
    const uint8_t index[] = {(uint8_t)(reg >> 8), (uint8_t)reg};
    return i2c_master_transmit_receive(sensor, index, sizeof(index), value, 1,
                                       STC_I2C_TIMEOUT_MS);
}

static esp_err_t read_word(uint16_t reg, uint16_t *value) {
    const uint8_t index[] = {(uint8_t)(reg >> 8), (uint8_t)reg};
    uint8_t data[2];
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit_receive(sensor, index, sizeof(index), data,
                                    sizeof(data), STC_I2C_TIMEOUT_MS),
        TAG, "read register 0x%04x", reg);
    *value = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    return ESP_OK;
}

static esp_err_t configure_defaults(void) {
    ESP_RETURN_ON_ERROR(write_reg(REG_READOUT_AVERAGING_PERIOD, 0x30), TAG,
                        "set averaging period");
    ESP_RETURN_ON_ERROR(write_reg(REG_SYSALS_ANALOGUE_GAIN, 0x46), TAG,
                        "set ALS gain");
    ESP_RETURN_ON_ERROR(write_reg(REG_SYSRANGE_VHV_REPEAT_RATE, 0xFF), TAG,
                        "set VHV repeat");
    ESP_RETURN_ON_ERROR(write_word(REG_SYSALS_INTEGRATION_PERIOD, 99), TAG,
                        "set ALS integration");
    ESP_RETURN_ON_ERROR(write_reg(REG_SYSRANGE_VHV_RECALIBRATE, 0x01), TAG,
                        "trigger VHV calibration");
    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_RETURN_ON_ERROR(write_reg(REG_SYSRANGE_INTERMEASUREMENT, 0x01), TAG,
                        "set 20 ms range period");
    ESP_RETURN_ON_ERROR(write_reg(REG_SYSALS_INTERMEASUREMENT, 0x31), TAG,
                        "set ALS period");
    ESP_RETURN_ON_ERROR(write_reg(REG_SYSTEM_INTERRUPT_CONFIG, 0x24), TAG,
                        "set range-ready interrupt");
    ESP_RETURN_ON_ERROR(write_reg(REG_SYSRANGE_MAX_CONVERGENCE, 0x14), TAG,
                        "set convergence budget");
    ESP_RETURN_ON_ERROR(write_reg(REG_INTERLEAVED_MODE, 0x00), TAG,
                        "disable interleaved mode");
    ESP_RETURN_ON_ERROR(write_word(REG_RANGE_SCALER, 0x00FD), TAG,
                        "set 1x scaling");
    ESP_RETURN_ON_ERROR(write_reg(REG_SYSRANGE_CROSSTALK_VALID_HEIGHT, 20), TAG,
                        "set crosstalk valid height");
    uint8_t range_checks = 0;
    ESP_RETURN_ON_ERROR(
        read_reg(REG_SYSRANGE_RANGE_CHECK_ENABLES, &range_checks), TAG,
        "read range checks");
    return write_reg(REG_SYSRANGE_RANGE_CHECK_ENABLES,
                     (uint8_t)(range_checks | 0x01));
}

esp_err_t stc_tof_bus_init(void) {
    if (bus) {
        return ESP_OK;
    }

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = STC_I2C_SDA_GPIO,
        .scl_io_num = STC_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus), TAG,
                        "create I2C bus");
    ESP_RETURN_ON_ERROR(i2c_master_probe(bus, STC_MUX_ADDRESS,
                                         STC_I2C_TIMEOUT_MS),
                        TAG, "probe TCA9548A");

    const i2c_device_config_t mux_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = STC_MUX_ADDRESS,
        .scl_speed_hz = STC_I2C_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &mux_config, &mux), TAG,
                        "add TCA9548A");

    const i2c_device_config_t sensor_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = STC_TOF_ADDRESS,
        .scl_speed_hz = STC_I2C_HZ,
    };
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(bus, &sensor_config, &sensor), TAG,
        "add VL6180X");
    ESP_LOGI(TAG, "SDA=%d SCL=%d mux=0x%02x sensor=0x%02x",
             STC_I2C_SDA_GPIO, STC_I2C_SCL_GPIO, STC_MUX_ADDRESS,
             STC_TOF_ADDRESS);
    return ESP_OK;
}

esp_err_t stc_tof_bus_recover(void) {
    return bus ? i2c_master_bus_reset(bus) : ESP_ERR_INVALID_STATE;
}

esp_err_t stc_tof_probe(uint8_t channel) {
    ESP_RETURN_ON_ERROR(mux_select(channel), TAG, "select probe channel");
    ESP_RETURN_ON_ERROR(i2c_master_probe(bus, STC_TOF_ADDRESS,
                                         STC_I2C_TIMEOUT_MS),
                        TAG, "probe channel %u", channel);
    uint8_t model = 0;
    ESP_RETURN_ON_ERROR(read_reg(REG_MODEL_ID, &model), TAG,
                        "read model ID");
    return model == STC_TOF_MODEL_ID ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t stc_tof_sensor_init(uint8_t channel, int8_t *factory_offset,
                              uint16_t *factory_xtalk) {
    if (!factory_offset || !factory_xtalk) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(mux_select(channel), TAG, "select init channel");

    uint8_t offset = 0;
    ESP_RETURN_ON_ERROR(read_reg(REG_SYSRANGE_PART_TO_PART_OFFSET, &offset),
                        TAG, "read factory offset");
    *factory_offset = (int8_t)offset;

    uint8_t fresh = 0;
    ESP_RETURN_ON_ERROR(read_reg(REG_SYSTEM_FRESH_OUT_OF_RESET, &fresh), TAG,
                        "read reset state");
    if (fresh == 1) {
        for (size_t i = 0;
             i < sizeof(mandatory_registers) / sizeof(mandatory_registers[0]);
             ++i) {
            ESP_RETURN_ON_ERROR(write_reg(mandatory_registers[i].reg,
                                         mandatory_registers[i].value),
                                TAG, "write mandatory register 0x%04x",
                                mandatory_registers[i].reg);
        }
        ESP_RETURN_ON_ERROR(write_reg(REG_SYSTEM_FRESH_OUT_OF_RESET, 0), TAG,
                            "clear fresh reset flag");
    }

    ESP_RETURN_ON_ERROR(configure_defaults(), TAG,
                        "configure default settings");
    ESP_RETURN_ON_ERROR(read_word(REG_SYSRANGE_CROSSTALK_RATE, factory_xtalk),
                        TAG, "read factory crosstalk");
    return write_reg(REG_SYSTEM_INTERRUPT_CLEAR, RANGE_CLEAR | 0x02 | 0x04);
}

esp_err_t stc_tof_start_continuous(uint8_t channel) {
    ESP_RETURN_ON_ERROR(mux_select(channel), TAG, "select start channel");
    return write_reg(REG_SYSRANGE_START, 0x03);
}

esp_err_t stc_tof_read(uint8_t channel, uint16_t *distance_mm,
                       uint32_t timeout_ms) {
    if (!distance_mm) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(mux_select(channel), TAG, "select read channel");

    const int64_t deadline =
        esp_timer_get_time() + (int64_t)(timeout_ms ? timeout_ms : 30) * 1000;
    for (;;) {
        uint8_t interrupt_status = 0;
        ESP_RETURN_ON_ERROR(
            read_reg(REG_SYSTEM_INTERRUPT_STATUS, &interrupt_status), TAG,
            "read interrupt status");
        if ((interrupt_status & RANGE_INTERRUPT_MASK) ==
            RANGE_INTERRUPT_NEW_SAMPLE) {
            break;
        }
        if (esp_timer_get_time() >= deadline) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    uint8_t range_status = 0;
    uint8_t range = 0;
    ESP_RETURN_ON_ERROR(read_reg(REG_RESULT_RANGE_STATUS, &range_status), TAG,
                        "read range status");
    ESP_RETURN_ON_ERROR(read_reg(REG_RESULT_RANGE_MM, &range), TAG,
                        "read range");
    ESP_RETURN_ON_ERROR(write_reg(REG_SYSTEM_INTERRUPT_CLEAR, RANGE_CLEAR),
                        TAG, "clear range interrupt");
    if (((range_status >> 4) & 0x0F) != 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    *distance_mm = range;
    return ESP_OK;
}

esp_err_t stc_tof_get_offset(uint8_t channel, int8_t *offset) {
    if (!offset) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(mux_select(channel), TAG, "select offset read");
    uint8_t raw = 0;
    ESP_RETURN_ON_ERROR(read_reg(REG_SYSRANGE_PART_TO_PART_OFFSET, &raw), TAG,
                        "read offset");
    *offset = (int8_t)raw;
    return ESP_OK;
}

esp_err_t stc_tof_set_offset(uint8_t channel, int8_t offset) {
    ESP_RETURN_ON_ERROR(mux_select(channel), TAG, "select offset write");
    return write_reg(REG_SYSRANGE_PART_TO_PART_OFFSET, (uint8_t)offset);
}

esp_err_t stc_tof_get_xtalk(uint8_t channel, uint16_t *rate) {
    if (!rate) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(mux_select(channel), TAG, "select xtalk read");
    return read_word(REG_SYSRANGE_CROSSTALK_RATE, rate);
}

esp_err_t stc_tof_set_xtalk(uint8_t channel, uint16_t rate) {
    ESP_RETURN_ON_ERROR(mux_select(channel), TAG, "select xtalk write");
    return write_word(REG_SYSRANGE_CROSSTALK_RATE, rate);
}
