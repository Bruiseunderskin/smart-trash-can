/* now_link.c — Wi-Fi STA + ESP-NOW，廣播開機、收到命令後記住對方 MAC */

#include "now_link.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "now_link";

static uint8_t s_broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t s_peer[6];
static bool s_have_peer;
static now_link_recv_cb_t s_cb;

static void add_peer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) {
    return;
  }
  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof peer);
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  esp_err_t err = esp_now_add_peer(&peer);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "add peer %02x:%02x:%02x:%02x:%02x:%02x failed: %s", mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5], esp_err_to_name(err));
  }
}

static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (!info || !data || len != (int)sizeof(stc_lidmsg_t)) {
    return;
  }
  stc_lidmsg_t msg;
  memcpy(&msg, data, sizeof msg);
  if (!stc_lidmsg_check(&msg)) {
    return;
  }
  if (memcmp(info->src_addr, s_peer, 6) != 0) {
    memcpy(s_peer, info->src_addr, 6);
    s_have_peer = true;
    add_peer(s_peer);
  }
  if (s_cb) {
    s_cb(&msg, info->src_addr);
  }
}

esp_err_t now_link_init(now_link_recv_cb_t on_frame) {
  s_cb = on_frame;

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase");
    err = nvs_flash_init();
  }
  ESP_RETURN_ON_ERROR(err, TAG, "nvs");
  ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
  ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init");
  ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "wifi storage");
  ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi mode");
  ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");
  ESP_RETURN_ON_ERROR(esp_wifi_set_channel(CONFIG_STC_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE), TAG,
                      "channel");

  ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "esp-now init");
  ESP_RETURN_ON_ERROR(esp_now_register_recv_cb(on_recv), TAG, "esp-now recv cb");
  add_peer(s_broadcast);

  uint8_t mac[6] = {0};
  (void)esp_wifi_get_mac(WIFI_IF_STA, mac);
  ESP_LOGI(TAG, "node mac %02x:%02x:%02x:%02x:%02x:%02x ch=%d", mac[0], mac[1], mac[2], mac[3],
           mac[4], mac[5], CONFIG_STC_NOW_CHANNEL);
  return ESP_OK;
}

int now_link_send(const stc_lidmsg_t *msg, const uint8_t *mac) {
  if (!msg) {
    return -1;
  }
  const uint8_t *to = mac ? mac : s_broadcast;
  esp_err_t err = esp_now_send(to, (const uint8_t *)msg, sizeof *msg);
  return err == ESP_OK ? 0 : -1;
}

bool now_link_peer(uint8_t out_mac[6]) {
  if (!s_have_peer) {
    return false;
  }
  memcpy(out_mac, s_peer, 6);
  return true;
}
