#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

esp_err_t cdc_dev_init(void);
int cdc_dev_read(uint8_t *buf, size_t cap, uint32_t timeout_ms);   /* 回 byte 數或負 */
int cdc_dev_write(const uint8_t *buf, size_t len);
