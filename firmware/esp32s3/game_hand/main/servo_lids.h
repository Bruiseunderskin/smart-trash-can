#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STC_LID_COUNT 4

esp_err_t stc_servo_lids_init(void);
esp_err_t stc_servo_lids_open_all(void);
esp_err_t stc_servo_lids_close_all(void);
esp_err_t stc_servo_lids_stop_all(void);

#ifdef __cplusplus
}
#endif
