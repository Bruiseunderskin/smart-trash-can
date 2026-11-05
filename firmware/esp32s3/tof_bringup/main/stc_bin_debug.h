#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int8_t offset;
    uint16_t xtalk;
    uint16_t last_mm;
    bool fault;
} stc_bin_debug_info_t;

stc_err_t stc_bin_debug_info(stc_bin_t bin, stc_bin_debug_info_t *info);
stc_err_t stc_bin_calibrate_offset(stc_bin_t bin, uint16_t target_mm);

#ifdef __cplusplus
}
#endif
