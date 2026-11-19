#include "espnow_controller.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

static const char *TAG = "stc_espnow_s31";

#define STC_ESPNOW_TRY(expr)                                                   \
    do {                                                                       \
        const esp_err_t err_ = (expr);                                         \
        if (err_ != ESP_OK) {                                                  \
            ESP_LOGE(TAG, "%s failed: %s", #expr, esp_err_to_name(err_));      \
            return STC_ERR_IO;                                                 \
        }                                                                      \
    } while (0)

static QueueHandle_t g_rx_queue;
static SemaphoreHandle_t g_mac_mutex;
static uint8_t g_node_mac[ESP_NOW_ETH_ALEN];
static bool g_node_known;
static uint8_t g_channel = 6;
static bool g_espnow_ready;
static bool g_suspended;

static stc_err_t wifi_init_or_already_initialized(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.static_rx_buf_num = 2;
    cfg.dynamic_rx_buf_num = 4;
    cfg.tx_buf_type = 0;
    cfg.static_tx_buf_num = 4;
    cfg.dynamic_tx_buf_num = 0;
    cfg.rx_mgmt_buf_type = 0;
    cfg.rx_mgmt_buf_num = 2;
    cfg.mgmt_sbuf_num = 8;
    cfg.ampdu_rx_enable = 0;
    cfg.ampdu_tx_enable = 0;
    cfg.nvs_enable = 0;
    cfg.cache_tx_buf_num = 0;
    cfg.espnow_max_encrypt_num = 0;
    const esp_err_t err = esp_wifi_init(&cfg);
    return err == ESP_OK || err == ESP_ERR_INVALID_STATE
               ? STC_OK
               : STC_ERR_IO;
}

static stc_err_t start_wifi_for_espnow(void)
{
    esp_err_t err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return STC_ERR_IO;
    }
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        return STC_ERR_IO;
    }
    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return STC_ERR_IO;
    }
    err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (err != ESP_OK) {
        return STC_ERR_IO;
    }
    err = esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW20);
    if (err != ESP_OK) {
        return STC_ERR_IO;
    }
    err = esp_wifi_set_channel(g_channel, WIFI_SECOND_CHAN_NONE);
    return err == ESP_OK ? STC_OK : STC_ERR_IO;
}

static bool post_peer_frame(const uint8_t src[ESP_NOW_ETH_ALEN],
                            const stc_hand_msg_t *msg)
{
    if (!g_rx_queue) {
        return false;
    }
    if (msg->type == STC_HAND_MSG_STATE) {
        xSemaphoreTake(g_mac_mutex, pdMS_TO_TICKS(10));
        if (!g_node_known || memcmp(g_node_mac, src, ESP_NOW_ETH_ALEN) != 0) {
            memcpy(g_node_mac, src, ESP_NOW_ETH_ALEN);
            g_node_known = true;
            esp_now_peer_info_t peer = {0};
            memcpy(peer.peer_addr, src, ESP_NOW_ETH_ALEN);
            peer.channel = g_channel;
            peer.ifidx = WIFI_IF_STA;
            peer.encrypt = false;
            if (!esp_now_is_peer_exist(src)) {
                (void)esp_now_add_peer(&peer);
            }
            ESP_LOGI(TAG, "real hand discovered " MACSTR, MAC2STR(src));
        }
        xSemaphoreGive(g_mac_mutex);
    }
    return xQueueSend(g_rx_queue, msg, pdMS_TO_TICKS(10)) == pdTRUE;
}

static void send_callback(const esp_now_send_info_t *info,
                          esp_now_send_status_t status)
{
    if (info && info->des_addr &&
        status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "send failed " MACSTR, MAC2STR(info->des_addr));
    }
}

static void receive_callback(const esp_now_recv_info_t *info,
                             const uint8_t *data, int len)
{
    if (!info || !info->src_addr || !data ||
        len != (int)sizeof(stc_hand_msg_t)) {
        return;
    }
    stc_hand_msg_t msg;
    memcpy(&msg, data, sizeof msg);
    if (stc_hand_msg_check(&msg)) {
        (void)post_peer_frame(info->src_addr, &msg);
    }
}

stc_err_t stc_game_espnow_init(uint8_t channel)
{
    g_channel = channel;
    if (!g_rx_queue) {
        g_rx_queue = xQueueCreate(16, sizeof(stc_hand_msg_t));
    }
    if (!g_mac_mutex) {
        g_mac_mutex = xSemaphoreCreateMutex();
    }
    if (!g_rx_queue || !g_mac_mutex) {
        return STC_ERR_IO;
    }
    if (g_espnow_ready) {
        return stc_game_espnow_resume(channel);
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return STC_ERR_IO;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return STC_ERR_IO;
    }
    if (wifi_init_or_already_initialized() != STC_OK ||
        start_wifi_for_espnow() != STC_OK) {
        return STC_ERR_IO;
    }

    err = esp_now_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(err));
        return STC_ERR_IO;
    }
    STC_ESPNOW_TRY(esp_now_register_send_cb(send_callback));
    STC_ESPNOW_TRY(esp_now_register_recv_cb(receive_callback));
    g_espnow_ready = true;
    g_suspended = false;
    ESP_LOGI(TAG, "controller ready channel=%u encryption=off",
             (unsigned)channel);
    return STC_OK;
}

stc_err_t stc_game_espnow_suspend(void)
{
    g_suspended = true;
    if (g_rx_queue) {
        (void)xQueueReset(g_rx_queue);
    }
    const esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGE(TAG, "esp_wifi_stop failed: %s", esp_err_to_name(err));
        return STC_ERR_IO;
    }
    ESP_LOGI(TAG, "radio suspended for CHAT");
    return STC_OK;
}

stc_err_t stc_game_espnow_resume(uint8_t channel)
{
    if (!g_espnow_ready) {
        return stc_game_espnow_init(channel);
    }
    g_channel = channel;
    if (start_wifi_for_espnow() != STC_OK) {
        ESP_LOGE(TAG, "could not restore ESP-NOW radio");
        return STC_ERR_IO;
    }
    if (g_node_known && g_mac_mutex) {
        xSemaphoreTake(g_mac_mutex, portMAX_DELAY);
        esp_now_peer_info_t peer = {0};
        if (esp_now_get_peer(g_node_mac, &peer) == ESP_OK) {
            peer.channel = g_channel;
            peer.ifidx = WIFI_IF_STA;
            peer.encrypt = false;
            const esp_err_t err = esp_now_mod_peer(&peer);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "peer channel restore failed: %s",
                         esp_err_to_name(err));
            }
        }
        xSemaphoreGive(g_mac_mutex);
    }
    g_suspended = false;
    ESP_LOGI(TAG, "radio resumed channel=%u", (unsigned)channel);
    return STC_OK;
}

int stc_game_espnow_send(const stc_hand_msg_t *msg, void *ctx)
{
    (void)ctx;
    if (!msg || !g_node_known || !g_mac_mutex || g_suspended) {
        return -1;
    }
    xSemaphoreTake(g_mac_mutex, portMAX_DELAY);
    const esp_err_t err =
        esp_now_send(g_node_mac, (const uint8_t *)msg, sizeof(*msg));
    xSemaphoreGive(g_mac_mutex);
    return err == ESP_OK ? 0 : -1;
}

bool stc_game_espnow_poll(stc_hand_msg_t *msg, uint32_t timeout_ms)
{
    return msg && g_rx_queue &&
           xQueueReceive(g_rx_queue, msg, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}
