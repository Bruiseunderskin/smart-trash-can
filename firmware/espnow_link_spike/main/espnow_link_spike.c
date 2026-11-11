/*
 * ESP-NOW S31 <-> S3 link spike.
 *
 * This project proves the transport only. It does not drive servos, TOF
 * sensors, camera, UI, or the game controller. The 16-byte frame is the
 * existing stc_lidmsg contract shared with the USB CDC implementation.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "sdkconfig.h"

#include "stc_lidmsg.h"

#if CONFIG_STC_ESPNOW_ROLE_NODE
#define STC_IS_NODE 1
#define STC_ROLE_NAME "node"
#else
#define STC_IS_NODE 0
#define STC_ROLE_NAME "controller"
#endif

static const char *TAG = "stc_espnow";

#define EVENT_QUEUE_LEN 16
#define NODE_STATE_PERIOD_MS 250
#define CONTROLLER_COMMAND_PERIOD_MS 3000
#define CONTROLLER_OFFLINE_MS 1000
#define CONTROLLER_ACK_TIMEOUT_MS 500

static const uint8_t k_broadcast_mac[ESP_NOW_ETH_ALEN] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

typedef enum {
    LINK_EVENT_RX = 0,
    LINK_EVENT_TX_DONE,
} link_event_type_t;

typedef struct {
    link_event_type_t type;
    uint8_t src_mac[ESP_NOW_ETH_ALEN];
    stc_lidmsg_t msg;
    esp_now_send_status_t status;
} link_event_t;

static QueueHandle_t s_event_queue;
static bool s_tx_busy;
static uint16_t s_rx_ok;
static uint16_t s_crc_errors;
static uint16_t s_tx_errors;

static bool mac_equal(const uint8_t lhs[ESP_NOW_ETH_ALEN],
                      const uint8_t rhs[ESP_NOW_ETH_ALEN])
{
    return memcmp(lhs, rhs, ESP_NOW_ETH_ALEN) == 0;
}

static bool is_broadcast(const uint8_t mac[ESP_NOW_ETH_ALEN])
{
    return mac_equal(mac, k_broadcast_mac);
}

static bool post_event(const link_event_t *event)
{
    if (!s_event_queue || !event) {
        return false;
    }
    return xQueueSend(s_event_queue, event, pdMS_TO_TICKS(10)) == pdTRUE;
}

static void send_callback(const esp_now_send_info_t *tx_info,
                          esp_now_send_status_t status)
{
    link_event_t event = {
        .type = LINK_EVENT_TX_DONE,
        .status = status,
    };
    if (tx_info && tx_info->des_addr) {
        memcpy(event.src_mac, tx_info->des_addr, sizeof(event.src_mac));
    }
    (void)post_event(&event);
}

static void receive_callback(const esp_now_recv_info_t *recv_info,
                             const uint8_t *data, int data_len)
{
    if (!recv_info || !recv_info->src_addr || !data ||
        data_len != (int)sizeof(stc_lidmsg_t)) {
        return;
    }

    link_event_t event = {.type = LINK_EVENT_RX};
    memcpy(event.src_mac, recv_info->src_addr, sizeof(event.src_mac));
    memcpy(&event.msg, data, sizeof(event.msg));
    (void)post_event(&event);
}

static esp_err_t add_peer(const uint8_t mac[ESP_NOW_ETH_ALEN])
{
    if (!mac) {
        return ESP_ERR_INVALID_ARG;
    }
    if (esp_now_is_peer_exist(mac)) {
        return ESP_OK;
    }

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, mac, sizeof(peer.peer_addr));
    peer.channel = CONFIG_STC_ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    const esp_err_t err = esp_now_add_peer(&peer);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "added peer " MACSTR, MAC2STR(mac));
    }
    return err;
}

static esp_err_t send_frame(const uint8_t mac[ESP_NOW_ETH_ALEN],
                            const stc_lidmsg_t *msg)
{
    if (!mac || !msg || s_tx_busy) {
        return ESP_ERR_INVALID_STATE;
    }

    s_tx_busy = true;
    const esp_err_t err = esp_now_send(mac, (const uint8_t *)msg, sizeof(*msg));
    if (err != ESP_OK) {
        s_tx_busy = false;
        s_tx_errors++;
        ESP_LOGE(TAG, "send failed: %s", esp_err_to_name(err));
    }
    return err;
}

static void init_wifi_and_espnow(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    const wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_STC_ESPNOW_CHANNEL,
                                         WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_callback));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(receive_callback));

#if STC_IS_NODE
    /* NODE_STATE is broadcast so the controller can discover the node MAC. */
    ESP_ERROR_CHECK(add_peer(k_broadcast_mac));
#endif

    uint8_t local_mac[ESP_NOW_ETH_ALEN] = {0};
    ESP_ERROR_CHECK(esp_read_mac(local_mac, ESP_MAC_WIFI_STA));
    ESP_LOGI(TAG,
             "role=%s mac=" MACSTR " channel=%d encryption=off",
             STC_ROLE_NAME, MAC2STR(local_mac),
             CONFIG_STC_ESPNOW_CHANNEL);
}

#if !STC_IS_NODE
static uint8_t s_node_mac[ESP_NOW_ETH_ALEN];
static bool s_node_known;
static bool s_node_online;
static int64_t s_last_node_state_us;
static uint8_t s_pending_ack_seq;
static bool s_ack_pending;
static uint16_t s_ack_timeouts;
static int64_t s_command_sent_us;
static uint8_t s_command_seq;
static uint8_t s_next_bin;
static int64_t s_next_command_us;

static bool controller_online(int64_t now_us)
{
    return s_node_known &&
           (now_us - s_last_node_state_us) <=
               (int64_t)CONTROLLER_OFFLINE_MS * 1000;
}

static void controller_receive(const link_event_t *event, int64_t now_us)
{
    const stc_lidmsg_t *msg = &event->msg;
    if (!stc_lidmsg_check(msg)) {
        s_crc_errors++;
        ESP_LOGW(TAG, "drop invalid frame from " MACSTR, MAC2STR(event->src_mac));
        return;
    }
    s_rx_ok++;

    if (msg->type == STC_LID_MSG_NODE_STATE) {
        if (is_broadcast(event->src_mac)) {
            return;
        }
        if (!s_node_known || !mac_equal(s_node_mac, event->src_mac)) {
            memcpy(s_node_mac, event->src_mac, sizeof(s_node_mac));
            s_node_known = true;
            const esp_err_t err = add_peer(s_node_mac);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "cannot add node peer: %s", esp_err_to_name(err));
                s_node_known = false;
                return;
            }
            ESP_LOGI(TAG, "node discovered: " MACSTR, MAC2STR(s_node_mac));
        }
        s_last_node_state_us = now_us;
        if (!s_node_online) {
            s_node_online = true;
            ESP_LOGI(TAG, "LINK UP");
        }
        return;
    }

    if (msg->type == STC_LID_MSG_ACK && s_ack_pending &&
        msg->seq == s_pending_ack_seq) {
        s_ack_pending = false;
        ESP_LOGI(TAG, "ACK seq=%u bin=%u rtt_us=%lld",
                 (unsigned)msg->seq, (unsigned)msg->bin,
                 (long long)(now_us - s_command_sent_us));
    }
}

static void controller_service(int64_t now_us)
{
    if (s_ack_pending &&
        (now_us - s_command_sent_us) >=
            (int64_t)CONTROLLER_ACK_TIMEOUT_MS * 1000) {
        s_ack_pending = false;
        s_ack_timeouts++;
        ESP_LOGW(TAG, "ACK timeout seq=%u total=%u",
                 (unsigned)s_pending_ack_seq, (unsigned)s_ack_timeouts);
    }

    if (s_node_online && !controller_online(now_us)) {
        s_node_online = false;
        s_ack_pending = false;
        ESP_LOGW(TAG, "LINK DOWN: no NODE_STATE for %d ms",
                 CONTROLLER_OFFLINE_MS);
    }
    if (!s_node_online || s_tx_busy || now_us < s_next_command_us) {
        return;
    }

    stc_lidmsg_t command = {0};
    command.sof = STC_LIDMSG_SOF;
    command.ver = STC_LIDMSG_VER;
    command.type = STC_LID_MSG_CMD;
    command.seq = s_command_seq++;
    command.bin = s_next_bin;
    command.action = 1;
    stc_lidmsg_seal(&command);

    s_pending_ack_seq = command.seq;
    s_ack_pending = true;
    s_command_sent_us = now_us;
    if (send_frame(s_node_mac, &command) == ESP_OK) {
        s_next_bin = (uint8_t)((s_next_bin + 1u) & 3u);
        ESP_LOGI(TAG, "sent LID_CMD seq=%u bin=%u",
                 (unsigned)command.seq, (unsigned)command.bin);
        s_next_command_us = now_us +
                            (int64_t)CONTROLLER_COMMAND_PERIOD_MS * 1000;
    } else {
        s_ack_pending = false;
        s_next_command_us = now_us + 100000;
    }
}
#else
static uint8_t s_state_seq;
static bool s_ack_queued;
static uint8_t s_ack_dest[ESP_NOW_ETH_ALEN];
static stc_lidmsg_t s_ack_msg;
static int64_t s_next_state_us;

static void node_receive(const link_event_t *event)
{
    const stc_lidmsg_t *msg = &event->msg;
    if (!stc_lidmsg_check(msg)) {
        s_crc_errors++;
        ESP_LOGW(TAG, "drop invalid frame from " MACSTR, MAC2STR(event->src_mac));
        return;
    }
    s_rx_ok++;

    if (msg->type != STC_LID_MSG_CMD) {
        return;
    }
    if (msg->bin >= 4u || is_broadcast(event->src_mac)) {
        ESP_LOGW(TAG, "reject LID_CMD bin=%u from invalid source",
                 (unsigned)msg->bin);
        return;
    }
    const esp_err_t err = add_peer(event->src_mac);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "cannot add controller peer: %s", esp_err_to_name(err));
        return;
    }

    memcpy(s_ack_dest, event->src_mac, sizeof(s_ack_dest));
    s_ack_msg = *msg;
    s_ack_msg.type = STC_LID_MSG_ACK;
    s_ack_msg.action = 0;
    memset(s_ack_msg.tof_mm, 0, sizeof(s_ack_msg.tof_mm));
    stc_lidmsg_seal(&s_ack_msg);
    s_ack_queued = true;

    ESP_LOGI(TAG, "received LID_CMD seq=%u bin=%u action=%u",
             (unsigned)msg->seq, (unsigned)msg->bin, (unsigned)msg->action);
}

static void node_service(int64_t now_us)
{
    if (s_tx_busy) {
        return;
    }

    if (s_ack_queued) {
        if (send_frame(s_ack_dest, &s_ack_msg) == ESP_OK) {
            s_ack_queued = false;
            ESP_LOGI(TAG, "sent LID_ACK seq=%u", (unsigned)s_ack_msg.seq);
        }
        return;
    }

    if (now_us < s_next_state_us) {
        return;
    }

    stc_lidmsg_t state = {0};
    state.sof = STC_LIDMSG_SOF;
    state.ver = STC_LIDMSG_VER;
    state.type = STC_LID_MSG_NODE_STATE;
    state.seq = s_state_seq++;
    /* This spike has no TOF hardware attached. Zero means unavailable. */
    stc_lidmsg_seal(&state);
    if (send_frame(k_broadcast_mac, &state) == ESP_OK) {
        s_next_state_us = now_us + (int64_t)NODE_STATE_PERIOD_MS * 1000;
    } else {
        s_next_state_us = now_us + 50000;
    }
}
#endif

static void process_event(const link_event_t *event, int64_t now_us)
{
    if (event->type == LINK_EVENT_TX_DONE) {
        s_tx_busy = false;
        if (event->status != ESP_NOW_SEND_SUCCESS) {
            s_tx_errors++;
            ESP_LOGW(TAG, "tx callback failed to " MACSTR,
                     MAC2STR(event->src_mac));
        }
        return;
    }

#if STC_IS_NODE
    node_receive(event);
    (void)now_us;
#else
    controller_receive(event, now_us);
#endif
}

static void link_task(void *arg)
{
    (void)arg;
    while (true) {
        link_event_t event;
        if (xQueueReceive(s_event_queue, &event, pdMS_TO_TICKS(20)) == pdTRUE) {
            process_event(&event, esp_timer_get_time());
        }
        const int64_t now_us = esp_timer_get_time();
#if STC_IS_NODE
        node_service(now_us);
#else
        controller_service(now_us);
#endif
    }
}

void app_main(void)
{
    s_event_queue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(link_event_t));
    if (!s_event_queue) {
        ESP_LOGE(TAG, "event queue allocation failed");
        return;
    }

    init_wifi_and_espnow();
    ESP_LOGI(TAG, "spike ready; no servos, TOF, camera, or UI are driven");

    if (xTaskCreate(link_task, "stc_espnow", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "link task creation failed");
    }
}
