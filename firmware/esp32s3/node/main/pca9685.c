#include "pca9685.h"

#include <math.h>

#define REG_MODE1 0x00
#define REG_MODE2 0x01
#define REG_LED0_ON_L 0x06
#define REG_PRESCALE 0xFE

static esp_err_t write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value) {
  const uint8_t buf[2] = {reg, value};
  return i2c_master_transmit(dev, buf, sizeof buf, 50);
}

esp_err_t pca9685_init(i2c_master_dev_handle_t dev, uint16_t freq_hz) {
  if (freq_hz == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  esp_err_t err = write_reg(dev, REG_MODE1, 0x00);
  if (err != ESP_OK) return err;
  const float prescale = 25000000.0f / (4096.0f * (float)freq_hz) - 1.0f;
  err = write_reg(dev, REG_PRESCALE, (uint8_t)(prescale + 0.5f));
  if (err != ESP_OK) return err;
  err = write_reg(dev, REG_MODE2, 0x04);  /* totem pole */
  if (err != ESP_OK) return err;
  return write_reg(dev, REG_MODE1, 0x20); /* AI + restart */
}

esp_err_t pca9685_set_pulse_us(i2c_master_dev_handle_t dev, uint8_t channel, uint16_t pulse_us) {
  if (channel > 15) {
    return ESP_ERR_INVALID_ARG;
  }
  /* 50 Hz → 20 ms 週期，4096 counts */
  const uint16_t counts = (uint16_t)(((uint32_t)pulse_us * 4096u) / 20000u);
  const uint8_t base = (uint8_t)(REG_LED0_ON_L + 4u * channel);
  const uint8_t buf[2] = {base, (uint8_t)(counts & 0xFF)};
  esp_err_t err = i2c_master_transmit(dev, buf, sizeof buf, 50);
  if (err != ESP_OK) return err;
  const uint8_t buf2[2] = {(uint8_t)(base + 1u), (uint8_t)((counts >> 8) & 0x0F)};
  return i2c_master_transmit(dev, buf2, sizeof buf2, 50);
}

esp_err_t pca9685_close_all(i2c_master_dev_handle_t dev) {
  for (uint8_t ch = 0; ch < 4; ++ch) {
    esp_err_t err = pca9685_set_pulse_us(dev, ch, 1000);
    if (err != ESP_OK) return err;
  }
  return ESP_OK;
}
