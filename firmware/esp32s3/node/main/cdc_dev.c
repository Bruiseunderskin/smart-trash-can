#include "cdc_dev.h"

#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

static const char *TAG = "cdc_dev";

esp_err_t cdc_dev_init(void) {
  usb_serial_jtag_driver_config_t cfg = {
      .tx_buffer_size = 512,
      .rx_buffer_size = 512,
  };
  esp_err_t err = usb_serial_jtag_driver_install(&cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "usb_serial_jtag install failed: %s", esp_err_to_name(err));
  }
  return err;
}

int cdc_dev_read(uint8_t *buf, size_t cap, uint32_t timeout_ms) {
  if (!buf || cap == 0) {
    return -1;
  }
  const int n = usb_serial_jtag_read_bytes(buf, cap, pdMS_TO_TICKS(timeout_ms));
  return n;
}

int cdc_dev_write(const uint8_t *buf, size_t len) {
  if (!buf || len == 0) {
    return -1;
  }
  return usb_serial_jtag_write_bytes(buf, len, pdMS_TO_TICKS(50));
}
