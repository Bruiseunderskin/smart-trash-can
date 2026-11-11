#include "tca9548a.h"

esp_err_t tca9548a_init(i2c_master_dev_handle_t dev) { return tca9548a_deselect(dev); }

esp_err_t tca9548a_select(i2c_master_dev_handle_t dev, uint8_t channel) {
  if (channel > 7) {
    return ESP_ERR_INVALID_ARG;
  }
  const uint8_t mask = (uint8_t)(1u << channel);
  return i2c_master_transmit(dev, &mask, 1, 50);
}

esp_err_t tca9548a_deselect(i2c_master_dev_handle_t dev) {
  const uint8_t mask = 0;
  return i2c_master_transmit(dev, &mask, 1, 50);
}
