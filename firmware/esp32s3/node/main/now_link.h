/* now_link.h — S31 ↔ S3 的控制鏈路，用 ESP-NOW 運同一份 stc_lidmsg 幀
 *
 * 2026-11 硬件改版後鏈路走 ESP-NOW（Wi-Fi channel 由 menuconfig 設定），
 * 節點開機先廣播 NODE_STATE，收到 S31 命令後記住對方 MAC 並單播回覆。
 * 幀格式（16 bytes、CRC-16/CCITT-FALSE）不變，只是換了底層傳輸。
 */
#pragma once

#include "esp_err.h"
#include "stc_lidmsg.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*now_link_recv_cb_t)(const stc_lidmsg_t *msg, const uint8_t *mac);

esp_err_t now_link_init(now_link_recv_cb_t on_frame);
int now_link_send(const stc_lidmsg_t *msg, const uint8_t *mac);
/* 最近一次收到命令的對方 MAC；未收到過就回 false */
bool now_link_peer(uint8_t out_mac[6]);

#ifdef __cplusplus
}
#endif
