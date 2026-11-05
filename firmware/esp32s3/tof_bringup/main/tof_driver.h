#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t stc_tof_bus_init(void);
esp_err_t stc_tof_bus_recover(void);
esp_err_t stc_tof_probe(uint8_t channel);

esp_err_t stc_tof_sensor_init(uint8_t channel, int8_t *factory_offset,
                              uint16_t *factory_xtalk);
esp_err_t stc_tof_start_continuous(uint8_t channel);
esp_err_t stc_tof_read(uint8_t channel, uint16_t *distance_mm,
                       uint32_t timeout_ms);

esp_err_t stc_tof_get_offset(uint8_t channel, int8_t *offset);
esp_err_t stc_tof_set_offset(uint8_t channel, int8_t offset);
esp_err_t stc_tof_get_xtalk(uint8_t channel, uint16_t *rate);
esp_err_t stc_tof_set_xtalk(uint8_t channel, uint16_t rate);

#ifdef __cplusplus
}
#endif
