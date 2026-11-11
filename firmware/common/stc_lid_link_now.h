/* stc_lid_link_now.h — S31 側的 ESP-NOW 傳輸（2026-11 硬件改版）
 * 幀仍是 stc_lidmsg 16 bytes；底層走 Wi-Fi 的 ESP-NOW。
 * 呼叫 stc_lid_link_now_attach() 之前，App 要先開好 Wi-Fi STA 與 esp_now_init()。
 */
#pragma once

#include "stc_lid_link.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 安裝 ESP-NOW 傳輸；0 = 已安裝，負 = 失敗。
 * Host（非 ESP32）編譯時照樣回 0，但 send 一律回 -1，令 CI 可連結。 */
int stc_lid_link_now_attach(void);

/* 指定節點 MAC（6 bytes）；未指定時送往廣播地址。 */
void stc_lid_link_now_set_peer(const uint8_t mac[6]);

#ifdef __cplusplus
}
#endif
