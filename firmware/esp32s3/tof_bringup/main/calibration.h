#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compute the signed part-to-part range offset for a measured target.
 * VL6180X subtracts the programmed positive offset from the raw range.
 */
bool stc_offset_correct(int8_t current_offset, uint16_t measured_mm,
                        uint16_t target_mm, int8_t *corrected_offset);

#ifdef __cplusplus
}
#endif
