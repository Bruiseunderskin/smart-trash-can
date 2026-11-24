#include "stc_preprocess.h"

#include <stdio.h>
#include <string.h>

#define W 8
#define H 4

int main(void)
{
    uint8_t uyvy[W * H * 2];
    uint8_t rgb[2 * 2 * 3];
    memset(uyvy, 0, sizeof(uyvy));

    for (int i = 0; i < W * H * 2; i += 4) {
        uyvy[i + 0] = 128; /* U */
        uyvy[i + 1] = 235; /* Y0 */
        uyvy[i + 2] = 128; /* V */
        uyvy[i + 3] = 235; /* Y1 */
    }

    if (stc_preprocess_uyvy_to_rgb_preview(uyvy, W, H, rgb, 2) != STC_OK) {
        fprintf(stderr, "preview preprocessing failed\n");
        return 1;
    }
    for (int i = 0; i < 2 * 2; ++i) {
        if (rgb[i * 3 + 0] != 255 || rgb[i * 3 + 1] != 255 ||
            rgb[i * 3 + 2] != 255) {
            fprintf(stderr, "pixel %d was not neutral white\n", i);
            return 2;
        }
    }
    return 0;
}
