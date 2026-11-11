/* test_label_map.c — 四個映射值，以及錯序 labels.txt 必須 fail（#53 §6.1） */
#include <stdio.h>
#include <string.h>

#include "stc_vision_map.h"

static int write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fputs(text, f);
    fclose(f);
    return 0;
}

int main(void)
{
    static const uint8_t expected[4] = {1, 2, 3, 0};
    for (uint8_t i = 0; i < 4; ++i) {
        if (stc_model_to_bin[i] != expected[i]) {
            fprintf(stderr, "table index %u: %u != %u\n", i, stc_model_to_bin[i], expected[i]);
            return 1;
        }
    }

    if (write_file("labels_ok.txt", "food\nhazardous\nother\nrecyclable\n") != 0) return 2;
    if (stc_vision_map_load_labels("labels_ok.txt") != STC_OK) {
        fprintf(stderr, "correct labels rejected\n");
        return 1;
    }
    for (uint8_t i = 0; i < 4; ++i) {
        if ((uint8_t)stc_vision_map_bin(i) != expected[i]) {
            fprintf(stderr, "mapped bin %u wrong after load\n", i);
            return 1;
        }
    }

    if (write_file("labels_bad.txt", "recyclable\nfood\nhazardous\nother\n") != 0) return 2;
    if (stc_vision_map_load_labels("labels_bad.txt") != STC_ERR_CONTRACT) {
        fprintf(stderr, "wrong order accepted\n");
        return 1;
    }
    if (stc_vision_map_bin(0) != STC_BIN_COUNT) {
        fprintf(stderr, "mapping still usable after a contract error\n");
        return 1;
    }

    if (stc_vision_map_load_labels("does_not_exist.txt") != STC_ERR_IO) {
        fprintf(stderr, "missing file did not return IO\n");
        return 1;
    }

    printf("test_label_map: 4 mappings, bad order rejected\n");
    return 0;
}
