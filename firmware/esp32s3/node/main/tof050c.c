#include "tof050c.h"

#define REG_ID 0x000
#define REG_SYSRANGE_START 0x018
#define REG_RESULT_RANGE 0x062

static esp_err_t write_reg16(i2c_master_dev_handle_t dev, uint16_t reg, uint8_t value) {
  const uint8_t buf[3] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), value};
  return i2c_master_transmit(dev, buf, sizeof buf, 50);
}

static esp_err_t read_reg16(i2c_master_dev_handle_t dev, uint16_t reg, uint8_t *value) {
  const uint8_t addr[2] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF)};
  return i2c_master_transmit_receive(dev, addr, sizeof addr, value, 1, 50);
}

esp_err_t tof050c_init(i2c_master_dev_handle_t dev) {
  uint8_t id = 0;
  esp_err_t err = read_reg16(dev, REG_ID, &id);
  if (err != ESP_OK) return err;
  return (id == 0xB4 || id == 0xB5) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t tof050c_read_mm(i2c_master_dev_handle_t dev, uint16_t *mm) {
  if (!mm) return ESP_ERR_INVALID_ARG;
  esp_err_t err = write_reg16(dev, REG_SYSRANGE_START, 0x01);
  if (err != ESP_OK) return err;
  uint8_t range = 0;
  err = read_reg16(dev, REG_RESULT_RANGE, &range);
  if (err != ESP_OK) return err;
  *mm = range;
  return ESP_OK;
}
