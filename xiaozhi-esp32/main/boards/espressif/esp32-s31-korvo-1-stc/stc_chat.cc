#include "stc_chat.h"

#include "application.h"
#include "assets.h"

#include <driver/uart.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cJSON.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#define TAG "StcChat"

namespace {

enum class StcChatMode {
    kQuestion,
    kSpeak,
};

struct StcChatRequest {
    StcChatMode mode;
    std::string text;
    int64_t submitted_us;
    uint32_t id;
};

constexpr size_t kMaxTextBytes = 1024;
constexpr size_t kMaxUartLineBytes = 1100;
constexpr size_t kMaxQueuedRequests = 8;
constexpr size_t kServerTextCodepointLimit = 16;
constexpr size_t kQuestionPrefixCodepoints = 4;
constexpr size_t kSpeakPrefixCodepoints = 3;
constexpr int64_t kIdleWaitUs = 60LL * 1000 * 1000;
constexpr int64_t kResponseWaitUs = 20LL * 1000 * 1000;
constexpr int64_t kPlaybackWaitUs = 90LL * 1000 * 1000;

std::mutex request_mutex;
std::deque<StcChatRequest> requests;
std::mutex init_mutex;
bool initialized = false;
uint32_t next_request_id = 1;

const char* ModeName(StcChatMode mode) { return mode == StcChatMode::kQuestion ? "Q" : "S"; }

bool HasVisibleText(std::string_view text) {
    return std::any_of(text.begin(), text.end(),
                       [](unsigned char ch) { return !std::isspace(ch); });
}

size_t Utf8CodepointCount(std::string_view text) {
    return std::count_if(text.begin(), text.end(),
                         [](unsigned char ch) { return (ch & 0xc0) != 0x80; });
}

std::string TruncateUtf8(std::string text, size_t max_bytes) {
    if (text.size() <= max_bytes) {
        return text;
    }
    size_t end = max_bytes;
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) {
        --end;
    }
    text.resize(end);
    return text;
}

std::string EscapeJsonString(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size() + 16);
    static constexpr char kHex[] = "0123456789abcdef";
    for (const unsigned char ch : text) {
        switch (ch) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\b':
                escaped += "\\b";
                break;
            case '\f':
                escaped += "\\f";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                if (ch < 0x20) {
                    escaped += "\\u00";
                    escaped.push_back(kHex[ch >> 4]);
                    escaped.push_back(kHex[ch & 0x0f]);
                } else {
                    escaped.push_back(static_cast<char>(ch));
                }
                break;
        }
    }
    return escaped;
}

std::string BuildOutboundText(StcChatMode mode, const std::string& text) {
    if (text.empty()) {
        return text;
    }
    if (mode == StcChatMode::kSpeak) {
        return "照讀：" + text;
    }
    return "40字：" + text;
}

void EnqueueText(StcChatMode mode, const char* text) {
    stc_chat_init();
    std::string value = text == nullptr ? "" : text;
    if (value.size() > kMaxTextBytes) {
        ESP_LOGW(TAG, "%s text truncated from %u to %u bytes", ModeName(mode),
                 static_cast<unsigned>(value.size()), static_cast<unsigned>(kMaxTextBytes));
        value = TruncateUtf8(std::move(value), kMaxTextBytes);
    }
    if (!HasVisibleText(value)) {
        uint32_t id = 0;
        {
            std::lock_guard<std::mutex> lock(request_mutex);
            id = next_request_id++;
        }
        ESP_LOGW(TAG,
                 "STC_CHAT_DONE id=%lu ok=0 reason=empty_text ttfb_ms=0 playback_ms=0 total_ms=0",
                 static_cast<unsigned long>(id));
        return;
    }
    const size_t text_codepoints = Utf8CodepointCount(value);
    const size_t prefix_codepoints =
        mode == StcChatMode::kQuestion ? kQuestionPrefixCodepoints : kSpeakPrefixCodepoints;
    if (text_codepoints + prefix_codepoints > kServerTextCodepointLimit) {
        uint32_t id = 0;
        {
            std::lock_guard<std::mutex> lock(request_mutex);
            id = next_request_id++;
        }
        ESP_LOGW(TAG,
                 "STC_CHAT_DONE id=%lu ok=0 reason=server_text_limit codepoints=%u "
                 "limit=%u ttfb_ms=0 playback_ms=0 total_ms=0",
                 static_cast<unsigned long>(id),
                 static_cast<unsigned>(text_codepoints + prefix_codepoints),
                 static_cast<unsigned>(kServerTextCodepointLimit));
        return;
    }

    const size_t text_bytes = value.size();
    StcChatRequest request = {
        .mode = mode,
        .text = std::move(value),
        .submitted_us = esp_timer_get_time(),
        .id = 0,
    };
    uint32_t request_id = 0;
    size_t queued = 0;
    bool accepted = false;
    {
        std::lock_guard<std::mutex> lock(request_mutex);
        request_id = next_request_id++;
        request.id = request_id;
        if (requests.size() < kMaxQueuedRequests) {
            requests.push_back(std::move(request));
            queued = requests.size();
            accepted = true;
        }
    }
    if (!accepted) {
        ESP_LOGW(TAG, "STC_CHAT_DONE id=%lu ok=0 reason=queue_full",
                 static_cast<unsigned long>(request_id));
        return;
    }
    {
        ESP_LOGI(TAG, "STC_CHAT t0 id=%lu mode=%s bytes=%u queued=%u",
                 static_cast<unsigned long>(request_id), ModeName(mode),
                 static_cast<unsigned>(text_bytes), static_cast<unsigned>(queued));
    }
}

bool WaitForIdle() {
    auto& app = Application::GetInstance();
    const int64_t deadline_us = esp_timer_get_time() + kIdleWaitUs;
    int64_t last_nudge_us = 0;

    while (esp_timer_get_time() < deadline_us) {
        const DeviceState state = app.GetDeviceState();
        if (state == kDeviceStateIdle) {
            return true;
        }

        // WakeWordInvoke is intentionally called with an empty string here:
        // in speaking/listening states it is only being used to abort or close
        // the previous turn. The real text is submitted after the device idles.
        if ((state == kDeviceStateSpeaking || state == kDeviceStateListening) &&
            esp_timer_get_time() - last_nudge_us >= 2000000) {
            ESP_LOGI(TAG, "Waiting for idle; closing active state %d", static_cast<int>(state));
            app.WakeWordInvoke("");
            last_nudge_us = esp_timer_get_time();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGE(TAG, "Timed out waiting for idle state");
    return false;
}

bool WaitForSpeaking() {
    auto& app = Application::GetInstance();
    const int64_t deadline_us = esp_timer_get_time() + kResponseWaitUs;
    bool saw_active_state = false;
    int64_t idle_since_us = 0;
    while (esp_timer_get_time() < deadline_us) {
        const int64_t now_us = esp_timer_get_time();
        const DeviceState state = app.GetDeviceState();
        if (state == kDeviceStateSpeaking) {
            return true;
        }
        if (state == kDeviceStateConnecting || state == kDeviceStateListening) {
            saw_active_state = true;
            idle_since_us = 0;
        } else if (state == kDeviceStateIdle) {
            // WakeWordInvoke schedules the idle -> connecting transition, so
            // the initial idle sample is not a failed request.
            if (saw_active_state) {
                return false;
            }
            if (idle_since_us == 0) {
                idle_since_us = now_us;
            } else if (now_us - idle_since_us >= 2000000) {
                return false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return false;
}

bool WaitForPlaybackAndCloseTurn() {
    auto& app = Application::GetInstance();
    const int64_t deadline_us = esp_timer_get_time() + kPlaybackWaitUs;

    while (esp_timer_get_time() < deadline_us) {
        const DeviceState state = app.GetDeviceState();
        if (state != kDeviceStateSpeaking && app.GetAudioService().IsPlaybackIdle()) {
            if (state == kDeviceStateListening) {
                // This closes the previous turn before the next queued request.
                app.WakeWordInvoke("");
            }
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return false;
}

void ChatTask(void*) {
    while (true) {
        std::optional<StcChatRequest> request;
        {
            std::lock_guard<std::mutex> lock(request_mutex);
            if (!requests.empty()) {
                request = std::move(requests.front());
                requests.pop_front();
            }
        }
        if (!request) {
            vTaskDelay(pdMS_TO_TICKS(25));
            continue;
        }

        auto& app = Application::GetInstance();
        if (!WaitForIdle()) {
            ESP_LOGE(TAG, "STC_CHAT_DONE id=%lu ok=0 reason=idle_timeout",
                     static_cast<unsigned long>(request->id));
            continue;
        }

        const std::string outbound =
            EscapeJsonString(BuildOutboundText(request->mode, request->text));
        const int64_t send_us = esp_timer_get_time();
        ESP_LOGI(TAG, "STC_CHAT send id=%lu mode=%s outbound_bytes=%u",
                 static_cast<unsigned long>(request->id), ModeName(request->mode),
                 static_cast<unsigned>(outbound.size()));
        app.WakeWordInvoke(outbound);

        const bool saw_speaking = WaitForSpeaking();
        const int64_t first_speaking_us = esp_timer_get_time();
        if (!saw_speaking) {
            ESP_LOGE(TAG,
                     "STC_CHAT_DONE id=%lu ok=0 reason=response_timeout ttfb_ms=%lld "
                     "playback_ms=0 total_ms=%lld",
                     static_cast<unsigned long>(request->id),
                     static_cast<long long>((first_speaking_us - request->submitted_us) / 1000),
                     static_cast<long long>((first_speaking_us - request->submitted_us) / 1000));
            if (!WaitForPlaybackAndCloseTurn()) {
                ESP_LOGE(TAG, "STC_CHAT_PLAYBACK_TIMEOUT id=%lu",
                         static_cast<unsigned long>(request->id));
            }
            continue;
        }

        const int64_t ttfb_ms = (first_speaking_us - request->submitted_us) / 1000;
        ESP_LOGI(TAG,
                 "STC_CHAT first_speaking id=%lu submit_to_speaking_ms=%lld "
                 "send_to_speaking_ms=%lld",
                 static_cast<unsigned long>(request->id), static_cast<long long>(ttfb_ms),
                 static_cast<long long>((first_speaking_us - send_us) / 1000));
        const bool playback_drained = WaitForPlaybackAndCloseTurn();
        const int64_t done_us = esp_timer_get_time();
        const int64_t playback_ms = (done_us - first_speaking_us) / 1000;
        ESP_LOGI(TAG,
                 "STC_CHAT_DONE id=%lu ok=%d reason=%s ttfb_ms=%lld playback_ms=%lld "
                 "total_ms=%lld",
                 static_cast<unsigned long>(request->id), playback_drained ? 1 : 0,
                 playback_drained ? "playback_drained" : "playback_timeout",
                 static_cast<long long>(ttfb_ms), static_cast<long long>(playback_ms),
                 static_cast<long long>((done_us - request->submitted_us) / 1000));
    }
}

void DispatchUartLine(const std::string& line) {
    if (line == "ask" || line == "ASK") {
        stc_chat_send_text("");
        return;
    }
    if (line == "say" || line == "SAY") {
        stc_chat_speak_text("");
        return;
    }
    if (line.size() < 4) {
        return;
    }
    std::string prefix = line.substr(0, 4);
    std::transform(prefix.begin(), prefix.end(), prefix.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (prefix == "ask ") {
        ESP_LOGI(TAG, "UART ask: %s", line.c_str() + 4);
        stc_chat_send_text(line.c_str() + 4);
    } else if (prefix == "say ") {
        ESP_LOGI(TAG, "UART say: %s", line.c_str() + 4);
        stc_chat_speak_text(line.c_str() + 4);
    } else if (line.rfind("phrase ", 0) == 0) {
        const char* key = line.c_str() + 7;
        const char* phrase = stc_phrase_get(key, nullptr);
        ESP_LOGI(TAG, "PHRASE key=%s value=%s", key, phrase);
    } else {
        ESP_LOGW(TAG, "Unknown UART command (expected 'say ', 'ask ', or 'phrase '): %s",
                 line.c_str());
    }
}

void UartTask(void*) {
    if (!uart_is_driver_installed(UART_NUM_0)) {
        const esp_err_t err =
            uart_driver_install(UART_NUM_0, 4096, 0, 0, nullptr, ESP_INTR_FLAG_IRAM);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to install UART0 driver: %s", esp_err_to_name(err));
            vTaskDelete(nullptr);
            return;
        }
    }

    std::string line;
    line.reserve(kMaxUartLineBytes);
    while (true) {
        uint8_t byte = 0;
        const int read = uart_read_bytes(UART_NUM_0, &byte, 1, pdMS_TO_TICKS(200));
        if (read <= 0) {
            continue;
        }
        if (byte == '\r' || byte == '\n') {
            if (!line.empty()) {
                DispatchUartLine(line);
                line.clear();
            }
            continue;
        }
        if (byte == '\b' || byte == 0x7f) {
            if (!line.empty()) {
                line.pop_back();
            }
            continue;
        }
        if (line.size() >= kMaxUartLineBytes) {
            ESP_LOGW(TAG, "UART command exceeded %u bytes; discarded", kMaxUartLineBytes);
            line.clear();
            continue;
        }
        line.push_back(static_cast<char>(byte));
    }
}

std::map<std::string, std::string> LoadPhrases() {
    std::map<std::string, std::string> phrases = {
        {"open", "請將卡片放入任何一個桶"},
        {"right", "答對喇"},
        {"wrong", "答錯喇"},
        {"question", "呢張卡係咩垃圾"},
        {"timeout", "用手勢示意佢再試一次"},
    };

    void* data = nullptr;
    size_t size = 0;
    if (!Assets::GetInstance().GetAssetData("phrases.json", data, size) || data == nullptr) {
        ESP_LOGW(TAG, "phrases.json not found; using built-in fallbacks");
        return phrases;
    }

    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(
        cJSON_ParseWithLength(static_cast<const char*>(data), size), &cJSON_Delete);
    if (!root || !cJSON_IsObject(root.get())) {
        ESP_LOGW(TAG, "phrases.json is invalid; using built-in fallbacks");
        return phrases;
    }

    for (const cJSON* item = root->child; item != nullptr; item = item->next) {
        if (cJSON_IsString(item) && item->string != nullptr) {
            phrases[item->string] = item->valuestring;
        }
    }
    return phrases;
}

}  // namespace

void stc_chat_send_text(const char* text) { EnqueueText(StcChatMode::kQuestion, text); }

void stc_chat_speak_text(const char* text) { EnqueueText(StcChatMode::kSpeak, text); }

const char* stc_phrase_get(const char* key, const char* fallback) {
    static std::once_flag load_once;
    static std::map<std::string, std::string> phrases;
    std::call_once(load_once, []() { phrases = LoadPhrases(); });

    if (key != nullptr) {
        auto it = phrases.find(key);
        if (it != phrases.end()) {
            return it->second.c_str();
        }
    }
    return fallback == nullptr ? "" : fallback;
}

void stc_chat_init() {
    std::lock_guard<std::mutex> lock(init_mutex);
    if (initialized) {
        return;
    }
    if (xTaskCreate(ChatTask, "stc_chat", 8192, nullptr, 5, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create chat task");
        return;
    }
    if (xTaskCreate(UartTask, "stc_uart", 4096, nullptr, 4, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UART task");
        return;
    }
    initialized = true;
}
