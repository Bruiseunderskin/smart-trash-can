#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_console.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "servo_lids.h"
#include "stc_hand_msg.h"
#include "stc_hand_round.h"
#include "stc_tof_drop.h"
#include "tof4.h"

static const char *TAG = "GAME_HAND";

#define OPEN_SETTLE_MS 800u
#define CLOSE_SETTLE_MS 700u
#define TOF_SAMPLE_TIMEOUT_MS 100u
#define HEARTBEAT_PERIOD_MS 250u
#define DROP_RETRY_MS 300u
#define DROP_MAX_SENDS 3u
#define S31_WAIT_DROP_READY_MS 1800u

typedef enum {
    COMMAND_START = 0,
    COMMAND_CLOSE,
    COMMAND_STOP,
    COMMAND_STATUS,
    COMMAND_SIM_DROP,
} command_type_t;

typedef struct {
    uint8_t src[ESP_NOW_ETH_ALEN];
    stc_hand_msg_t msg;
} rx_item_t;

static const uint8_t k_broadcast[ESP_NOW_ETH_ALEN] =
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static QueueHandle_t s_command_queue;
static QueueHandle_t s_rx_queue;
static stc_hand_round_t s_round;
static stc_tof_drop_detector_t s_detector;
static stc_tof_baseline_t s_baseline[STC_TOF4_SENSOR_COUNT];
static uint8_t s_active_round;
static uint8_t s_next_round;
static uint32_t s_open_command_at;
static uint32_t s_open_settle_due;
static uint32_t s_close_settle_due;
static uint32_t s_last_wait_log_ms;
static bool s_open_applied;
static bool s_close_due_set;
static bool s_result_logged;
static uint8_t s_last_missing;
static uint8_t s_controller_mac[ESP_NOW_ETH_ALEN];
static bool s_controller_known;
static uint8_t s_heartbeat_seq;
static uint8_t s_command_seq;
static bool s_transport_round;
static bool s_have_last_command;
static uint8_t s_last_seq;
static uint8_t s_last_round;
static uint8_t s_last_action;
static uint8_t s_drop_sends;
static uint32_t s_drop_due_ms;
static uint16_t s_drop_dist_mm;
static bool s_drop_pending;
static stc_hand_msg_t s_drop_msg;
static uint8_t s_command_bin;
static bool s_paused;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void receive_callback(const esp_now_recv_info_t *info,
                             const uint8_t *data, int len)
{
    if (!info || !info->src_addr || !data ||
        len != (int)sizeof(stc_hand_msg_t)) {
        return;
    }
    stc_hand_msg_t msg;
    memcpy(&msg, data, sizeof msg);
    if (!stc_hand_msg_check(&msg)) {
        return;
    }
    rx_item_t item = {.msg = msg};
    memcpy(item.src, info->src_addr, sizeof item.src);
    (void)xQueueSend(s_rx_queue, &item, pdMS_TO_TICKS(10));
}

static void send_callback(const esp_now_send_info_t *info,
                          esp_now_send_status_t status)
{
    if (info && info->des_addr && status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "ESP_NOW_SEND_FAIL dest=" MACSTR,
                 MAC2STR(info->des_addr));
    }
}

static esp_err_t network_init(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG,
                        "event loop init failed");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG,
                        "wifi storage failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG,
                        "wifi mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG,
                        "wifi power save failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE),
                        TAG, "wifi channel failed");
    ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "esp-now init failed");
    ESP_RETURN_ON_ERROR(esp_now_register_recv_cb(receive_callback), TAG,
                        "esp-now receive callback failed");
    ESP_RETURN_ON_ERROR(esp_now_register_send_cb(send_callback), TAG,
                        "esp-now send callback failed");

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, k_broadcast, sizeof peer.peer_addr);
    peer.channel = 6;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    return esp_now_add_peer(&peer);
}

static void remember_controller(const uint8_t mac[ESP_NOW_ETH_ALEN])
{
    if (!s_controller_known ||
        memcmp(s_controller_mac, mac, ESP_NOW_ETH_ALEN) != 0) {
        memcpy(s_controller_mac, mac, ESP_NOW_ETH_ALEN);
        s_controller_known = true;
        if (!esp_now_is_peer_exist(mac)) {
            esp_now_peer_info_t peer = {0};
            memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
            peer.channel = 6;
            peer.ifidx = WIFI_IF_STA;
            peer.encrypt = false;
            ESP_ERROR_CHECK(esp_now_add_peer(&peer));
        }
        ESP_LOGI(TAG, "CONTROLLER_FOUND mac=" MACSTR, MAC2STR(mac));
    }
}

static esp_err_t send_frame(const uint8_t *dest, const stc_hand_msg_t *msg)
{
    return esp_now_send(dest, (const uint8_t *)msg, sizeof *msg);
}

static stc_hand_msg_t base_message(stc_hand_msg_type_t type)
{
    stc_hand_msg_t msg = {0};
    msg.sof = STC_HAND_MSG_SOF;
    msg.ver = STC_HAND_MSG_VERSION;
    msg.type = type;
    msg.bin = 0xFF;
    msg.flags = STC_HAND_FLAG_READY;
    return msg;
}

static void send_ack(const stc_hand_msg_t *command)
{
    if (!s_controller_known) {
        return;
    }
    stc_hand_msg_t ack = base_message(STC_HAND_MSG_ACK);
    ack.seq = command->seq;
    ack.round = command->round;
    ack.action = command->action;
    if (command->action == STC_HAND_ACTION_OPEN_ALL ||
        command->action == STC_HAND_ACTION_OPEN_ALL_HOLD) {
        ack.flags |= STC_HAND_FLAG_OPEN;
    }
    stc_hand_msg_seal(&ack);
    const esp_err_t err = send_frame(s_controller_mac, &ack);
    ESP_LOGI(TAG,
             "TX_ACK round=%u seq=%u action=%u result=%s",
             ack.round, ack.seq, ack.action, esp_err_to_name(err));
}

static void send_heartbeat(void)
{
    stc_hand_msg_t state = base_message(STC_HAND_MSG_STATE);
    state.seq = s_heartbeat_seq++;
    const stc_hand_round_phase_t phase = stc_hand_round_phase(&s_round);
    if (phase == STC_HAND_ROUND_OPENING ||
        phase == STC_HAND_ROUND_DETECTING ||
        phase == STC_HAND_ROUND_WAIT_CLOSE ||
        phase == STC_HAND_ROUND_HOLDING) {
        state.flags |= STC_HAND_FLAG_OPEN;
    }
    stc_hand_msg_seal(&state);
    (void)send_frame(k_broadcast, &state);
}

static const char *phase_name(stc_hand_round_phase_t phase)
{
    switch (phase) {
    case STC_HAND_ROUND_IDLE:
        return "idle";
    case STC_HAND_ROUND_OPENING:
        return "opening";
    case STC_HAND_ROUND_DETECTING:
        return "detecting";
    case STC_HAND_ROUND_WAIT_CLOSE:
        return "wait_close";
    case STC_HAND_ROUND_RESULT:
        return "result";
    case STC_HAND_ROUND_HOLDING:
        return "holding";
    default:
        return "unknown";
    }
}

static void reset_round_runtime(void)
{
    s_open_command_at = 0;
    s_open_applied = false;
    s_close_due_set = false;
    s_result_logged = false;
    s_open_settle_due = 0;
    s_close_settle_due = 0;
    s_last_wait_log_ms = 0;
    s_last_missing = 0;
    s_drop_pending = false;
    s_drop_sends = 0;
    s_drop_due_ms = 0;
    s_drop_dist_mm = 0;
}

static void print_status(void)
{
    const uint32_t now = now_ms();
    int32_t remaining_ms = 0;
    const stc_hand_round_phase_t phase = stc_hand_round_phase(&s_round);
    if ((phase == STC_HAND_ROUND_DETECTING ||
         phase == STC_HAND_ROUND_WAIT_CLOSE) &&
        (int32_t)(s_round.deadline_ms - now) > 0) {
        remaining_ms = (int32_t)(s_round.deadline_ms - now);
    }
    printf("GAME_HAND phase=%s round=%u outcome=%d bin=%d remaining_ms=%ld\n",
           phase_name(phase), s_active_round, (int)s_round.outcome,
           s_round.selected_bin, (long)remaining_ms);
    if (s_paused) {
        printf("GAME_HAND paused=1 heartbeat=off tof=stopped servo_pwm=off\n");
    }
}

static void fail_round(const char *reason)
{
    ESP_LOGE(TAG, "ROUND_FAIL reason=%s round=%u", reason, s_active_round);
    const esp_err_t err = stc_servo_lids_close_all();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "fault close failed: %s", esp_err_to_name(err));
    }
    stc_hand_round_init(&s_round);
    reset_round_runtime();
}

static bool start_round_with(uint8_t round_id, uint8_t command_seq,
                             bool transport_round)
{
    if (stc_hand_round_phase(&s_round) != STC_HAND_ROUND_IDLE &&
        stc_hand_round_phase(&s_round) != STC_HAND_ROUND_RESULT) {
        ESP_LOGW(TAG, "START_REJECTED phase=%s",
                 phase_name(stc_hand_round_phase(&s_round)));
        return false;
    }

    const uint32_t started_at = now_ms();
    s_active_round = round_id;
    s_command_seq = command_seq;
    s_transport_round = transport_round;
    reset_round_runtime();
    s_open_command_at = started_at;
    const esp_err_t err =
        stc_hand_round_start(&s_round, s_active_round, started_at);
    if (err != STC_OK) {
        ESP_LOGE(TAG, "ROUND_START_FAIL error=%d", (int)err);
        return false;
    }
    ESP_LOGI(TAG, "ROUND_START round=%u timeout_ms=%u close_delay_ms=%u",
             s_active_round, STC_HAND_ROUND_DETECT_TIMEOUT_MS,
             STC_HAND_ROUND_DROP_CLOSE_DELAY_MS);
    return true;
}

static void start_local_round(void)
{
    (void)start_round_with(s_next_round++, 0, false);
}

static void manual_close(void)
{
    ESP_LOGW(TAG, "MANUAL_CLOSE");
    (void)stc_servo_lids_close_all();
    stc_hand_round_init(&s_round);
    reset_round_runtime();
}

static void manual_stop(void)
{
    ESP_LOGW(TAG, "MANUAL_STOP");
    (void)stc_servo_lids_stop_all();
    stc_hand_round_init(&s_round);
    reset_round_runtime();
}

static void send_drop_attempt(uint32_t now)
{
    if (!s_drop_pending || !s_controller_known ||
        s_drop_sends >= DROP_MAX_SENDS ||
        (int32_t)(now - s_drop_due_ms) < 0) {
        return;
    }
    const esp_err_t err = send_frame(s_controller_mac, &s_drop_msg);
    s_drop_sends++;
    s_drop_due_ms = now + DROP_RETRY_MS;
    ESP_LOGI(TAG,
             "TX_DROP round=%u seq=%u bin=%u dist_mm=%u attempt=%u/%u "
             "result=%s",
             s_drop_msg.round, s_drop_msg.seq, s_drop_msg.bin,
             s_drop_msg.dist_mm, s_drop_sends, DROP_MAX_SENDS,
             esp_err_to_name(err));
    if (s_drop_sends >= DROP_MAX_SENDS) {
        s_drop_pending = false;
    }
}

static void queue_drop_result(uint32_t now)
{
    if (!s_transport_round || !s_controller_known) {
        return;
    }
    s_drop_msg = base_message(STC_HAND_MSG_DROP);
    s_drop_msg.seq = s_command_seq;
    s_drop_msg.round = s_active_round;
    s_drop_msg.bin = stc_hand_round_selected_bin(&s_round);
    s_drop_msg.dist_mm = s_drop_dist_mm;
    s_drop_msg.flags |= STC_HAND_FLAG_DROP_VALID;
    stc_hand_msg_seal(&s_drop_msg);
    s_drop_pending = true;
    s_drop_sends = 0;
    const uint32_t s31_ready_at = s_open_command_at + S31_WAIT_DROP_READY_MS;
    s_drop_due_ms =
        (int32_t)(now - s31_ready_at) >= 0 ? now : s31_ready_at;
    send_drop_attempt(now);
}

static void process_hand_frame(const rx_item_t *item)
{
    const stc_hand_msg_t *msg = &item->msg;
    if (msg->type == STC_HAND_MSG_ACK) {
        if (s_drop_pending && msg->seq == s_command_seq &&
            msg->round == s_active_round &&
            msg->bin == stc_hand_round_selected_bin(&s_round)) {
            s_drop_pending = false;
            ESP_LOGI(TAG, "DROP_ACK round=%u seq=%u",
                     msg->round, msg->seq);
        }
        return;
    }
    if (msg->type != STC_HAND_MSG_CMD ||
        (msg->action != STC_HAND_ACTION_OPEN_ALL &&
         msg->action != STC_HAND_ACTION_CLOSE_ALL &&
         msg->action != STC_HAND_ACTION_OPEN_ALL_HOLD &&
         msg->action != STC_HAND_ACTION_PAUSE &&
         msg->action != STC_HAND_ACTION_RESUME)) {
        return;
    }

    remember_controller(item->src);
    const bool duplicate =
        s_have_last_command && s_last_seq == msg->seq &&
        s_last_round == msg->round && s_last_action == msg->action;
    if (duplicate) {
        send_ack(msg);
        ESP_LOGI(TAG, "RX_CMD_DUPLICATE round=%u seq=%u action=%u",
                 msg->round, msg->seq, msg->action);
        return;
    }

    if (msg->action == STC_HAND_ACTION_PAUSE) {
        send_ack(msg);
        (void)manual_close();
        vTaskDelay(pdMS_TO_TICKS(CLOSE_SETTLE_MS));
        (void)stc_tof4_stop_continuous();
        (void)stc_servo_lids_stop_all();
        s_paused = true;
        ESP_LOGI(TAG,
                 "RX_PAUSE round=%u seq=%u heartbeat=off tof=stopped",
                 msg->round, msg->seq);
    } else if (msg->action == STC_HAND_ACTION_RESUME) {
        s_paused = false;
        send_ack(msg);
        send_heartbeat();
        ESP_LOGI(TAG, "RX_RESUME round=%u seq=%u heartbeat=on",
                 msg->round, msg->seq);
    } else if (msg->action == STC_HAND_ACTION_OPEN_ALL_HOLD) {
        s_active_round = msg->round;
        s_command_seq = msg->seq;
        s_transport_round = true;
        reset_round_runtime();
        if (stc_hand_round_hold_open(&s_round, msg->round, now_ms()) !=
            STC_OK) {
            ESP_LOGW(TAG, "RX_HOLD_REJECTED round=%u seq=%u",
                     msg->round, msg->seq);
            return;
        }
        send_ack(msg);
        ESP_LOGI(TAG, "RX_HOLD_OPEN round=%u seq=%u", msg->round, msg->seq);
    } else if (msg->action == STC_HAND_ACTION_OPEN_ALL) {
        if (!start_round_with(msg->round, msg->seq, true)) {
            ESP_LOGW(TAG, "RX_OPEN_REJECTED round=%u seq=%u",
                     msg->round, msg->seq);
            return;
        }
        send_ack(msg);
        ESP_LOGI(TAG, "RX_OPEN round=%u seq=%u", msg->round, msg->seq);
    } else {
        send_ack(msg);
        const stc_hand_round_phase_t phase =
            stc_hand_round_phase(&s_round);
        if (phase == STC_HAND_ROUND_WAIT_CLOSE &&
            stc_hand_round_outcome(&s_round) ==
                STC_HAND_ROUND_OUTCOME_DROP) {
            ESP_LOGI(TAG,
                     "RX_CLOSE_DEFERRED round=%u remaining_ms=%ld",
                     s_active_round,
                     (long)(s_round.deadline_ms - now_ms()));
        } else if (phase == STC_HAND_ROUND_IDLE) {
            (void)stc_servo_lids_close_all();
        } else if (phase == STC_HAND_ROUND_OPENING ||
                   phase == STC_HAND_ROUND_DETECTING ||
                   phase == STC_HAND_ROUND_HOLDING) {
            manual_close();
        }
        ESP_LOGI(TAG, "RX_CLOSE round=%u seq=%u", msg->round, msg->seq);
    }

    s_have_last_command = true;
    s_last_seq = msg->seq;
    s_last_round = msg->round;
    s_last_action = msg->action;
}

static void handle_command(command_type_t command)
{
    switch (command) {
    case COMMAND_START:
        s_paused = false;
        start_local_round();
        break;
    case COMMAND_CLOSE:
        manual_close();
        break;
    case COMMAND_STOP:
        manual_stop();
        break;
    case COMMAND_STATUS:
        print_status();
        break;
    case COMMAND_SIM_DROP:
        if (!s_transport_round ||
            !stc_hand_round_drop_accepted(
                &s_round, s_active_round, s_command_bin, now_ms())) {
            ESP_LOGW(TAG, "SIM_DROP_REJECT round=%u bin=%u phase=%s",
                     s_active_round, s_command_bin,
                     phase_name(stc_hand_round_phase(&s_round)));
            break;
        }
        s_drop_dist_mm = 40;
        queue_drop_result(now_ms());
        ESP_LOGI(TAG, "SIM_DROP_ACCEPT round=%u bin=%u",
                 s_active_round, s_command_bin);
        break;
    }
}

static void apply_round_actions(void)
{
    const stc_hand_round_action_t action =
        stc_hand_round_take_action(&s_round);
    if (action == STC_HAND_ROUND_ACTION_OPEN_ALL) {
        const esp_err_t err = stc_servo_lids_open_all();
        if (err != ESP_OK) {
            fail_round("SERVO_OPEN");
            return;
        }
        s_open_applied = true;
        s_open_settle_due = now_ms() + OPEN_SETTLE_MS;
        ESP_LOGI(TAG, "ALL_LIDS_OPEN round=%u settle_ms=%u",
                 s_active_round, OPEN_SETTLE_MS);
    } else if (action == STC_HAND_ROUND_ACTION_CLOSE_ALL) {
        const stc_hand_round_outcome_t outcome =
            stc_hand_round_outcome(&s_round);
        const esp_err_t err = stc_servo_lids_close_all();
        if (err != ESP_OK) {
            fail_round("SERVO_CLOSE");
            return;
        }
        s_close_due_set = true;
        s_close_settle_due = now_ms() + CLOSE_SETTLE_MS;
        ESP_LOGI(TAG, "ALL_LIDS_CLOSE round=%u reason=%s",
                 s_active_round,
                 outcome == STC_HAND_ROUND_OUTCOME_TIMEOUT
                     ? "NO_ANSWER_TIMEOUT"
                     : "DROP_WINDOW_ELAPSED");
    }
}

static void arm_detection(uint32_t now)
{
    ESP_LOGI(TAG, "BASELINE_START round=%u", s_active_round);
    esp_err_t err = stc_tof4_start_continuous();
    if (err == ESP_OK) {
        err = stc_tof4_capture_baseline(
            s_baseline, TOF_SAMPLE_TIMEOUT_MS);
    }
    if (err != ESP_OK) {
        fail_round("TOF_BASELINE");
        return;
    }
    if (stc_tof_drop_init(&s_detector, s_baseline) != STC_OK) {
        fail_round("TOF_DETECTOR_INIT");
        return;
    }

    const uint32_t armed_at = now_ms();
    if (stc_hand_round_arm(&s_round, armed_at) != STC_OK) {
        fail_round("ROUND_ARM");
        return;
    }
    s_last_wait_log_ms = armed_at;
    ESP_LOGI(TAG,
             "DETECTING round=%u armed_at_ms=%" PRIu32
             " timeout_ms=%u sample_period_ms=10 confirm_samples=%u",
             s_active_round, armed_at, STC_HAND_ROUND_DETECT_TIMEOUT_MS,
             STC_TOF_CONFIRM_SAMPLES);
}

static void process_opening(const uint32_t now)
{
    if (stc_hand_round_phase(&s_round) != STC_HAND_ROUND_OPENING ||
        !s_open_applied || (int32_t)(now - s_open_settle_due) < 0) {
        return;
    }
    arm_detection(now);
}

static void process_detection(const uint32_t now)
{
    if (stc_hand_round_phase(&s_round) != STC_HAND_ROUND_DETECTING) {
        return;
    }

    stc_tof_sample_t samples[STC_TOF4_SENSOR_COUNT] = {0};
    uint8_t missing = 0;
    const esp_err_t read_err =
        stc_tof4_read_round(samples, TOF_SAMPLE_TIMEOUT_MS, &missing);
    if (missing != s_last_missing) {
        ESP_LOGW(TAG, "TOF_MISS round=%u gpio_mask=0x%02x last=0x%02x",
                 s_active_round, missing, s_last_missing);
        s_last_missing = missing;
    }
    if (read_err != ESP_OK) {
        return;
    }

    stc_tof_drop_event_t event;
    const bool detected =
        stc_tof_drop_update(&s_detector, samples, &event);
    if (!detected && s_detector.candidate < STC_TOF4_SENSOR_COUNT &&
        s_detector.streak == 1) {
        ESP_LOGW(TAG,
                 "TOF_CANDIDATE round=%u sensor=%u gpio=%d "
                 "range_mm=%u baseline_mm=%u return_rate=%u baseline_return=%u",
                 s_active_round, s_detector.candidate,
                 stc_tof4_gpio(s_detector.candidate),
                 samples[s_detector.candidate].range_mm,
                 s_baseline[s_detector.candidate].range_mm,
                 samples[s_detector.candidate].return_rate,
                 s_baseline[s_detector.candidate].return_rate);
    }
    if (!detected) {
        return;
    }

    const uint32_t detected_at = now_ms();
    if (!stc_hand_round_drop_accepted(
            &s_round, s_active_round, event.sensor_index, detected_at)) {
        ESP_LOGW(TAG, "DROP_REJECTED round=%u sensor=%u",
                 s_active_round, event.sensor_index);
        return;
    }
    s_drop_dist_mm = event.range_mm;
    /* Report the confirmed bin immediately. The three-second lid hold still
     * runs locally, but waiting for it here races S31's WAIT_DROP timeout. */
    queue_drop_result(detected_at);

    ESP_LOGI(TAG,
             "ANSWER_DETECTED round=%u bin=%u sensor=%u gpio=%d "
             "range_mm=%u baseline_mm=%u return_rate=%u "
             "baseline_return=%u close_delay_ms=%u",
             s_active_round, event.sensor_index, event.sensor_index,
             stc_tof4_gpio(event.sensor_index), event.range_mm,
             event.baseline_range_mm, event.return_rate,
             event.baseline_return_rate,
             STC_HAND_ROUND_DROP_CLOSE_DELAY_MS);
}

static void process_close_settle(const uint32_t now)
{
    if (!s_close_due_set ||
        (int32_t)(now - s_close_settle_due) < 0) {
        return;
    }
    s_close_due_set = false;
    if (stc_hand_round_close_complete(&s_round, now) != STC_OK) {
        return;
    }
    print_status();
}

static void log_result_once(void)
{
    if (s_result_logged ||
        stc_hand_round_phase(&s_round) != STC_HAND_ROUND_RESULT) {
        return;
    }
    s_result_logged = true;

    const stc_hand_round_outcome_t outcome =
        stc_hand_round_outcome(&s_round);
    if (outcome == STC_HAND_ROUND_OUTCOME_DROP) {
        ESP_LOGI(TAG, "ROUND_RESULT round=%u outcome=ANSWER bin=%u",
                 s_active_round, stc_hand_round_selected_bin(&s_round));
    } else if (outcome == STC_HAND_ROUND_OUTCOME_TIMEOUT) {
        ESP_LOGW(TAG, "ROUND_RESULT round=%u outcome=NO_ANSWER",
                 s_active_round);
    }
}

static void log_detection_wait(const uint32_t now)
{
    const stc_hand_round_phase_t phase = stc_hand_round_phase(&s_round);
    if (phase != STC_HAND_ROUND_DETECTING &&
        phase != STC_HAND_ROUND_WAIT_CLOSE) {
        return;
    }
    if ((int32_t)(now - s_last_wait_log_ms) < 1000) {
        return;
    }
    s_last_wait_log_ms = now;
    const int32_t remaining_ms =
        (int32_t)(s_round.deadline_ms - now);
    ESP_LOGI(TAG,
             "ROUND_TIMER round=%u phase=%s remaining_ms=%ld outcome=%d bin=%d",
             s_active_round, phase_name(phase), (long)remaining_ms,
             (int)s_round.outcome, s_round.selected_bin);
}

static int post_command(command_type_t command)
{
    return xQueueSend(s_command_queue, &command, pdMS_TO_TICKS(20)) == pdTRUE
               ? 0
               : 1;
}

static int command_start(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return post_command(COMMAND_START);
}

static int command_close(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return post_command(COMMAND_CLOSE);
}

static int command_stop(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return post_command(COMMAND_STOP);
}

static int command_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return post_command(COMMAND_STATUS);
}

static int command_simdrop(int argc, char **argv)
{
    if (argc != 2 || argv[1][0] < '0' || argv[1][0] > '3' ||
        argv[1][1] != '\0') {
        printf("usage: simdrop 0..3\n");
        return 1;
    }
    s_command_bin = (uint8_t)(argv[1][0] - '0');
    return post_command(COMMAND_SIM_DROP);
}

static void register_console_commands(void)
{
    const esp_console_cmd_t commands[] = {
        {.command = "start", .help = "Open all lids and start a round",
         .func = command_start},
        {.command = "close", .help = "Close all lids and cancel the round",
         .func = command_close},
        {.command = "stop", .help = "Stop PWM and reset the round",
         .func = command_stop},
        {.command = "status", .help = "Print round status",
         .func = command_status},
        {.command = "simdrop", .help = "Inject one test DROP bin 0..3",
         .func = command_simdrop},
    };
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&commands[i]));
    }
    ESP_ERROR_CHECK(esp_console_register_help_command());
}

void app_main(void)
{
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs);

    ESP_ERROR_CHECK(stc_tof4_init());
    stc_hand_round_init(&s_round);
    reset_round_runtime();
    s_paused = false;
    s_controller_known = false;
    s_heartbeat_seq = 0;
    s_have_last_command = false;

    s_command_queue = xQueueCreate(8, sizeof(command_type_t));
    s_rx_queue = xQueueCreate(16, sizeof(rx_item_t));
    if (!s_command_queue || !s_rx_queue) {
        ESP_LOGE(TAG, "command queue init failed");
        return;
    }
    ESP_ERROR_CHECK(network_init());

    register_console_commands();

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "hand>";
    repl_config.max_cmdline_length = 96;
    esp_console_dev_uart_config_t uart_config =
        ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(
        esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    ESP_LOGI(TAG, "GAME_HAND=READY; type start to begin one round");
    uint32_t next_heartbeat = 0;
    while (true) {
        command_type_t command;
        if (xQueueReceive(s_command_queue, &command, 0) == pdTRUE) {
            handle_command(command);
        }
        rx_item_t item;
        while (xQueueReceive(s_rx_queue, &item, 0) == pdTRUE) {
            process_hand_frame(&item);
        }

        if (s_paused) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        apply_round_actions();
        const uint32_t now = now_ms();
        process_opening(now);
        process_detection(now);
        stc_hand_round_poll(&s_round, now);
        apply_round_actions();
        process_close_settle(now_ms());
        log_detection_wait(now);
        log_result_once();
        send_drop_attempt(now_ms());
        if ((int32_t)(now - next_heartbeat) >= 0) {
            send_heartbeat();
            next_heartbeat = now + HEARTBEAT_PERIOD_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
