/* stc_lid_link.h — 鏈路協定：重試、離線判定（#53 §6.4）
 * 傳輸由呼叫者注入，所以 host 可以完整測試，不需要 USB。 */
#pragma once
#include "stc_lidmsg.h"
#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STC_LID_ACK_TIMEOUT_MS 300u
#define STC_LID_MAX_RETRY 2
#define STC_LID_NODE_TIMEOUT_MS 500u

typedef struct {
    /* 回 0 = 已送出，負 = 傳輸錯誤 */
    int (*send)(const stc_lidmsg_t *msg, void *ctx);
    void *ctx;
} stc_lid_transport_t;

void stc_lid_link_init(const stc_lid_transport_t *transport);
/* 送 OPEN 命令並在 300 ms 內等同 seq 的 ACK；逾時重試 2 次後回 STC_ERR_IO */
stc_err_t stc_lid_link_open(stc_bin_t bin, uint32_t now_ms);
/* 餵入收到的幀（ACK 或 NODE_STATE），更新離線狀態 */
void stc_lid_link_on_frame(const stc_lidmsg_t *msg, uint32_t now_ms);
/* 節點 500 ms 無 NODE_STATE → 離線 */
bool stc_lid_link_node_online(uint32_t now_ms);
uint8_t stc_lid_link_last_seq(void);
/* 每次 tick 呼叫；逾時會自動重試，用盡即回 STC_ERR_IO */
stc_err_t stc_lid_link_poll(uint32_t now_ms);

#ifdef __cplusplus
}
#endif
