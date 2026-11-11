#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>

#define TOF050C_ADDR 0x29

esp_err_t tof050c_init(i2c_master_dev_handle_t dev);
esp_err_t tof050c_read_mm(i2c_master_dev_handle_t dev, uint16_t *mm);
