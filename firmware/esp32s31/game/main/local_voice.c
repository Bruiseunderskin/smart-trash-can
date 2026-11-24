#include "local_voice.h"

#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#include "bsp/esp32_s31_korvo_1.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

extern const uint8_t stc_open_pcm_start[] asm("_binary_stc_open_pcm_start");
extern const uint8_t stc_open_pcm_end[] asm("_binary_stc_open_pcm_end");
extern const uint8_t stc_correct_pcm_start[] asm("_binary_stc_correct_pcm_start");
extern const uint8_t stc_correct_pcm_end[] asm("_binary_stc_correct_pcm_end");
extern const uint8_t stc_wrong_pcm_start[] asm("_binary_stc_wrong_pcm_start");
extern const uint8_t stc_wrong_pcm_end[] asm("_binary_stc_wrong_pcm_end");
extern const uint8_t stc_timeout_pcm_start[] asm("_binary_stc_timeout_pcm_start");
extern const uint8_t stc_timeout_pcm_end[] asm("_binary_stc_timeout_pcm_end");
extern const uint8_t stc_retry_pcm_start[] asm("_binary_stc_retry_pcm_start");
extern const uint8_t stc_retry_pcm_end[] asm("_binary_stc_retry_pcm_end");
extern const uint8_t stc_summary_pcm_start[] asm("_binary_stc_summary_pcm_start");
extern const uint8_t stc_summary_pcm_end[] asm("_binary_stc_summary_pcm_end");

static const char *TAG = "stc_voice";
static esp_codec_dev_handle_t g_speaker;
static QueueHandle_t g_queue;
static atomic_bool g_stop;
static atomic_bool g_playing;
static stc_mode_t g_mode;

typedef struct {
    const uint8_t *data;
    size_t size;
} pcm_clip_t;

static pcm_clip_t clip_for(stc_line_t line)
{
    switch (line) {
    case STC_LINE_OPEN:
        return (pcm_clip_t){stc_open_pcm_start,
                            (size_t)(stc_open_pcm_end - stc_open_pcm_start)};
    case STC_LINE_CORRECT:
        return (pcm_clip_t){stc_correct_pcm_start,
                            (size_t)(stc_correct_pcm_end - stc_correct_pcm_start)};
    case STC_LINE_WRONG:
        return (pcm_clip_t){stc_wrong_pcm_start,
                            (size_t)(stc_wrong_pcm_end - stc_wrong_pcm_start)};
    case STC_LINE_TIMEOUT:
        return (pcm_clip_t){stc_timeout_pcm_start,
                            (size_t)(stc_timeout_pcm_end - stc_timeout_pcm_start)};
    case STC_LINE_RETRY:
        return (pcm_clip_t){stc_retry_pcm_start,
                            (size_t)(stc_retry_pcm_end - stc_retry_pcm_start)};
    case STC_LINE_SUMMARY:
        return (pcm_clip_t){stc_summary_pcm_start,
                            (size_t)(stc_summary_pcm_end - stc_summary_pcm_start)};
    default:
        return (pcm_clip_t){0};
    }
}

static void voice_task(void *arg)
{
    (void)arg;
    int16_t stereo[512 * 2];
    while (true) {
        stc_line_t line;
        if (xQueueReceive(g_queue, &line, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        const pcm_clip_t clip = clip_for(line);
        if (!clip.data || !g_speaker) {
            continue;
        }
        atomic_store(&g_stop, false);
        atomic_store(&g_playing, true);
        const int16_t *samples = (const int16_t *)clip.data;
        const size_t sample_count = clip.size / sizeof(int16_t);
        size_t offset = 0;
        while (offset < sample_count && !atomic_load(&g_stop)) {
            const size_t chunk = (sample_count - offset) > 512
                                     ? 512
                                     : sample_count - offset;
            for (size_t i = 0; i < chunk; ++i) {
                stereo[i * 2] = samples[offset + i];
                stereo[i * 2 + 1] = samples[offset + i];
            }
            (void)esp_codec_dev_write(g_speaker, stereo,
                                      chunk * 2 * sizeof(int16_t));
            offset += chunk;
        }
        atomic_store(&g_playing, false);
    }
}

stc_err_t stc_game_voice_init(void)
{
    g_queue = xQueueCreate(8, sizeof(stc_line_t));
    if (!g_queue) {
        return STC_ERR_IO;
    }
    g_speaker = bsp_audio_codec_speaker_init();
    if (!g_speaker) {
        ESP_LOGE(TAG, "speaker init failed");
        return STC_ERR_IO;
    }
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 2,
        .channel_mask = 0,
        .sample_rate = 16000,
        .mclk_multiple = 0,
    };
    esp_err_t err = esp_codec_dev_open(g_speaker, &fs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "speaker open: %s", esp_err_to_name(err));
        return STC_ERR_IO;
    }
    (void)esp_codec_dev_set_out_vol(g_speaker, 70);
    g_mode = STC_MODE_GAME;
    if (xTaskCreate(voice_task, "stc_voice", 4096, NULL, 5, NULL) != pdPASS) {
        return STC_ERR_IO;
    }
    ESP_LOGI(TAG, "local PCM voice ready (16 kHz stereo, cloud disabled)");
    return STC_OK;
}

stc_err_t stc_voice_init(void) { return stc_game_voice_init(); }

stc_err_t stc_voice_say(stc_line_t line)
{
    if (!g_queue || line < 0 || line >= STC_LINE_COUNT) {
        return STC_ERR_CONTRACT;
    }
    return xQueueSend(g_queue, &line, pdMS_TO_TICKS(20)) == pdTRUE
               ? STC_OK
               : STC_ERR_IO;
}

stc_err_t stc_voice_play_sfx(stc_sfx_t sfx)
{
    (void)sfx;
    return STC_ERR_UNSUPPORTED;
}

stc_err_t stc_voice_set_mode(stc_mode_t mode)
{
    if (mode != STC_MODE_GAME) {
        return STC_ERR_UNSUPPORTED;
    }
    g_mode = mode;
    return STC_OK;
}

stc_mode_t stc_voice_get_mode(void) { return g_mode; }

stc_err_t stc_voice_start_dialog(void)
{
    return STC_ERR_UNSUPPORTED;
}

stc_err_t stc_voice_speak_text(const char *text)
{
    (void)text;
    return STC_ERR_UNSUPPORTED;
}

stc_err_t stc_voice_ask(const char *text, char *reply_out,
                        size_t reply_len, uint32_t timeout_ms)
{
    (void)text;
    (void)reply_out;
    (void)reply_len;
    (void)timeout_ms;
    return STC_ERR_UNSUPPORTED;
}

void stc_voice_poll(void) {}
bool stc_voice_is_busy(void)
{
    return atomic_load(&g_playing) || (g_queue && uxQueueMessagesWaiting(g_queue));
}

void stc_voice_stop(void)
{
    atomic_store(&g_stop, true);
    if (g_queue) {
        xQueueReset(g_queue);
    }
}
