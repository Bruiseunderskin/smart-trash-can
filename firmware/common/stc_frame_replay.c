#include "stc_frame_source.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#define MAX_FRAMES 8

static char g_dir[256];
static char g_files[MAX_FRAMES][128];
static int g_count;
static int g_next;
static int g_loaded;

static int load_dir(void)
{
    g_count = 0;
    g_next = 0;
    DIR *d = opendir(g_dir);
    if (!d) {
        return -1;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL && g_count < MAX_FRAMES) {
        size_t len = strlen(e->d_name);
        if (len < 5 || strcmp(e->d_name + len - 5, ".uyvy") != 0) {
            continue;
        }
        snprintf(g_files[g_count], sizeof g_files[g_count], "%.127s", e->d_name);
        g_count++;
    }
    closedir(d);
    return g_count > 0 ? 0 : -1;
}

static stc_err_t replay_open(void *ctx, uint32_t timeout_ms)
{
    (void)ctx;
    (void)timeout_ms;
    g_loaded = (load_dir() == 0);
    return g_loaded ? STC_OK : STC_ERR_IO;
}

static stc_err_t replay_capture(void *ctx, uint8_t *out, size_t capacity, uint32_t *t_ms)
{
    (void)ctx;
    if (!g_loaded || g_count == 0 || !out || capacity < STC_FRAME_BYTES) {
        return STC_ERR_IO;
    }
    char path[512];
    snprintf(path, sizeof path, "%.255s/%.127s", g_dir, g_files[g_next]);
    FILE *f = fopen(path, "rb");
    if (!f) {
        return STC_ERR_IO;
    }
    size_t n = fread(out, 1, STC_FRAME_BYTES, f);
    fclose(f);
    if (n != STC_FRAME_BYTES) {
        return STC_ERR_IO;
    }
    g_next = (g_next + 1) % g_count;
    if (t_ms) {
        *t_ms = 0;
    }
    return STC_OK;
}

static void replay_close(void *ctx)
{
    (void)ctx;
}

static const stc_frame_source_t k_replay = {
    .name = "replay", .ctx = NULL, .open = replay_open,
    .capture = replay_capture, .close = replay_close
};

const stc_frame_source_t *stc_frame_source_replay(const char *dir)
{
    if (!dir) {
        return NULL;
    }
    snprintf(g_dir, sizeof g_dir, "%s", dir);
    return &k_replay;
}
