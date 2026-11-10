#include "stc_lid.h"

#include <string.h>

static stc_bin_t g_last_open;
static unsigned g_open_count;
static stc_drop_t g_pending;
static bool g_has_drop;

static stc_err_t lb_open(stc_bin_t bin)
{
    if ((unsigned)bin >= (unsigned)STC_BIN_COUNT) {
        return STC_ERR_CONTRACT;
    }
    g_last_open = bin;
    g_open_count++;
    return STC_OK;
}

static stc_err_t lb_close(void) { return STC_OK; }

static int lb_poll(stc_drop_t *ev)
{
    if (!g_has_drop) {
        return 0;
    }
    if (ev) {
        *ev = g_pending;
    }
    g_has_drop = false;
    return 1;
}

static void lb_tick(uint32_t now_ms)
{
    (void)now_ms;
    g_pending.t_ms += 0;
}

static uint16_t lb_distance(stc_bin_t bin)
{
    (void)bin;
    return 0;
}

static const stc_lid_ops_t k_loopback = {
    .open_bin = lb_open, .close_all = lb_close, .poll = lb_poll,
    .tick = lb_tick, .distance_mm = lb_distance, .name = "loopback"
};

const stc_lid_ops_t *stc_lid_loopback(void) { return &k_loopback; }

void stc_lid_loopback_inject_drop(stc_bin_t bin, uint16_t dist_mm)
{
    g_pending.bin = bin;
    g_pending.dist_mm = dist_mm;
    g_pending.t_ms = 0;
    g_has_drop = true;
}

unsigned stc_lid_loopback_open_count(void) { return g_open_count; }
stc_bin_t stc_lid_loopback_last_bin(void) { return g_last_open; }

/* stub：永遠 UNSUPPORTED */
static stc_err_t st_open(stc_bin_t b) { (void)b; return STC_ERR_UNSUPPORTED; }
static stc_err_t st_close(void) { return STC_ERR_UNSUPPORTED; }
static int st_poll(stc_drop_t *e) { (void)e; return (int)STC_ERR_UNSUPPORTED; }
static void st_tick(uint32_t n) { (void)n; }
static uint16_t st_dist(stc_bin_t b) { (void)b; return 0; }

static const stc_lid_ops_t k_stub = {
    .open_bin = st_open, .close_all = st_close, .poll = st_poll,
    .tick = st_tick, .distance_mm = st_dist, .name = "stub"
};

const stc_lid_ops_t *stc_lid_stub(void) { return &k_stub; }
