/* servo.h — 四蓋舵機，各自一條 GPIO 直出 LEDC PWM（硬件 PCB）
 *
 * 每個桶一支舵機，各自一條 GPIO。50 Hz、脈寬 500–2500 µs。
 * 校正值用 menuconfig 設定，因為實際開合角度要等實物量。
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STC_SERVO_COUNT 4

esp_err_t servo_init(void);
esp_err_t servo_set_us(uint8_t bin, uint16_t pulse_us);
esp_err_t servo_open(uint8_t bin);
esp_err_t servo_close_all(void);

#ifdef __cplusplus
}
#endif
