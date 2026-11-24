#include "application.h"
#include "assets/lang_config.h"
#include "../esp32-s31-korvo-1/bsp_adc_calibration.h"
#include "camera_capture.h"
#include "button.h"
#include "codecs/es8389_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "led/single_led.h"
#include "stc_chat_display.h"
#include "stc_game_runtime.h"
#include "stc_mode_request.h"
#include "stc_network_gate.h"
#include "stc_shell_mode.h"
#include "stc_shell_ui.h"
#include "stc_voice.h"
#include "wifi_board.h"

#include <driver/i2c_master.h>
#include <driver/uart.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_event.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_lcd_touch_gt1151.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <led_strip.h>
#include <lvgl.h>
#include <wifi_manager.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>

#define TAG "Esp32S31Korvo1Stc"

static std::atomic<bool> chat_network_enabled{false};
static esp_lcd_panel_handle_t stc_panel_handle = nullptr;

static void ConfigureDnsFallback(void*, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data) {
    if (event_base != IP_EVENT || event_id != IP_EVENT_STA_GOT_IP ||
        event_data == nullptr) {
        return;
    }
    auto* event = static_cast<ip_event_got_ip_t*>(event_data);
    esp_netif_dns_info_t dns = {};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ESP_IP4TOADDR(1, 1, 1, 1);
    const esp_err_t error = esp_netif_set_dns_info(
        event->esp_netif, ESP_NETIF_DNS_BACKUP, &dns);
    ESP_LOGI(TAG, "STC_DNS fallback=1.1.1.1 result=%s",
             esp_err_to_name(error));
}

static void StopCloudNetwork() {
    auto& wifi = WifiManager::GetInstance();
    wifi.StopConfigAp();
    wifi.StopStation();
}

extern "C" void stc_network_gate_enable_for_chat(void) {
    if (!chat_network_enabled.exchange(true)) {
        ESP_LOGI(TAG, "STC_NETWORK_GATE state=CHAT action=start");
        (void)stc_game_runtime_suspend_hand();
        Board::GetInstance().StartNetwork();
    }
}

extern "C" void stc_network_gate_disable(void) {
    if (chat_network_enabled.exchange(false)) {
        ESP_LOGI(TAG, "STC_NETWORK_GATE state=GAME action=stop");
        StopCloudNetwork();
        const stc_err_t hand = stc_game_runtime_resume_hand();
        const esp_err_t channel =
            esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
        if (hand != STC_OK || channel != ESP_OK) {
            ESP_LOGE(TAG,
                     "STC_NETWORK_GATE restore failed hand=%d channel=%s",
                     static_cast<int>(hand), esp_err_to_name(channel));
        }
    }
}

extern "C" bool stc_network_gate_is_enabled(void) {
    return chat_network_enabled.load();
}

static uint32_t NowMs() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

static bool PrepareWifiManagerForChat() {
    auto& wifi = WifiManager::GetInstance();
    if (wifi.IsInitialized()) {
        return true;
    }

    WifiManagerConfig config;
    config.ssid_prefix = "Xiaozhi";
    config.language = Lang::CODE;
    config.show_ota_config = true;
    config.show_sleep_config = true;
    uint8_t mac[6] = {};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        char hostname[32];
        snprintf(hostname, sizeof(hostname), "%s-%02X%02X",
                 config.ssid_prefix.c_str(), mac[4], mac[5]);
        config.station_hostname = hostname;
    }
    return wifi.Initialize(config);
}

class StcStatusLed : public Led {
public:
    explicit StcStatusLed(gpio_num_t gpio) {
        led_strip_config_t strip_config = {};
        strip_config.strip_gpio_num = gpio;
        strip_config.max_leds = 1;
        strip_config.color_component_format =
            LED_STRIP_COLOR_COMPONENT_FMT_GRB;
        strip_config.led_model = LED_MODEL_WS2812;

        led_strip_rmt_config_t rmt_config = {};
        rmt_config.resolution_hz = 10 * 1000 * 1000;
        ESP_ERROR_CHECK(
            led_strip_new_rmt_device(&strip_config, &rmt_config, &strip_));
        led_strip_clear(strip_);
    }

    ~StcStatusLed() override {
        if (strip_ != nullptr) {
            led_strip_del(strip_);
        }
    }

    void OnStateChanged() override {}

    void SetColor(uint8_t red, uint8_t green, uint8_t blue) {
        if (red == red_ && green == green_ && blue == blue_) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        red_ = red;
        green_ = green;
        blue_ = blue;
        led_strip_set_pixel(strip_, 0, red_, green_, blue_);
        led_strip_refresh(strip_);
    }

private:
    led_strip_handle_t strip_ = nullptr;
    std::mutex mutex_;
    uint8_t red_ = UINT8_MAX;
    uint8_t green_ = UINT8_MAX;
    uint8_t blue_ = UINT8_MAX;
};

typedef enum {
    BSP_ADC_BUTTON_SET,
    BSP_ADC_BUTTON_MODE,
    BSP_ADC_BUTTON_VOL_DOWN,
    BSP_ADC_BUTTON_VOL_UP,
    BSP_ADC_BUTTON_NUM
} bsp_adc_button_t;

typedef struct {
    button_driver_t base;
    uint16_t min_mv;
    uint16_t max_mv;
} korvo_adc_button_driver_t;

typedef struct {
    bsp_adc_button_t button;
    uint16_t center_mv;
} korvo_adc_button_ladder_t;

static adc_oneshot_unit_handle_t s_button_adc_handle = nullptr;
static korvo_adc_button_driver_t s_button_drivers[BSP_ADC_BUTTON_NUM];
static const korvo_adc_button_ladder_t s_button_ladder[BSP_ADC_BUTTON_NUM] = {
    {BSP_ADC_BUTTON_VOL_UP, ADC_BUTTON_VOL_UP_MV},
    {BSP_ADC_BUTTON_VOL_DOWN, ADC_BUTTON_VOL_DOWN_MV},
    {BSP_ADC_BUTTON_MODE, ADC_BUTTON_MODE_MV},
    {BSP_ADC_BUTTON_SET, ADC_BUTTON_SET_MV},
};

static uint16_t ButtonMvMidpoint(uint16_t a, uint16_t b) {
    return static_cast<uint16_t>(
        (static_cast<uint32_t>(a) + static_cast<uint32_t>(b)) / 2U);
}

static uint8_t KorvoAdcButtonGetKeyLevel(button_driver_t* driver) {
    auto* btn = reinterpret_cast<korvo_adc_button_driver_t*>(driver);
    uint32_t raw_sum = 0;
    int raw = 0;
    for (int i = 0; i < 4; ++i) {
        if (adc_oneshot_read(s_button_adc_handle, ADC_CHANNEL_0, &raw) !=
            ESP_OK) {
            return BUTTON_INACTIVE;
        }
        raw_sum += raw;
    }
    int mv = 0;
    if (bsp_s31_adc_calibration_raw_to_mv(
            ADC_UNIT_1, static_cast<int>(raw_sum / 4), &mv) != ESP_OK) {
        return BUTTON_INACTIVE;
    }
    return (mv >= btn->min_mv && mv <= btn->max_mv) ? BUTTON_ACTIVE
                                                     : BUTTON_INACTIVE;
}

static void RequestMode(stc_shell_state_t state, const char* source) {
    const stc_mode_t mode =
        state == STC_SHELL_GAME ? STC_MODE_GAME : STC_MODE_CHAT;
    (void)stc_mode_request_post(mode);
    ESP_LOGI(TAG, "STC_MODE_REQUEST target=%s source=%s",
             state == STC_SHELL_GAME ? "GAME" : "CHAT", source);
}

static void UpdateStatusLed() {
    auto* led =
        static_cast<StcStatusLed*>(Board::GetInstance().GetLed());
    if (led == nullptr) {
        return;
    }
    switch (stc_shell_get()) {
        case STC_SHELL_GAME:
            led->SetColor(0, 0, 8);
            break;
        case STC_SHELL_CHAT_STARTING:
            led->SetColor(0, 12, 12);
            break;
        case STC_SHELL_CHAT_READY:
            led->SetColor(16, 0, 16);
            break;
    }
}

static void LogStatus(const char* source) {
    ESP_LOGI(TAG,
             "STC_STATUS source=%s shell=%d voice_mode=%d device_state=%d "
             "network=%d hand_online=%d voice_busy=%d voice_detected=%d "
             "playback_idle=%d",
             source, static_cast<int>(stc_shell_get()),
             static_cast<int>(stc_voice_get_mode()),
             static_cast<int>(Application::GetInstance().GetDeviceState()),
             stc_network_gate_is_enabled() ? 1 : 0,
             stc_game_runtime_hand_online() ? 1 : 0,
             stc_voice_is_busy() ? 1 : 0,
             Application::GetInstance()
                     .GetAudioService()
                     .IsVoiceDetected()
                 ? 1
                 : 0,
             Application::GetInstance()
                     .GetAudioService()
                     .IsPlaybackIdle()
                 ? 1
                 : 0);
    stc_game_runtime_log_status(source);
}

static void RunVoiceSelfTest() {
    static const stc_line_t lines[] = {
        STC_LINE_OPEN,    STC_LINE_CORRECT, STC_LINE_WRONG,
        STC_LINE_TIMEOUT, STC_LINE_RETRY,   STC_LINE_SUMMARY,
    };
    stc_voice_stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    for (size_t index = 0; index < sizeof(lines) / sizeof(lines[0]);
         ++index) {
        stc_err_t result;
        do {
            result = stc_voice_say(lines[index]);
            if (result == STC_ERR_NOT_READY) {
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        } while (result == STC_ERR_NOT_READY);
        ESP_LOGI(TAG, "VOICE_SELFTEST enqueue=%u line=%d result=%d",
                 static_cast<unsigned>(index + 1),
                 static_cast<int>(lines[index]),
                 static_cast<int>(result));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    while (stc_voice_is_busy()) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_LOGI(TAG, "VOICE_SELFTEST sequential=6 result=complete");
}

static void RunSfxSelfTest() {
    stc_voice_stop();
    for (int sfx = 0; sfx < STC_SFX_COUNT; ++sfx) {
        const stc_err_t result =
            stc_voice_play_sfx((stc_sfx_t)sfx);
        ESP_LOGI(TAG, "SFX_SELFTEST enqueue=%d result=%d", sfx,
                 static_cast<int>(result));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    while (stc_voice_is_busy()) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_LOGI(TAG, "SFX_SELFTEST sequential=6 result=complete");
}

static void LogScreenPixelStatistics() {
    if (stc_panel_handle == nullptr) {
        ESP_LOGE(TAG, "STC_SCREEN unavailable panel=null");
        return;
    }
    void* frame_buffers[2] = {};
    const esp_err_t error = esp_lcd_rgb_panel_get_frame_buffer(
        stc_panel_handle, 2, &frame_buffers[0], &frame_buffers[1]);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "STC_SCREEN unavailable error=%d",
                 static_cast<int>(error));
        return;
    }

    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    if (!lock) {
        ESP_LOGE(TAG, "STC_SCREEN unavailable display_lock=0");
        return;
    }

    for (int frame = 0; frame < 2; ++frame) {
        const auto* pixels =
            static_cast<const uint16_t*>(frame_buffers[frame]);
        uint64_t red_sum = 0;
        uint64_t green_sum = 0;
        uint64_t blue_sum = 0;
        uint32_t samples = 0;
        uint32_t white = 0;
        for (int y = 0; y < DISPLAY_HEIGHT; y += 8) {
            for (int x = 0; x < DISPLAY_WIDTH; x += 8) {
                const uint16_t pixel =
                    pixels[y * DISPLAY_WIDTH + x];
                const uint8_t red = static_cast<uint8_t>(
                    ((pixel >> 11) & 0x1F) * 255 / 31);
                const uint8_t green = static_cast<uint8_t>(
                    ((pixel >> 5) & 0x3F) * 255 / 63);
                const uint8_t blue =
                    static_cast<uint8_t>((pixel & 0x1F) * 255 / 31);
                red_sum += red;
                green_sum += green;
                blue_sum += blue;
                ++samples;
                white += red >= 245 && green >= 245 && blue >= 245;
            }
        }
        ESP_LOGI(TAG,
                 "STC_SCREEN fb=%d avg_r=%lu avg_g=%lu avg_b=%lu "
                 "white_permille=%lu",
                 frame, static_cast<unsigned long>(red_sum * 1000 / samples),
                 static_cast<unsigned long>(green_sum * 1000 / samples),
                 static_cast<unsigned long>(blue_sum * 1000 / samples),
                 static_cast<unsigned long>(white * 1000 / samples));
    }
}

static void HandleUartCommand(char* command) {
    command[strcspn(command, "\r\n")] = '\0';
    if (strcmp(command, "stc status") == 0) {
        LogStatus("uart");
    } else if (strcmp(command, "stc screen") == 0) {
        LogScreenPixelStatistics();
    } else if (strcmp(command, "stc voice") == 0) {
        RunVoiceSelfTest();
    } else if (strcmp(command, "stc sfx") == 0) {
        RunSfxSelfTest();
    } else if (strcmp(command, "stc chat") == 0) {
        RequestMode(STC_SHELL_CHAT_STARTING, "uart");
    } else if (strcmp(command, "stc game") == 0) {
        RequestMode(STC_SHELL_GAME, "uart");
    } else if (strcmp(command, "stc play") == 0) {
        stc_game_runtime_play_button();
        ESP_LOGI(TAG, "STC_CMD play submitted");
    } else if (strcmp(command, "stc scan") == 0) {
        stc_game_runtime_submit_action(STC_UI_ACTION_SCAN, -1);
    } else if (strcmp(command, "stc next") == 0) {
        stc_game_runtime_submit_action(STC_UI_ACTION_NEXT, -1);
    } else if (strcmp(command, "stc home") == 0) {
        stc_game_runtime_submit_action(STC_UI_ACTION_HOME, -1);
    } else if (strncmp(command, "stc bin ", 8) == 0 &&
               command[8] >= '0' && command[8] <= '3' &&
               command[9] == '\0') {
        stc_game_runtime_submit_action(STC_UI_ACTION_SELECT_BIN,
                                       command[8] - '0');
    } else if (strncmp(command, "stc mock ", 9) == 0 &&
               command[9] >= '0' && command[9] <= '3' &&
               command[10] == '\0') {
        stc_game_runtime_submit_mock_label(command[9] - '0');
    } else if (command[0] != '\0') {
        ESP_LOGW(TAG, "STC_CMD unknown='%s'", command);
    }
}

static void StcCommandTask(void*) {
    const uart_config_t config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    const esp_err_t config_error = uart_param_config(UART_NUM_0, &config);
    const esp_err_t install_error =
        uart_driver_install(UART_NUM_0, 1024, 0, 0, nullptr, 0);
    if (config_error != ESP_OK ||
        (install_error != ESP_OK &&
         install_error != ESP_ERR_INVALID_STATE)) {
        ESP_LOGE(TAG, "STC_CMD uart init failed config=%d install=%d",
                 static_cast<int>(config_error),
                 static_cast<int>(install_error));
        vTaskDelete(nullptr);
    }
    ESP_LOGI(TAG,
             "STC_CMD ready: 'stc status', 'stc screen', 'stc voice', "
             "'stc sfx', "
             "'stc chat', 'stc game', 'stc play', 'stc scan', 'stc next', "
             "'stc home'");
    char line[128] = {};
    size_t length = 0;
    for (;;) {
        uint8_t byte = 0;
        const int read =
            uart_read_bytes(UART_NUM_0, &byte, 1, pdMS_TO_TICKS(100));
        if (read != 1) {
            continue;
        }
        if (byte == '\r' || byte == '\n') {
            if (length > 0) {
                line[length] = '\0';
                HandleUartCommand(line);
                length = 0;
            }
            continue;
        }
        if (length + 1 < sizeof(line)) {
            line[length++] = static_cast<char>(byte);
        } else {
            length = 0;
            ESP_LOGW(TAG, "STC_CMD line too long");
        }
    }
}

static void ShellExitGame() { stc_voice_stop(); }

static void ShellEnterChat() {
    stc_voice_stop();
    (void)stc_voice_set_mode(STC_MODE_CHAT);
    stc_shell_ui_set_state(STC_SHELL_CHAT_STARTING);
    ESP_LOGI(TAG, "APP_STATE GAME -> CHAT_STARTING");
}

static void ShellExitChat() {
    stc_voice_stop();
    (void)stc_voice_set_mode(STC_MODE_GAME);
    stc_shell_ui_set_state(STC_SHELL_GAME);
    ESP_LOGI(TAG, "APP_STATE CHAT -> GAME");
}

static void ShellEnterGame() {
    stc_shell_ui_set_state(STC_SHELL_GAME);
    stc_game_runtime_render();
}

static void GameTask(void*) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    stc_mode_request_init();
    stc_shell_init();
    const stc_shell_hooks_t hooks = {
        .exit_game = ShellExitGame,
        .enter_chat = ShellEnterChat,
        .exit_chat = ShellExitChat,
        .enter_game = ShellEnterGame,
    };
    stc_shell_set_hooks(&hooks);

    (void)stc_voice_init();
    (void)stc_voice_set_mode(STC_MODE_GAME);
    if (!PrepareWifiManagerForChat()) {
        ESP_LOGE(TAG, "WiFi manager pre-initialization failed");
    } else {
        const esp_err_t dns_handler = esp_event_handler_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, ConfigureDnsFallback, nullptr);
        ESP_LOGI(TAG, "STC_DNS handler=%s", esp_err_to_name(dns_handler));
    }
    const stc_err_t runtime = stc_game_runtime_start();
    if (runtime != STC_OK) {
        ESP_LOGE(TAG, "STC_GAME_RUNTIME start failed=%d",
                 static_cast<int>(runtime));
    }
    xTaskCreate(StcCommandTask, "stc_cmd", 4096, nullptr, 4, nullptr);
    vTaskDelay(pdMS_TO_TICKS(250));

    TickType_t last_ui_refresh = 0;
    DeviceState last_device_state = kDeviceStateUnknown;
    bool last_voice_detected = false;
    bool dialog_requested = false;

    for (;;) {
        stc_voice_poll();

        stc_mode_t target;
        const bool has_request = stc_mode_request_take(&target);
        if (has_request && target == STC_MODE_CHAT &&
            stc_shell_get() == STC_SHELL_GAME) {
            (void)stc_shell_enter_chat(NowMs());
            dialog_requested = false;
        } else if (has_request && target == STC_MODE_GAME &&
                   stc_shell_get() != STC_SHELL_GAME) {
            (void)stc_shell_exit_chat();
            dialog_requested = false;
        }

        auto& app = Application::GetInstance();
        const DeviceState device_state = app.GetDeviceState();
        const bool network_connected =
            WifiManager::GetInstance().IsConnected();

        if (stc_shell_get() == STC_SHELL_CHAT_STARTING &&
            !dialog_requested && device_state == kDeviceStateIdle &&
            network_connected) {
            const stc_err_t dialog = stc_voice_start_dialog();
            if (dialog == STC_OK) {
                dialog_requested = true;
                (void)stc_shell_mark_chat_ready();
                stc_shell_ui_set_state(STC_SHELL_CHAT_READY);
                ESP_LOGI(TAG,
                         "APP_STATE CHAT_STARTING -> CHAT_READY");
            } else {
                ESP_LOGW(TAG, "APP_STATE CHAT dialog failed=%d",
                         static_cast<int>(dialog));
            }
        }

        if (stc_shell_get() != STC_SHELL_GAME) {
            const bool active_state =
                device_state == kDeviceStateConnecting ||
                device_state == kDeviceStateSpeaking ||
                device_state == kDeviceStateActivating;
            const bool voice_detected = app.IsVoiceDetected();
            const bool voice_started =
                voice_detected && !last_voice_detected;
            const bool playback_active =
                !app.GetAudioService().IsPlaybackIdle();
            if (device_state != last_device_state || active_state ||
                voice_started || playback_active) {
                (void)stc_shell_notify_activity(NowMs());
            }
            last_voice_detected = voice_detected;
        } else {
            last_voice_detected = app.IsVoiceDetected();
        }
        last_device_state = device_state;

        stc_shell_tick(NowMs());
        if (stc_shell_get() == STC_SHELL_GAME &&
            stc_voice_get_mode() != STC_MODE_GAME) {
            // Defensive mirror if an upstream component closed the voice gate.
            (void)stc_shell_exit_chat();
            dialog_requested = false;
        }

        UpdateStatusLed();
        const TickType_t now = xTaskGetTickCount();
        if (last_ui_refresh == 0 ||
            now - last_ui_refresh >= pdMS_TO_TICKS(100)) {
            last_ui_refresh = now;
            stc_shell_ui_refresh_status(stc_shell_get(), device_state,
                                        network_connected);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

class Esp32S31Korvo1Stc : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_ = nullptr;
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    Button boot_button_;
    Button* adc_button_[BSP_ADC_BUTTON_NUM] = {};
    LcdDisplay* display_ = nullptr;

    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_config = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config,
                                           &codec_i2c_bus_));
        stc_camera_capture_set_i2c_bus(codec_i2c_bus_);
    }

    void ChangeVol(int delta) {
        auto codec = GetAudioCodec();
        int volume = codec->output_volume() + delta;
        volume = volume > 100 ? 100 : volume;
        volume = volume < 0 ? 0 : volume;
        codec->SetOutputVolume(volume);
        ESP_LOGI(TAG, "Volume: %d", volume);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            if (stc_shell_get() != STC_SHELL_GAME) {
                RequestMode(STC_SHELL_GAME, "boot");
            } else {
                stc_game_runtime_play_button();
            }
        });

        const adc_oneshot_unit_init_cfg_t adc_unit_config = {
            .unit_id = ADC_UNIT_1,
        };
        ESP_ERROR_CHECK(
            adc_oneshot_new_unit(&adc_unit_config, &adc_handle_));
        const adc_oneshot_chan_cfg_t adc_chan_config = {
            .atten = ADC_ATTEN_DB_0,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(
            adc_handle_, ADC_CHANNEL_0, &adc_chan_config));
        s_button_adc_handle = adc_handle_;
        ESP_ERROR_CHECK(bsp_s31_adc_calibration_init(
            adc_handle_, ADC_UNIT_1, ADC_CHANNEL_0));

        const button_config_t button_config = {};
        for (int i = 0; i < BSP_ADC_BUTTON_NUM; ++i) {
            const bsp_adc_button_t button = s_button_ladder[i].button;
            const uint16_t center = s_button_ladder[i].center_mv;
            if (i == 0) {
                const uint16_t upper = ButtonMvMidpoint(
                    s_button_ladder[0].center_mv,
                    s_button_ladder[1].center_mv);
                const uint16_t margin = upper - center;
                s_button_drivers[i].min_mv =
                    center > margin ? center - margin : 0;
                s_button_drivers[i].max_mv = upper;
            } else if (i == BSP_ADC_BUTTON_NUM - 1) {
                s_button_drivers[i].min_mv = ButtonMvMidpoint(
                    s_button_ladder[i - 1].center_mv, center);
                s_button_drivers[i].max_mv =
                    ButtonMvMidpoint(center, ADC_BUTTON_IDLE_MV);
            } else {
                s_button_drivers[i].min_mv = ButtonMvMidpoint(
                    s_button_ladder[i - 1].center_mv, center);
                s_button_drivers[i].max_mv = ButtonMvMidpoint(
                    center, s_button_ladder[i + 1].center_mv);
            }
            s_button_drivers[i].base.get_key_level =
                KorvoAdcButtonGetKeyLevel;
            button_handle_t handle = nullptr;
            ESP_ERROR_CHECK(iot_button_create(
                &button_config, &s_button_drivers[i].base, &handle));
            adc_button_[button] = new Button(handle);
        }

        auto volume_up_button = adc_button_[BSP_ADC_BUTTON_VOL_UP];
        volume_up_button->OnClick([this]() { ChangeVol(10); });
        volume_up_button->OnLongPress(
            [this]() { GetAudioCodec()->SetOutputVolume(100); });

        auto volume_down_button = adc_button_[BSP_ADC_BUTTON_VOL_DOWN];
        volume_down_button->OnClick([this]() { ChangeVol(-10); });
        volume_down_button->OnLongPress(
            [this]() { GetAudioCodec()->SetOutputVolume(0); });

        auto set_button = adc_button_[BSP_ADC_BUTTON_SET];
        set_button->OnClick([this]() { EnterWifiConfigMode(); });
        set_button->OnLongPress(
            []() { RequestMode(STC_SHELL_CHAT_STARTING, "set_long"); });

        auto mode_button = adc_button_[BSP_ADC_BUTTON_MODE];
        mode_button->OnClick(
            []() { RequestMode(STC_SHELL_GAME, "mode"); });
    }

    void InitializeRgbDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel_handle = nullptr;

        esp_lcd_rgb_panel_config_t rgb_config = {
            .clk_src = LCD_CLK_SRC_DEFAULT,
            .timings =
                {
                    .pclk_hz = 16 * 1000 * 1000,
                    .h_res = DISPLAY_WIDTH,
                    .v_res = DISPLAY_HEIGHT,
                    .hsync_pulse_width = 4,
                    .hsync_back_porch = 8,
                    .hsync_front_porch = 8,
                    .vsync_pulse_width = 4,
                    .vsync_back_porch = 8,
                    .vsync_front_porch = 8,
                    .flags =
                        {
                            .pclk_active_neg = true,
                        },
                },
            .data_width = 16,
            .in_color_format = LCD_COLOR_FMT_RGB565,
            .out_color_format = LCD_COLOR_FMT_RGB565,
            .num_fbs = 2,
            .bounce_buffer_size_px = DISPLAY_WIDTH * 8,
            .hsync_gpio_num = LCD_RGB_HSYNC,
            .vsync_gpio_num = LCD_RGB_VSYNC,
            .de_gpio_num = LCD_RGB_DE,
            .pclk_gpio_num = LCD_RGB_PCLK,
            .disp_gpio_num = LCD_RGB_DISP,
            .data_gpio_nums =
                {
                    LCD_RGB_DATA0,
                    LCD_RGB_DATA1,
                    LCD_RGB_DATA2,
                    LCD_RGB_DATA3,
                    LCD_RGB_DATA4,
                    LCD_RGB_DATA5,
                    LCD_RGB_DATA6,
                    LCD_RGB_DATA7,
                    LCD_RGB_DATA8,
                    LCD_RGB_DATA9,
                    LCD_RGB_DATA10,
                    LCD_RGB_DATA11,
                    LCD_RGB_DATA12,
                    LCD_RGB_DATA13,
                    LCD_RGB_DATA14,
                    LCD_RGB_DATA15,
                },
            .flags =
                {
                    .fb_in_psram = 1,
                },
        };

        ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&rgb_config, &panel_handle));
        stc_panel_handle = panel_handle;
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

        display_ = new StcChatDisplay(
            panel_io, panel_handle, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
            DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
            [](stc_shell_state_t state) {
                RequestMode(state, "touch");
            });
    }

    void InitializeTouch() {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH - 1,
            .y_max = DISPLAY_HEIGHT - 1,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = GPIO_NUM_NC,
            .levels =
                {
                    .reset = 0,
                    .interrupt = 0,
                },
            .flags =
                {
                    .swap_xy = 0,
                    .mirror_x = 0,
                    .mirror_y = 0,
                },
        };

        esp_lcd_panel_io_handle_t tp_io_handle = nullptr;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {};
        tp_io_config.scl_speed_hz = 400 * 1000;
        tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT1151_ADDRESS;
        tp_io_config.control_phase_bytes = 1;
        tp_io_config.dc_bit_offset = 0;
        tp_io_config.lcd_cmd_bits = 16;
        tp_io_config.flags.disable_control_phase = 1;

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(
            codec_i2c_bus_, &tp_io_config, &tp_io_handle));
        ESP_ERROR_CHECK(
            esp_lcd_touch_new_i2c_gt1151(tp_io_handle, &tp_cfg, &tp));

        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };
        lvgl_port_add_touch(&touch_cfg);
        ESP_LOGI(TAG, "GT1151 touch initialized");
    }

public:
    Esp32S31Korvo1Stc() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeButtons();
        InitializeRgbDisplay();
        InitializeTouch();
        xTaskCreate(GameTask, "stc_shell", 8192, nullptr, 5, nullptr);
    }

    AudioCodec* GetAudioCodec() override {
        static Es8389AudioCodec audio_codec(
            codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8389_ADDR,
            AUDIO_CODEC_USE_MCLK, AUDIO_INPUT_CHANNELS,
            AUDIO_OUTPUT_CHANNELS);
        return &audio_codec;
    }

    void StartNetwork() override {
        if (!stc_network_gate_is_enabled()) {
            ESP_LOGI(TAG,
                     "STC_NETWORK_GATE state=GAME action=block "
                     "cloud_connections=0");
            return;
        }
        WifiBoard::StartNetwork();
    }

    Led* GetLed() override {
        static StcStatusLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    Display* GetDisplay() override { return display_; }

    Camera* GetCamera() override { return nullptr; }
};

DECLARE_BOARD(Esp32S31Korvo1Stc);
