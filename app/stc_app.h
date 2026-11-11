/* stc_app.h — 把遊戲 snapshot 轉成 UI payload（#53 I4）
 * 唯一做這件事的地方：UI 只收 payload，game 只出 snapshot。 */
#pragma once
#include "stc_game.h"
#include "stc_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 回 false 表示 snapshot 與 UI 契約不兼容（例如 phase 越界）。 */
bool stc_app_payload_from_snapshot(const stc_snapshot_t *snap, stc_ui_payload_t *out);

#ifdef __cplusplus
}
#endif
