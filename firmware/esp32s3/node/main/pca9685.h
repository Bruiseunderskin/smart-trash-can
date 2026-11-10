#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>

#define PCA9685_ADDR 0x40

esp_err_t pca9685_init(i2c_master_dev_handle_t dev, uint16_t freq_hz);
esp_err_t pca9685_set_pulse_us(i2c_master_dev_handle_t dev, uint8_t channel, uint16_t pulse_us);
esp_err_t pca9685_close_all(i2c_master_dev_handle_t dev);
