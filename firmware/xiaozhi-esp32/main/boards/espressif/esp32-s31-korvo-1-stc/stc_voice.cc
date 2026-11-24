#include "stc_voice.h"

#include "application.h"
#include "assets.h"
#include "audio_codec.h"
#include "board.h"
#include "stc_network_gate.h"

#include <cJSON.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr const char* TAG = "StcVoice";
constexpr size_t kQueueCapacity = 8;
constexpr size_t kChunkSamples = 640;  // 40 ms at 16 kHz.

constexpr std::array<const char*, STC_LINE_COUNT> kKeys = {
    "open", "correct", "wrong", "timeout", "retry", "summary",
    "cheer_correct_a", "cheer_correct_b", "encourage_wrong_a",
    "encourage_wrong_b", "summary_tip",
};

constexpr std::array<const char*, STC_SFX_COUNT> kSfxAssets = {
    "sfx-scan.ogg",  "sfx-decide.ogg", "sfx-correct.ogg",
    "sfx-wrong.ogg", "sfx-retry.ogg",  "sfx-round.ogg",
};

std::array<std::string, STC_LINE_COUNT> assets = {
    "stc_open.pcm",    "stc_correct.pcm", "stc_wrong.pcm",
    "stc_timeout.pcm", "stc_retry.pcm",   "stc_summary.pcm",
    "stc_cheer_correct_a.pcm", "stc_cheer_correct_b.pcm",
    "stc_encourage_wrong_a.pcm", "stc_encourage_wrong_b.pcm",
    "stc_summary_tip.pcm",
};

std::once_flag phrase_once;
std::mutex queue_mutex;

enum class VoiceItemKind {
    kLine,
    kSfx,
};

struct VoiceItem {
    VoiceItemKind kind;
    stc_line_t line;
    stc_sfx_t sfx;
    int64_t enqueued_us;
};

std::deque<VoiceItem> queue;
std::atomic<bool> playing{false};
std::atomic<bool> stop_requested{false};
std::atomic<bool> initialized{false};
std::atomic<stc_mode_t> mode{STC_MODE_GAME};
std::atomic<bool> chat_dialog_pending{false};
TaskHandle_t voice_task_handle;

void LoadPhraseAssets() {
    void* data = nullptr;
    size_t size = 0;
    if (!Assets::GetInstance().GetAssetData("phrases.json", data, size) ||
        data == nullptr) {
        ESP_LOGW(TAG, "phrases.json missing; using default PCM names");
        return;
    }
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(
        cJSON_ParseWithLength(static_cast<const char*>(data), size),
        &cJSON_Delete);
    if (!root || !cJSON_IsObject(root.get())) {
        ESP_LOGW(TAG, "phrases.json invalid; using default PCM names");
        return;
    }
    for (size_t line = 0; line < kKeys.size(); ++line) {
        const cJSON* item =
            cJSON_GetObjectItemCaseSensitive(root.get(), kKeys[line]);
        if (!item) {
            continue;
        }
        if (cJSON_IsString(item) && item->valuestring != nullptr) {
            assets[line] = item->valuestring;
            continue;
        }
        const cJSON* pcm = cJSON_GetObjectItemCaseSensitive(item, "pcm");
        if (cJSON_IsString(pcm) && pcm->valuestring != nullptr) {
            assets[line] = pcm->valuestring;
        }
    }
}

bool GetPcm(stc_line_t line, const uint8_t** data, size_t* size) {
    if (line < 0 || line >= STC_LINE_COUNT) {
        return false;
    }
    std::call_once(phrase_once, LoadPhraseAssets);
    void* asset_data = nullptr;
    if (!Assets::GetInstance().GetAssetData(assets[line], asset_data, *size) ||
        asset_data == nullptr || *size < sizeof(int16_t)) {
        return false;
    }
    *data = static_cast<const uint8_t*>(asset_data);
    return true;
}

bool GetSfx(stc_sfx_t sfx, const uint8_t** data, size_t* size) {
    if (sfx < 0 || sfx >= STC_SFX_COUNT) {
        return false;
    }
    void* asset_data = nullptr;
    if (!Assets::GetInstance().GetAssetData(kSfxAssets[sfx], asset_data,
                                            *size) ||
        asset_data == nullptr || *size < 4 ||
        std::memcmp(asset_data, "OggS", 4) != 0) {
        return false;
    }
    *data = static_cast<const uint8_t*>(asset_data);
    return true;
}

void PlayLocal(const VoiceItem& item) {
    const stc_line_t line = item.line;
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!GetPcm(line, &data, &size)) {
        ESP_LOGE(TAG, "STC_VOICE_LINE line=%d result=missing_pcm asset=%s",
                 static_cast<int>(line), assets[line].c_str());
        return;
    }

    AudioCodec* codec = Board::GetInstance().GetAudioCodec();
    if (codec == nullptr) {
        ESP_LOGE(TAG, "STC_VOICE_LINE line=%d result=no_codec",
                 static_cast<int>(line));
        return;
    }
    if (!codec->output_enabled()) {
        codec->EnableOutput(true);
    }

    const size_t total_samples = size / sizeof(int16_t);
    const int16_t* pcm = reinterpret_cast<const int16_t*>(data);
    stop_requested.store(false);
    playing.store(true);
    const int64_t started_us = esp_timer_get_time();
    const int64_t queued_us = started_us - item.enqueued_us;
    ESP_LOGI(TAG,
             "STC_VOICE_LINE_START line=%d samples=%u channels=%d "
             "queued_ms=%lld",
             static_cast<int>(line), static_cast<unsigned>(total_samples),
             codec->output_channels(),
             static_cast<long long>(queued_us / 1000));

    std::vector<int16_t> output;
    output.reserve(kChunkSamples);
    for (size_t offset = 0; offset < total_samples && !stop_requested.load();
         offset += kChunkSamples) {
        const size_t count = std::min(kChunkSamples, total_samples - offset);
        /* Keep this path mono. Es8389AudioCodec::Write() owns the conversion
         * to the configured output channel count; duplicating here would make
         * it convert the samples twice and distort every local phrase. */
        output.assign(pcm + offset, pcm + offset + count);
        codec->OutputData(output);
        vTaskDelay(pdMS_TO_TICKS(40));
    }
    playing.store(false);
    ESP_LOGI(TAG,
             "STC_VOICE_LINE_DONE line=%d duration_ms=%lld interrupted=%d",
             static_cast<int>(line),
             static_cast<long long>((esp_timer_get_time() - started_us) /
                                    1000),
             stop_requested.load() ? 1 : 0);
    stop_requested.store(false);
}

void PlaySfx(const VoiceItem& item) {
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!GetSfx(item.sfx, &data, &size)) {
        ESP_LOGE(TAG, "STC_SFX result=missing asset=%s",
                 kSfxAssets[item.sfx]);
        return;
    }

    auto& app = Application::GetInstance();
    auto& audio = app.GetAudioService();
    stop_requested.store(false);
    playing.store(true);
    audio.ResetDecoder();
    const int64_t started_us = esp_timer_get_time();
    ESP_LOGI(TAG, "STC_SFX_START sfx=%d bytes=%u",
             static_cast<int>(item.sfx), static_cast<unsigned>(size));
    audio.PlaySound(std::string_view(
        reinterpret_cast<const char*>(data), size));
    while (!audio.IsPlaybackIdle() && !stop_requested.load()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    playing.store(false);
    ESP_LOGI(TAG, "STC_SFX_DONE sfx=%d duration_ms=%lld interrupted=%d",
             static_cast<int>(item.sfx),
             static_cast<long long>((esp_timer_get_time() - started_us) /
                                    1000),
             stop_requested.load() ? 1 : 0);
    stop_requested.store(false);
}

void VoiceTask(void*) {
    for (;;) {
        VoiceItem item = {VoiceItemKind::kLine, STC_LINE_COUNT,
                          STC_SFX_COUNT, 0};
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (!queue.empty()) {
                item = queue.front();
                queue.pop_front();
            }
        }
        if (item.kind == VoiceItemKind::kLine &&
            item.line == STC_LINE_COUNT) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
            continue;
        }
        if (item.kind == VoiceItemKind::kSfx) {
            PlaySfx(item);
        } else {
            PlayLocal(item);
        }
    }
}

void ScheduleChatCancel() {
    Application::GetInstance().Schedule([]() {
        auto& app = Application::GetInstance();
        auto& audio = app.GetAudioService();
        app.DismissAlert();
        audio.EnableVoiceProcessing(false);
        audio.ResetDecoder();

        const DeviceState state = app.GetDeviceState();
        if (state == kDeviceStateSpeaking) {
            app.AbortSpeaking(kAbortReasonNone);
        } else if (state == kDeviceStateListening) {
            app.StopListening();
        }
        if (state == kDeviceStateConnecting ||
            state == kDeviceStateListening ||
            state == kDeviceStateSpeaking ||
            state == kDeviceStateNotifying ||
            state == kDeviceStateActivating) {
            app.SetDeviceState(kDeviceStateIdle);
        }
        ESP_LOGI(TAG, "STC_VOICE_DIALOG cancel state=%d",
                 static_cast<int>(state));
    });
}

}  // namespace

extern "C" stc_err_t stc_voice_init(void) {
    if (initialized.exchange(true)) {
        return STC_OK;
    }
    if (xTaskCreate(VoiceTask, "stc_voice", 6144, nullptr, 5,
                    &voice_task_handle) != pdPASS) {
        initialized.store(false);
        return STC_ERR_IO;
    }
    return STC_OK;
}

extern "C" stc_err_t stc_voice_say(stc_line_t line) {
    const int64_t enqueued_us = esp_timer_get_time();
    if (line < 0 || line >= STC_LINE_COUNT) {
        return STC_ERR_CONTRACT;
    }
    if (stc_voice_init() != STC_OK) {
        return STC_ERR_IO;
    }
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!GetPcm(line, &data, &size)) {
        ESP_LOGW(TAG, "say rejected: missing PCM asset %s",
                 assets[line].c_str());
        return STC_ERR_IO;
    }
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (queue.size() >= kQueueCapacity) {
            ESP_LOGW(TAG, "say rejected: queue_full line=%d",
                     static_cast<int>(line));
            return STC_ERR_NOT_READY;
        }
        queue.push_back({VoiceItemKind::kLine, line, STC_SFX_COUNT,
                         enqueued_us});
    }
    xTaskNotifyGive(voice_task_handle);
    return STC_OK;
}

extern "C" stc_err_t stc_voice_play_sfx(stc_sfx_t sfx) {
    const int64_t enqueued_us = esp_timer_get_time();
    if (sfx < 0 || sfx >= STC_SFX_COUNT) {
        return STC_ERR_CONTRACT;
    }
    if (stc_voice_init() != STC_OK) {
        return STC_ERR_IO;
    }
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!GetSfx(sfx, &data, &size)) {
        ESP_LOGW(TAG, "sfx rejected: missing asset %s",
                 kSfxAssets[sfx]);
        return STC_ERR_IO;
    }
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (queue.size() >= kQueueCapacity) {
            ESP_LOGW(TAG, "sfx rejected: queue_full sfx=%d",
                     static_cast<int>(sfx));
            return STC_ERR_NOT_READY;
        }
        queue.push_back({VoiceItemKind::kSfx, STC_LINE_COUNT, sfx,
                         enqueued_us});
    }
    xTaskNotifyGive(voice_task_handle);
    return STC_OK;
}

extern "C" stc_err_t stc_voice_set_mode(stc_mode_t requested_mode) {
    if (requested_mode != STC_MODE_GAME &&
        requested_mode != STC_MODE_CHAT) {
        return STC_ERR_CONTRACT;
    }
    auto& app = Application::GetInstance();
    // STC enters CHAT from its touch UI, so WakeNet is unnecessary. Keeping it
    // disabled leaves enough internal SRAM for the game camera/model stack.
    app.GetAudioService().SetWakeWordAllowed(false);
    mode.store(requested_mode);
    if (requested_mode == STC_MODE_GAME) {
        chat_dialog_pending.store(false);
        stop_requested.store(true);
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            queue.clear();
        }
        ScheduleChatCancel();
        app.Schedule([]() {
            auto& app = Application::GetInstance();
            app.ResetProtocol();
            /* ResetProtocol queued the channel close first. Stop Wi-Fi only
             * after that close task has run, so it cannot surface a network
             * error sound after the game page is visible. */
            app.Schedule([]() {
                stc_network_gate_disable();
                Application::GetInstance()
                    .GetAudioService()
                    .EnableWakeWordDetection(false);
            });
        });
    } else {
        // A later CHAT entry after GAME stopped WiFi must re-enter a setup
        // state so the upstream network-connected handler reactivates OTA and
        // protocol. WakeNet is intentionally left to that activation path.
        if (app.GetDeviceState() == kDeviceStateIdle) {
            app.SetDeviceState(kDeviceStateWifiConfiguring);
        }
        stc_network_gate_enable_for_chat();
    }
    ESP_LOGI(TAG, "STC_VOICE_MODE %s",
             requested_mode == STC_MODE_GAME ? "GAME" : "CHAT");
    return STC_OK;
}

extern "C" stc_mode_t stc_voice_get_mode(void) { return mode.load(); }

extern "C" stc_err_t stc_voice_start_dialog(void) {
    if (mode.load() != STC_MODE_CHAT) {
        return STC_ERR_NOT_READY;
    }
    auto& app = Application::GetInstance();
    if (app.GetDeviceState() == kDeviceStateIdle) {
        chat_dialog_pending.store(false);
        // ToggleChatState uses Xiaozhi's normal auto-stop/VAD listening mode.
        app.ToggleChatState();
    } else {
        chat_dialog_pending.store(true);
        ESP_LOGI(TAG, "STC_VOICE_DIALOG deferred state=%d",
                 static_cast<int>(app.GetDeviceState()));
    }
    return STC_OK;
}

extern "C" void stc_voice_cancel_dialog(void) {
    chat_dialog_pending.store(false);
    ScheduleChatCancel();
}

extern "C" void stc_voice_restart_dialog(void) {
    ScheduleChatCancel();
    chat_dialog_pending.store(true);
}

extern "C" stc_err_t stc_voice_speak_text(const char* text) {
    (void)text;
    return STC_ERR_UNSUPPORTED;
}

extern "C" stc_err_t stc_voice_ask(const char* text, char* reply_out,
                                   size_t reply_len, uint32_t timeout_ms) {
    (void)text;
    (void)reply_out;
    (void)reply_len;
    (void)timeout_ms;
    return STC_ERR_UNSUPPORTED;
}

extern "C" void stc_voice_poll(void) {
    auto& app = Application::GetInstance();
    const DeviceState state = app.GetDeviceState();
    if (mode.load() == STC_MODE_CHAT &&
        chat_dialog_pending.exchange(false) && state == kDeviceStateIdle) {
        app.ToggleChatState();
    }
}

extern "C" bool stc_voice_is_busy(void) {
    if (playing.load()) {
        return true;
    }
    std::lock_guard<std::mutex> lock(queue_mutex);
    return !queue.empty();
}

extern "C" void stc_voice_stop(void) {
    chat_dialog_pending.store(false);
    stop_requested.store(true);
    std::lock_guard<std::mutex> lock(queue_mutex);
    queue.clear();
}
