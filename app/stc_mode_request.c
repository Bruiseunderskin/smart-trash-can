#include "stc_mode_request.h"

#include <stdatomic.h>

#define MODE_REQUEST_NONE (-1)

static atomic_int g_requested_mode;

void stc_mode_request_init(void)
{
    atomic_store(&g_requested_mode, MODE_REQUEST_NONE);
}

stc_err_t stc_mode_request_post(stc_mode_t mode)
{
    if (mode != STC_MODE_GAME && mode != STC_MODE_CHAT) {
        return STC_ERR_CONTRACT;
    }
    atomic_store(&g_requested_mode, (int)mode);
    return STC_OK;
}

bool stc_mode_request_take(stc_mode_t *mode_out)
{
    if (!mode_out) {
        return false;
    }
    const int request =
        atomic_exchange(&g_requested_mode, MODE_REQUEST_NONE);
    if (request != STC_MODE_GAME && request != STC_MODE_CHAT) {
        return false;
    }
    *mode_out = (stc_mode_t)request;
    return true;
}
