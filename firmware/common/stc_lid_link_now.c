#include "stc_lid_link_now.h"

#include <string.h>

/* ESP32-S31 側的 ESP-NOW 傳輸。Wi-Fi 與 esp_now_init() 由 App 先做好；
 * 這裡只負責把 16 bytes 幀送去節點 MAC。Host 編譯時退回 -1：
 * 可編譯、可連結，但不會假裝鏈路可用。 */
#if defined(ESP_PLATFORM) && __has_include("esp_now.h")
#include "esp_now.h"

static uint8_t g_peer[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static int now_send(const stc_lidmsg_t *msg, void *ctx)
{
    (void)ctx;
    return esp_now_send(g_peer, (const uint8_t *)msg, sizeof *msg) == ESP_OK ? 0 : -1;
}

int stc_lid_link_now_attach(void)
{
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof peer);
    memcpy(peer.peer_addr, g_peer, sizeof g_peer);
    peer.channel = 0;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    if (!esp_now_is_peer_exist(g_peer) && esp_now_add_peer(&peer) != ESP_OK) {
        return -1;
    }
    stc_lid_transport_t tx = {.send = now_send, .ctx = NULL};
    stc_lid_link_init(&tx);
    return 0;
}

void stc_lid_link_now_set_peer(const uint8_t mac[6])
{
    if (!mac) {
        return;
    }
    memcpy(g_peer, mac, sizeof g_peer);
}
#else
static int now_send(const stc_lidmsg_t *msg, void *ctx)
{
    (void)msg;
    (void)ctx;
    return -1;
}

int stc_lid_link_now_attach(void)
{
    stc_lid_transport_t tx = {.send = now_send, .ctx = NULL};
    stc_lid_link_init(&tx);
    return 0;
}

void stc_lid_link_now_set_peer(const uint8_t mac[6]) { (void)mac; }
#endif
