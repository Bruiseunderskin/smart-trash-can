/* test_rgb565.c — RGB888 camera pixels to native RGB565 preview pixels. */
#include <stdint.h>
#include <stdio.h>

#include "stc_preprocess.h"

int main(void)
{
    const uint8_t rgb[] = {
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255,
        0, 0, 0,
    };
    uint16_t out[5] = {0};
    if (stc_preprocess_rgb_to_rgb565(rgb, 5, out) != STC_OK) {
        return 1;
    }
    const uint16_t expected[] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000};
    for (int i = 0; i < 5; ++i) {
        if (out[i] != expected[i]) {
            fprintf(stderr, "pixel %d expected=%04x actual=%04x\n",
                    i, expected[i], out[i]);
            return 1;
        }
    }
    puts("test_rgb565: RGB888 conversion OK");
    return 0;
}
