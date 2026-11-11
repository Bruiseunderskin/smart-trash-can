#include "stc_frame_source.h"

/* I6 才接真驅動；在此之前一律回 UNSUPPORTED，唔可以假裝有幀。 */
static stc_err_t esp_open(void *ctx, uint32_t timeout_ms)
{
    (void)ctx;
    (void)timeout_ms;
    return STC_ERR_UNSUPPORTED;
}

static stc_err_t esp_capture(void *ctx, uint8_t *out, size_t capacity, uint32_t *t_ms)
{
    (void)ctx;
    (void)out;
    (void)capacity;
    (void)t_ms;
    return STC_ERR_UNSUPPORTED;
}

static void esp_close(void *ctx) { (void)ctx; }

static const stc_frame_source_t k_esp = {
    .name = "esp", .ctx = NULL, .open = esp_open,
    .capture = esp_capture, .close = esp_close
};

const stc_frame_source_t *stc_frame_source_esp(void)
{
    return &k_esp;
}
