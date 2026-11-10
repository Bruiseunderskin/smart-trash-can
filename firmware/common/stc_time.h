/* stc_time.h — 可注入時鐘（host 測試用 mock，板上用 esp_timer） */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (*stc_now_fn)(void);
void stc_time_set_source(stc_now_fn fn);

#ifdef __cplusplus
}
#endif
