// stc_node — 四蓋 + 四粒 TOF 的 S3 節點（#53 I7）
//
// 收 CDC 的 LID_CMD → PCA9685 開對應蓋；每 100 ms 送 NODE_STATE（四粒 TOF）。
// 無線全關（見 sdkconfig.defaults）。上板驗證仍待 Bruiseunderskin。

#include <string.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "cdc_dev.h"
#include "pca9685.h"
#include "stc_lidmsg.h"
#include "tca9548a.h"
#include "tof050c.h"

static const char *TAG = "stc_node";

#define NODE_SDA GPIO_NUM_8
#define NODE_SCL GPIO_NUM_9
#define STATE_PERIOD_MS 100

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_mux;
static i2c_master_dev_handle_t s_pca;
static i2c_master_dev_handle_t s_tof;

static esp_err_t add_device(uint8_t address, i2c_master_dev_handle_t *out) {
  i2c_device_config_t cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = address,
      .scl_speed_hz = 400000,
  };
  return i2c_master_bus_add_device(s_bus, &cfg, out);
}

static void open_bin(uint8_t bin) {
  const uint16_t pulse_us = (uint16_t)(1500 + (bin % 4) * 100);
  esp_err_t err = pca9685_set_pulse_us(s_pca, bin, pulse_us);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "open bin %u failed: %s", bin, esp_err_to_name(err));
  }
}

static void send_state(void) {
  stc_lidmsg_t msg;
  memset(&msg, 0, sizeof msg);
  msg.sof = STC_LIDMSG_SOF;
  msg.ver = STC_LIDMSG_VER;
  msg.type = STC_LID_MSG_NODE_STATE;

  for (uint8_t ch = 0; ch < 4; ++ch) {
    if (tca9548a_select(s_mux, ch) == ESP_OK) {
      uint16_t mm = 0;
      if (tof050c_read_mm(s_tof, &mm) == ESP_OK) {
        msg.tof_mm[ch] = mm;
      }
    }
  }
  (void)tca9548a_deselect(s_mux);
  stc_lidmsg_seal(&msg);
  (void)cdc_dev_write((const uint8_t *)&msg, sizeof msg);
}

static void handle_command(const stc_lidmsg_t *msg) {
  if (msg->type != STC_LID_MSG_CMD) {
    return;
  }
  if (msg->action == 0) {
    (void)pca9685_close_all(s_pca);
  } else {
    open_bin(msg->bin);
  }
  stc_lidmsg_t ack = *msg;
  ack.type = STC_LID_MSG_ACK;
  stc_lidmsg_seal(&ack);
  (void)cdc_dev_write((const uint8_t *)&ack, sizeof ack);
}

void app_main(void) {
  i2c_master_bus_config_t bus_cfg = {
      .i2c_port = I2C_NUM_0,
      .sda_io_num = NODE_SDA,
      .scl_io_num = NODE_SCL,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_bus));
  ESP_ERROR_CHECK(add_device(TCA9548A_ADDR, &s_mux));
  ESP_ERROR_CHECK(add_device(PCA9685_ADDR, &s_pca));
  ESP_ERROR_CHECK(add_device(TOF050C_ADDR, &s_tof));

  ESP_ERROR_CHECK(tca9548a_init(s_mux));
  ESP_ERROR_CHECK(pca9685_init(s_pca, 50));
  ESP_ERROR_CHECK(pca9685_close_all(s_pca));
  ESP_ERROR_CHECK(cdc_dev_init());

  ESP_LOGI(TAG, "node up: i2c sda=%d scl=%d, wifi off", NODE_SDA, NODE_SCL);

  int64_t next_state = 0;
  while (true) {
    uint8_t buf[64];
    const int n = cdc_dev_read(buf, sizeof buf, 20);
    if (n == (int)sizeof(stc_lidmsg_t)) {
      stc_lidmsg_t msg;
      memcpy(&msg, buf, sizeof msg);
      if (stc_lidmsg_check(&msg)) {
        handle_command(&msg);
      }
    }

    const int64_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now >= next_state) {
      send_state();
      next_state = now + STATE_PERIOD_MS;
    }
  }
}
