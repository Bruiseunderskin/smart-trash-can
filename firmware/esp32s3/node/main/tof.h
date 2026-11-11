/* tof.h — 四粒 TOF050C（VL6180X），薄層包住 nestorit/vl6180x 元件
 *
 * 底層（model ID 檢查、AN4545 初始化、連續測距、狀態解碼）全部交給
 * `nestorit/vl6180x` v1.0.0（ESP Component Registry）；本層只負責這塊板子
 * 特有的兩件事：XSHUT 逐支改址（0x30–0x33）與「只在新且有效的樣本才回值」。
 */
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOF_COUNT 4
/* 第一粒改址後的地址；bin i → 0x30 + i（位址只在通電期間有效）。 */
#define TOF_FIRST_ADDR 0x30

esp_err_t tof_init(i2c_master_bus_handle_t bus);
/* ESP_OK = 有新的有效距離；ESP_ERR_NOT_FINISHED = 還沒有新樣本；
 * ESP_ERR_INVALID_RESPONSE = 有新樣本但 range status 不是 0（值已丟棄）。
 * 永遠不會回上一輪的舊值。 */
esp_err_t tof_poll(uint8_t index, uint16_t *mm);
uint8_t tof_fault_mask(void);

#ifdef __cplusplus
}
#endif
