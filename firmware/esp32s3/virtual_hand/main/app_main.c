#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "driver/usb_serial_jtag.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "stc_hand_msg.h"
#include "stc_virtual_hand.h"

static const char *TAG = "VIRTUAL_HAND";
static const uint8_t k_broadcast[ESP_NOW_ETH_ALEN] =
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static QueueHandle_t g_rx_queue;
static SemaphoreHandle_t g_mac_mutex;
static uint8_t g_controller_mac[ESP_NOW_ETH_ALEN];
static bool g_controller_known;
static uint8_t g_heartbeat_seq;

typedef struct {
    uint8_t src[ESP_NOW_ETH_ALEN];
    stc_hand_msg_t msg;
} rx_item_t;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
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
        rx_item_t item = {.msg = msg};
        memcpy(item.src, info->src_addr, sizeof item.src);
        (void)xQueueSend(g_rx_queue, &item, pdMS_TO_TICKS(10));
    }
}

static void send_callback(const esp_now_send_info_t *info,
                          esp_now_send_status_t status)
{
    if (info && info->des_addr && status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "send failed to " MACSTR, MAC2STR(info->des_addr));
    }
}

static esp_err_t wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_callback));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(receive_callback));
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, k_broadcast, sizeof peer.peer_addr);
    peer.channel = 6;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    return esp_now_add_peer(&peer);
}

static void remember_controller(const uint8_t mac[ESP_NOW_ETH_ALEN])
{
    xSemaphoreTake(g_mac_mutex, portMAX_DELAY);
    if (!g_controller_known || memcmp(g_controller_mac, mac, ESP_NOW_ETH_ALEN) != 0) {
        memcpy(g_controller_mac, mac, ESP_NOW_ETH_ALEN);
        g_controller_known = true;
        if (!esp_now_is_peer_exist(mac)) {
            esp_now_peer_info_t peer = {0};
            memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
            peer.channel = 6;
            peer.ifidx = WIFI_IF_STA;
            peer.encrypt = false;
            (void)esp_now_add_peer(&peer);
        }
        ESP_LOGI(TAG, "controller " MACSTR, MAC2STR(mac));
    }
    xSemaphoreGive(g_mac_mutex);
}

static void send_outputs(const stc_virtual_hand_output_t *out)
{
    for (uint8_t i = 0; i < out->count; ++i) {
        const stc_hand_msg_t *msg = &out->msgs[i];
        const uint8_t *dest = msg->type == STC_HAND_MSG_ACK
                                  ? g_controller_mac
                                  : k_broadcast;
        if (msg->type == STC_HAND_MSG_ACK && !g_controller_known) {
            continue;
        }
        const esp_err_t err =
            esp_now_send(dest, (const uint8_t *)msg, sizeof(*msg));
        ESP_LOGI(TAG,
                 "TX type=%u round=%u seq=%u bin=%u action=%u dest=%s err=%s",
                 (unsigned)msg->type, (unsigned)msg->round,
                 (unsigned)msg->seq, (unsigned)msg->bin,
                 (unsigned)msg->action,
                 msg->type == STC_HAND_MSG_ACK ? "controller" : "broadcast",
                 esp_err_to_name(err));
    }
}

static void send_heartbeat(void)
{
    stc_hand_msg_t state = {0};
    state.sof = STC_HAND_MSG_SOF;
    state.ver = STC_HAND_MSG_VERSION;
    state.type = STC_HAND_MSG_STATE;
    state.seq = g_heartbeat_seq++;
    state.bin = 0xFF;
    state.flags = STC_HAND_FLAG_READY | STC_HAND_FLAG_VIRTUAL;
    if (stc_virtual_hand_open()) state.flags |= STC_HAND_FLAG_OPEN;
    stc_hand_msg_seal(&state);
    (void)esp_now_send(k_broadcast, (const uint8_t *)&state, sizeof state);
}

static int parse_bins(const char *text, uint8_t out[5])
{
    char buffer[64];
    snprintf(buffer, sizeof buffer, "%s", text);
    char *save = NULL;
    char *token = strtok_r(buffer, ", ", &save);
    int count = 0;
    while (token && count < 5) {
        char *end = NULL;
        long value = strtol(token, &end, 10);
        if (!end || *end || value < 0 || value > 3) return -1;
        out[count++] = (uint8_t)value;
        token = strtok_r(NULL, ", ", &save);
    }
    return count == 5 && !token ? 0 : -1;
}

static void print_status(void)
{
    const uint8_t *pattern = stc_virtual_hand_pattern();
    printf("VIRTUAL_HAND status open=%d delay=%u pattern=%u,%u,%u,%u,%u\n",
           stc_virtual_hand_open(), (unsigned)stc_virtual_hand_delay(),
           pattern[0], pattern[1], pattern[2], pattern[3], pattern[4]);
}

static void console_task(void *arg)
{
    (void)arg;
    char line[96];
    size_t used = 0;
    while (true) {
        uint8_t ch;
#if CONFIG_ESP_CONSOLE_UART_DEFAULT
        const int n = read(STDIN_FILENO, &ch, 1);
#else
        const int n = usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(100));
#endif
        if (n <= 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            if (used == 0) continue;
            line[used] = '\0';
            used = 0;
            if (strcmp(line, "help") == 0) {
                printf("VIRTUAL_HAND commands: drop 0-3 | pattern a,b,c,d,e | "
                       "delay ms | status | help\n");
            } else if (strncmp(line, "drop ", 5) == 0) {
                stc_virtual_hand_set_next_drop((int8_t)atoi(line + 5));
                printf("VIRTUAL_HAND next_drop=%s\n", line + 5);
            } else if (strncmp(line, "pattern ", 8) == 0) {
                uint8_t pattern[5];
                if (parse_bins(line + 8, pattern) == 0) {
                    stc_virtual_hand_set_pattern(pattern);
                    print_status();
                } else {
                    printf("VIRTUAL_HAND invalid pattern\n");
                }
            } else if (strncmp(line, "delay ", 6) == 0) {
                stc_virtual_hand_set_delay((uint32_t)strtoul(line + 6, NULL, 10));
                print_status();
            } else if (strcmp(line, "status") == 0) {
                print_status();
            } else {
                printf("VIRTUAL_HAND unknown command; type help\n");
            }
            continue;
        }
        if (isprint(ch) && used < sizeof(line) - 1) {
            line[used++] = (char)ch;
        }
    }
}

void app_main(void)
{
    const esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(nvs);
    }

    g_rx_queue = xQueueCreate(16, sizeof(rx_item_t));
    g_mac_mutex = xSemaphoreCreateMutex();
    if (!g_rx_queue || !g_mac_mutex) return;

    stc_virtual_hand_init();
    ESP_ERROR_CHECK(wifi_init());
    /* The configured USB Serial/JTAG console installs this driver during VFS
     * startup. Re-installing it would abort before the virtual hand starts. */
    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t config =
            USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&config));
    }
#if CONFIG_ESP_CONSOLE_UART_DEFAULT
    const int stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (stdin_flags < 0 ||
        fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK) < 0) {
        ESP_LOGE(TAG, "failed to make UART stdin non-blocking");
        return;
    }
#endif
    ESP_ERROR_CHECK(xTaskCreate(console_task, "vh_console", 4096, NULL, 4, NULL) == pdPASS
                        ? ESP_OK : ESP_FAIL);

    uint8_t mac[ESP_NOW_ETH_ALEN];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    ESP_LOGI(TAG, "ready mac=" MACSTR " channel=6 encryption=off",
             MAC2STR(mac));

    uint32_t next_heartbeat = 0;
    while (true) {
        rx_item_t item;
        if (xQueueReceive(g_rx_queue, &item, pdMS_TO_TICKS(20)) == pdTRUE) {
            remember_controller(item.src);
            stc_virtual_hand_output_t command_out = {0};
            if (item.msg.type == STC_HAND_MSG_CMD) {
                stc_virtual_hand_on_command(&item.msg, now_ms(), &command_out);
                send_outputs(&command_out);
            } else if (item.msg.type == STC_HAND_MSG_ACK) {
                stc_virtual_hand_on_ack(&item.msg);
            }
        }
        const uint32_t now = now_ms();
        stc_virtual_hand_output_t out;
        stc_virtual_hand_poll(now, &out);
        send_outputs(&out);
        if ((int32_t)(now - next_heartbeat) >= 0) {
            send_heartbeat();
            next_heartbeat = now + 250;
        }
    }
}
