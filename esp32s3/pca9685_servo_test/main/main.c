#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Example pins only: change these to match your board and wiring.
#define SDA_GPIO 8
#define SCL_GPIO 9
#define PCA_ADDRESS 0x40
#define SERVO_MIN_US 1000
#define SERVO_MAX_US 2000

static const char *TAG = "servo_test";
static i2c_master_dev_handle_t pca;

static esp_err_t write_reg(uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(pca, data, sizeof(data), 1000);
}

// channel 0..3, or -1 to update all four on one I2C STOP.
static esp_err_t move_servos(int channel, int degrees)
{
    int pulse_us = SERVO_MIN_US + degrees * (SERVO_MAX_US - SERVO_MIN_US) / 180;
    uint16_t ticks = (uint16_t)((pulse_us * 50 * 4096 + 500000) / 1000000);
    uint8_t data[17] = {(uint8_t)(0x06 + (channel < 0 ? 0 : 4 * channel))};
    int count = channel < 0 ? 4 : 1;
    for (int i = 0; i < count; ++i) {
        data[1 + 4 * i] = 0;
        data[2 + 4 * i] = 0;
        data[3 + 4 * i] = ticks & 0xff;
        data[4 + 4 * i] = ticks >> 8;
    }
    return i2c_master_transmit(pca, data, 1 + 4 * count, 1000);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting: SDA=GPIO%d SCL=GPIO%d address=0x%02x", SDA_GPIO, SCL_GPIO, PCA_ADDRESS);
    i2c_master_bus_handle_t bus;
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0, .sda_io_num = SDA_GPIO, .scl_io_num = SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_config, &bus);
    if (err != ESP_OK) goto fail;
    err = i2c_master_probe(bus, PCA_ADDRESS, 1000);
    if (err != ESP_OK) goto fail;
    ESP_LOGI(TAG, "PCA9685 detected");
    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = PCA_ADDRESS,
        .scl_speed_hz = 100000,
    };
    err = i2c_master_bus_add_device(bus, &device_config, &pca);
    if (err != ESP_OK) goto fail;
    // Sleep + auto increment, output changes on STOP, 50 Hz at nominal 25 MHz.
    if ((err = write_reg(0x00, 0x30)) != ESP_OK ||
        (err = write_reg(0x01, 0x04)) != ESP_OK ||
        (err = write_reg(0xfe, 121)) != ESP_OK ||
        (err = write_reg(0x00, 0x20)) != ESP_OK) goto fail;
    vTaskDelay(pdMS_TO_TICKS(10));

    const int positions[] = {90, 60, 120, 90};
    for (;;) {
        ESP_LOGI(TAG, "Testing each channel individually");
        for (int channel = 0; channel < 4; ++channel) {
            for (int i = 0; i < 4; ++i) {
                ESP_LOGI(TAG, "Channel %d -> %d degrees", channel, positions[i]);
                err = move_servos(channel, positions[i]);
                if (err != ESP_OK) goto fail;
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
        ESP_LOGI(TAG, "Testing all four channels together");
        for (int i = 0; i < 4; ++i) {
            ESP_LOGI(TAG, "All channels -> %d degrees", positions[i]);
            err = move_servos(-1, positions[i]);
            if (err != ESP_OK) goto fail;
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
fail:
    ESP_LOGE(TAG, "I2C error: %s. Check GPIOs, address, VCC, GND, SDA and SCL; reset to retry.", esp_err_to_name(err));
}
