#include "stc_lid_link.h"

/* ESP 側 USB CDC 傳輸。未接真實 usb_host_cdc_acm 前一律回 UNSUPPORTED，
 * 令 host 與 CI 都能編譯，亦不會假裝鏈路可用。 */
#if defined(ESP_PLATFORM) && __has_include("usb/cdc_acm_host.h")
#include "usb/cdc_acm_host.h"

static int cdc_send(const stc_lidmsg_t *msg, void *ctx)
{
    (void)ctx;
    return cdc_acm_host_data_tx_blocking(NULL, (const uint8_t *)msg, sizeof *msg, 50) == ESP_OK ? 0 : -1;
}
#else
static int cdc_send(const stc_lidmsg_t *msg, void *ctx)
{
    (void)msg;
    (void)ctx;
    return -1;
}
#endif

int stc_lid_link_cdc_attach(void)
{
    stc_lid_transport_t tx = {.send = cdc_send, .ctx = NULL};
    stc_lid_link_init(&tx);
    return 0;
}
