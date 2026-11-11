#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>

#define TCA9548A_ADDR 0x70

esp_err_t tca9548a_init(i2c_master_dev_handle_t dev);
esp_err_t tca9548a_select(i2c_master_dev_handle_t dev, uint8_t channel);  /* 0–7，只開一個 */
esp_err_t tca9548a_deselect(i2c_master_dev_handle_t dev);
