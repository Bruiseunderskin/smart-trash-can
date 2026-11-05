#include <stdio.h>

#include "stc_game.h"
#include "stc_game_internal.h"

static int failures;

#define CHECK(expression)                                                      \
  do {                                                                         \
    if (!(expression)) {                                                       \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__,         \
              #expression);                                                    \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

static void send_label(stc_bin_t bin) {
  stc_event_t event = {.type = STC_EV_LABEL};
  event.u.label.bin = bin;
  event.u.label.class_idx = (uint8_t)bin;
  CHECK(stc_game_input(&event) == STC_OK);
}

static void send_drop(stc_bin_t bin) {
  stc_event_t event = {.type = STC_EV_DROP};
  event.u.drop.bin = bin;
  CHECK(stc_game_input(&event) == STC_OK);
}

static void finish_round(uint32_t now_ms) {
  stc_game_tick(now_ms + STC_GAME_RESULT_MS);
}

int main(void) {
  CHECK(stc_game_init() == STC_OK);
  CHECK(stc_game_phase() == STC_GAME_IDLE);
  stc_snapshot_t snapshot = stc_game_snapshot();
  CHECK(snapshot.round == 0);
  CHECK(snapshot.score == 0);
  CHECK(snapshot.rounds_total == 5);

  CHECK(stc_game_start() == STC_OK);
  CHECK(stc_game_phase() == STC_GAME_CAPTURE);
  stc_event_t play = {.type = STC_EV_PLAY};
  CHECK(stc_game_input(&play) == STC_ERR_NOT_READY);
  CHECK(stc_game_phase() == STC_GAME_CAPTURE);
  send_label(STC_BIN_FOOD);
  CHECK(stc_game_phase() == STC_GAME_WAIT_DROP);
  send_drop(STC_BIN_FOOD);
  CHECK(stc_game_phase() == STC_GAME_ROUND_RESULT);
  snapshot = stc_game_snapshot();
  CHECK(snapshot.score == 10);
  CHECK(stc_game_last_delta() == 10);
  CHECK(!stc_game_can_enter_chat());

  finish_round(1000);
  snapshot = stc_game_snapshot();
  CHECK(snapshot.round == 1);
  CHECK(stc_game_phase() == STC_GAME_CAPTURE);
  send_label(STC_BIN_HAZARD);
  send_drop(STC_BIN_RECYCLABLE);
  snapshot = stc_game_snapshot();
  CHECK(snapshot.score == 5);
  CHECK(stc_game_last_delta() == -5);

  finish_round(3000);
  send_label(STC_BIN_OTHER);
  stc_game_tick(ESTIMATE_TIMEOUT_MS(STC_GAME_START_MS + 1));
  CHECK(stc_game_phase() == STC_GAME_ROUND_RESULT);
  snapshot = stc_game_snapshot();
  CHECK(snapshot.score == 5);
  CHECK(stc_game_last_delta() == 0);

  finish_round(20000);
  send_label(STC_BIN_RECYCLABLE);
  send_drop(STC_BIN_RECYCLABLE);
  finish_round(23000);
  send_label(STC_BIN_FOOD);
  send_drop(STC_BIN_FOOD);
  finish_round(26000);
  snapshot = stc_game_snapshot();
  CHECK(snapshot.round == 5);
  CHECK(snapshot.score == 25);
  CHECK(stc_game_phase() == STC_GAME_SETTLEMENT);
  CHECK(stc_game_can_enter_chat());

  if (failures) {
    fprintf(stderr, "%d game checks failed\n", failures);
    return 1;
  }
  puts("Game checks passed");
  return 0;
}
