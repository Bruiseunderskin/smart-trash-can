#include "calibration.h"

bool stc_offset_correct(int8_t current_offset, uint16_t measured_mm,
                        uint16_t target_mm, int8_t *corrected_offset) {
    if (!corrected_offset || measured_mm == 0 || target_mm == 0) {
        return false;
    }
    int32_t value =
        (int32_t)current_offset + (int32_t)measured_mm - (int32_t)target_mm;
    if (value < INT8_MIN) {
        value = INT8_MIN;
    } else if (value > INT8_MAX) {
        value = INT8_MAX;
    }
    *corrected_offset = (int8_t)value;
    return true;
}
