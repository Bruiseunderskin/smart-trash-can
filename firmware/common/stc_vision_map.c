#include "stc_vision_map.h"

#include <stdio.h>
#include <string.h>

const uint8_t stc_model_to_bin[STC_MODEL_CLASSES] = {1u, 2u, 3u, 0u, STC_BIN_COUNT};

static const char *const k_expected[STC_MODEL_CLASSES] = {
    "food", "hazardous", "other", "recyclable", "unknown"
};

static char g_loaded[STC_MODEL_CLASSES][16];
static bool g_ready = false;

stc_err_t stc_vision_map_load_labels(const char *path)
{
    if (!path) {
        return STC_ERR_IO;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        g_ready = false;
        return STC_ERR_IO;
    }

    char line[32];
    int index = 0;
    stc_err_t rc = STC_OK;
    while (index < STC_MODEL_CLASSES && fgets(line, sizeof line, f)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')) {
            line[--len] = '\0';
        }
        if (len == 0) {
            continue;
        }
        if (strcmp(line, k_expected[index]) != 0) {
            rc = STC_ERR_CONTRACT;
            break;
        }
        memcpy(g_loaded[index], line, 15); g_loaded[index][15] = '\0';
        index++;
    }
    fclose(f);

    if (rc == STC_OK && index != STC_MODEL_CLASSES) {
        rc = STC_ERR_CONTRACT;   /* 少過五行 = 契約不符，不默認 */
    }
    g_ready = (rc == STC_OK);
    return rc;
}

stc_bin_t stc_vision_map_bin(uint8_t class_index)
{
    if (!g_ready || class_index >= STC_MODEL_CLASSES) {
        return STC_BIN_COUNT;
    }
    return (stc_bin_t)stc_model_to_bin[class_index];
}

const char *stc_vision_map_label(uint8_t class_index)
{
    if (!g_ready || class_index >= STC_MODEL_CLASSES) {
        return NULL;
    }
    return g_loaded[class_index];
}
