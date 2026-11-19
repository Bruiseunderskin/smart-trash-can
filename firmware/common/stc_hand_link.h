/* stc_hand_link.h — S31-side ESP-NOW hand command/reliability layer. */
#pragma once

#include <stdbool.h>

#include "stc_hand_msg.h"
#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STC_HAND_CMD_IDLE = 0,
    STC_HAND_CMD_WAITING,
    STC_HAND_CMD_ACKED,
    STC_HAND_CMD_TIMEOUT,
} stc_hand_cmd_state_t;

typedef struct {
    int (*send)(const stc_hand_msg_t *msg, void *ctx);
    void *ctx;
} stc_hand_transport_t;

typedef struct {
    /* The transport carries the low eight bits of the monotonic game round ID.
     * This hook returns the full ID captured when the command was sent. */
    void (*on_ack)(stc_hand_action_t action, bool success, uint32_t round_id,
                   void *ctx);
    /* Return true only after the game core accepted this DROP. A false result
     * leaves the reliability slot open so S3's next retry can be delivered. */
    bool (*on_drop)(const stc_hand_msg_t *msg, uint32_t round_id, void *ctx);
    void (*on_online)(bool online, void *ctx);
} stc_hand_hooks_t;

void stc_hand_link_init(const stc_hand_transport_t *transport,
                        const stc_hand_hooks_t *hooks, void *user_ctx);
stc_err_t stc_hand_link_command(stc_hand_action_t action, uint32_t round_id,
                                uint32_t now_ms);
void stc_hand_link_on_frame(const stc_hand_msg_t *msg, uint32_t now_ms);
stc_err_t stc_hand_link_poll(uint32_t now_ms);
bool stc_hand_link_online(uint32_t now_ms);
stc_hand_cmd_state_t stc_hand_link_command_state(void);
uint8_t stc_hand_link_last_seq(void);
bool stc_hand_link_last_drop(stc_hand_msg_t *out);

#ifdef __cplusplus
}
#endif
