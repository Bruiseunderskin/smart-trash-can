#pragma once

#include <stdint.h>

#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void stc_camera_capture_set_i2c_bus(void *i2c_bus);
stc_err_t stc_camera_capture_begin(void);
stc_err_t stc_camera_capture_frame(uint8_t *rgb, uint32_t timeout_ms);
void stc_camera_capture_end(void);

#ifdef __cplusplus
}
#endif
