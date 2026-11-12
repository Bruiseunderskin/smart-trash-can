// stc_node — 四蓋 + 四粒 TOF 的 S3 節點（#53 I7，2026-11 硬件改版）
//
// 2026-11 硬件改版後的拓撲：
//   * 四支舵機由 GPIO 直出 LEDC PWM（50 Hz），每支一條腳；
//   * 4 粒 TOF050C（VL6180X）共用一條 I²C（SDA 8 / SCL 9），靠 XSHUT 逐支放行後
//     改址成 0x30–0x33（驅動用 nestorit/vl6180x 元件，見 tof.c）；
//   * S31 ↔ 節點走 ESP-NOW；幀仍然是 stc_lidmsg 16 bytes（CRC-16/CCITT-FALSE）。
//   * TOF 用連續測距（50 Hz／粒），主迴圈 5 ms 輪詢，NODE_STATE 每 100 ms 送
//     一次「該視窗內看過的最小有效距離」（0 = 視窗內沒有有效樣本）。
// 無線：Wi-Fi 開（ESP-NOW 需要）、藍牙關。上板驗證仍待 Bruinseunderskin。

#include <string.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "now_link.h"
#include "servo.h"
#include "stc_lidmsg.h"
#include "tof.h"

static const char *TAG = "stc_node";

#define NODE_SDA GPIO_NUM_8
#define NODE_SCL GPIO_NUM_9
/* 取樣迴圈 5 ms（200 Hz），遠高於感測器本身的 50 Hz，所以每個樣本都會被看到。
 * 發佈週期仍是 100 ms，並且只送「這段視窗內看過的最小有效距離」。 */
#define SAMPLE_TICK_MS 5
#define STATE_PERIOD_MS 100
#define CMD_QUEUE_LEN 4

typedef struct {
  stc_lidmsg_t msg;
  uint8_t mac[6];
} cmd_item_t;

static QueueHandle_t s_cmd_queue;

/* 每個桶在目前 100 ms 視窗看過的最小有效距離（mm），0 = 這個視窗沒有有效樣本。
 * 卡片落下是一次性的暫態：單看某一瞬間容易剛好落在兩個樣本之間，取視窗最小值
 * 才抓得到「落下時最近的那一下」。 */
static uint16_t s_window_min[TOF_COUNT];

static void on_frame(const stc_lidmsg_t *msg, const uint8_t *mac) {
  if (!msg || msg->type != STC_LID_MSG_CMD) {
    return;
  }
  cmd_item_t item;
  item.msg = *msg;
  memcpy(item.mac, mac, sizeof item.mac);
  if (xQueueSend(s_cmd_queue, &item, 0) != pdTRUE) {
    ESP_LOGW(TAG, "command queue full, dropping seq %u", msg->seq);
  }
}

/* 把四粒的新樣本併進視窗最小值；沒有新樣本（NOT_READY）或樣本無效就不動。 */
static void sample_sensors(void) {
  for (uint8_t ch = 0; ch < TOF_COUNT; ++ch) {
    uint16_t mm = 0;
    if (tof_poll(ch, &mm) != ESP_OK) {
      continue;
    }
    if (s_window_min[ch] == 0 || mm < s_window_min[ch]) {
      s_window_min[ch] = mm;
    }
  }
}

static void send_state(void) {
  stc_lidmsg_t msg;
  memset(&msg, 0, sizeof msg);
  msg.sof = STC_LIDMSG_SOF;
  msg.ver = STC_LIDMSG_VER;
  msg.type = STC_LID_MSG_NODE_STATE;

  for (uint8_t ch = 0; ch < TOF_COUNT; ++ch) {
    msg.tof_mm[ch] = s_window_min[ch];
    s_window_min[ch] = 0;
  }
  stc_lidmsg_seal(&msg);

  uint8_t peer[6];
  (void)now_link_send(&msg, now_link_peer(peer) ? peer : NULL);
}

static void handle_command(const cmd_item_t *item) {
  if (item->msg.action == 0) {
    (void)servo_close_all();
  } else if (item->msg.bin < STC_SERVO_COUNT) {
    esp_err_t err = servo_open(item->msg.bin);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "open bin %u failed: %s", item->msg.bin, esp_err_to_name(err));
    }
  }

  stc_lidmsg_t ack = item->msg;
  ack.type = STC_LID_MSG_ACK;
  stc_lidmsg_seal(&ack);
  (void)now_link_send(&ack, item->mac);
}

void app_main(void) {
  s_cmd_queue = xQueueCreate(CMD_QUEUE_LEN, sizeof(cmd_item_t));

  i2c_master_bus_config_t bus_cfg = {
      .i2c_port = I2C_NUM_0,
      .sda_io_num = NODE_SDA,
      .scl_io_num = NODE_SCL,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  i2c_master_bus_handle_t bus = NULL;
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

  ESP_ERROR_CHECK(servo_init());
  /* A missing sensor must not stop the node: it reports a fault mask instead. */
  esp_err_t tof = tof_init(bus);
  if (tof != ESP_OK) {
    ESP_LOGW(TAG, "tof fault mask 0x%02x", tof_fault_mask());
  }
  ESP_ERROR_CHECK(now_link_init(on_frame));

  ESP_LOGI(TAG, "node up: i2c sda=%d scl=%d, esp-now ch=%d", NODE_SDA, NODE_SCL,
           CONFIG_STC_NOW_CHANNEL);

  int64_t next_state = 0;
  TickType_t last_wake = xTaskGetTickCount();
  while (true) {
    /* 命令優先：佇列裡有幾筆就處理幾筆，不讓舵機動作卡住取樣。 */
    cmd_item_t item;
    while (xQueueReceive(s_cmd_queue, &item, 0) == pdTRUE) {
      handle_command(&item);
    }

    sample_sensors();

    const int64_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now >= next_state) {
      send_state();
      next_state = now + STATE_PERIOD_MS;
    }

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_TICK_MS));
  }
}
