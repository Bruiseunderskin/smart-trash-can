/* test_mode_gate.c — 轉移表、唯一 CHAT 入口、非法輸入零副作用（#53 I3） */
#include <stdio.h>

#include "stc_game.h"
#include "stc_mode.h"

static int g_enter_game, g_enter_chat, g_exit_chat, g_enter_round, g_exit_round;
static void h_game(void) { g_enter_game++; }
static void h_chat(void) { g_enter_chat++; }
static void h_xchat(void) { g_exit_chat++; }
static void h_round(void) { g_enter_round++; }
static void h_xround(void) { g_exit_round++; }

static int fail(const char *msg)
{
    fprintf(stderr, "%s (state=%s)\n", msg, stc_mode_state_name(stc_mode_get()));
    return 1;
}

int main(void)
{
    stc_mode_hooks_t hooks = {h_game, h_chat, h_xchat, h_round, h_xround};
    if (stc_game_init() != STC_OK) return fail("game init");
    stc_mode_init();
    stc_mode_set_hooks(&hooks);

    if (stc_mode_get() != STC_ST_BOOT) return fail("not boot");

    /* BOOT：只有 IN_PLAY 合法 */
    if (stc_mode_input(STC_IN_CHAT_ENTER, 0) != STC_ERR_NOT_READY) return fail("boot accepted chat");
    if (stc_mode_get() != STC_ST_BOOT) return fail("boot changed");
    if (stc_mode_input(STC_IN_PLAY, 0) != STC_OK) return fail("boot refused play");
    if (stc_mode_get() != STC_ST_GAME_IDLE) return fail("boot->idle");

    /* GAME_ROUND：100 次 SET_LONG 必須 0 次進 CHAT */
    if (stc_mode_input(STC_IN_PLAY, 10) != STC_OK) return fail("idle refused play");
    if (stc_mode_get() != STC_ST_GAME_ROUND) return fail("idle->round");
    for (int i = 0; i < 100; ++i) {
        (void)stc_mode_input(STC_IN_SET_LONG, (uint32_t)(20 + i));
        if (stc_mode_layer() == STC_MODE_CHAT) return fail("SET_LONG entered chat in round");
        if (stc_mode_get() != STC_ST_GAME_ROUND) return fail("round changed on SET_LONG");
    }
    if (g_enter_chat != 0) return fail("chat hook fired in round");

    /* 回合中 PLAY 亦拒絕 */
    if (stc_mode_input(STC_IN_PLAY, 200) != STC_ERR_NOT_READY) return fail("round accepted play");

    /* SUMMARY：SET_LONG 拒絕，PLAY 回 IDLE */
    stc_snapshot_t snap = stc_game_snapshot();
    (void)snap;
    if (stc_mode_input(STC_IN_ROUND_END, 300) != STC_OK) return fail("round end refused");
    if (stc_mode_get() != STC_ST_GAME_IDLE) return fail("first round end should return to idle");

    /* CHAT 唯一入口：GAME_IDLE + IN_CHAT_ENTER */
    if (stc_mode_input(STC_IN_CHAT_ENTER, 400) != STC_OK) return fail("idle refused chat");
    if (stc_mode_get() != STC_ST_CHAT) return fail("idle->chat");
    if (!stc_mode_allows_chat() && stc_mode_layer() != STC_MODE_CHAT) return fail("chat layer wrong");

    /* CHAT 60 s 閒置自動回 GAME */
    stc_mode_tick(400 + 59000);
    if (stc_mode_get() != STC_ST_CHAT) return fail("left chat too early");
    stc_mode_tick(400 + 60000);
    if (stc_mode_get() != STC_ST_GAME_IDLE) return fail("chat idle timeout");
    if (g_exit_chat != 1) return fail("exit_chat hook count");

    /* 雲端活動計數（GAME 模式） */
    stc_mode_note_cloud_activity();
    if (stc_mode_cloud_violations() != 1) return fail("cloud counter");

    /* FAULT / RESET */
    if (stc_mode_input(STC_IN_FAULT, 70000) != STC_OK) return fail("fault refused");
    if (stc_mode_get() != STC_ST_FAULT) return fail("not fault");
    if (stc_mode_input(STC_IN_PLAY, 70001) != STC_ERR_NOT_READY) return fail("fault accepted play");
    if (stc_mode_input(STC_IN_RESET, 70002) != STC_OK) return fail("reset refused");
    if (stc_mode_get() != STC_ST_BOOT) return fail("reset->boot");

    printf("test_mode_gate: transitions ok, 100x SET_LONG blocked\n");
    return 0;
}
