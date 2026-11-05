#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stc_bin.h"
#include "stc_bin_debug.h"
#include "stc_types.h"

#define CONSOLE_UART UART_NUM_0
#define CONSOLE_BAUD 115200
#define CONSOLE_BUFFER_SIZE 1024

static const char *TAG = "tof_bringup";
static volatile bool stream_enabled;

static const char *stc_error_name(stc_err_t error) {
    switch (error) {
    case STC_OK:
        return "OK";
    case STC_ERR_TIMEOUT:
        return "TIMEOUT";
    case STC_ERR_NOT_READY:
        return "NOT_READY";
    case STC_ERR_IO:
        return "IO";
    case STC_ERR_CONTRACT:
        return "CONTRACT";
    case STC_ERR_CANCELLED:
        return "CANCELLED";
    case STC_ERR_UNSUPPORTED:
        return "UNSUPPORTED";
    default:
        return "UNKNOWN";
    }
}

static void print_help(void) {
    ESP_LOGI(TAG, "commands:");
    ESP_LOGI(TAG, "  tof info");
    ESP_LOGI(TAG, "  tof stream on|off");
    ESP_LOGI(TAG, "  tof cal <ch 0-3> <known-mm>");
    ESP_LOGI(TAG, "  tof selftest");
    ESP_LOGI(TAG, "  tof monitor [off|milliseconds]");
}

static void print_info(void) {
    for (unsigned bin = 0; bin < STC_BIN_COUNT; ++bin) {
        stc_bin_debug_info_t info = {0};
        const stc_err_t error =
            stc_bin_debug_info((stc_bin_t)bin, &info);
        if (error != STC_OK) {
            ESP_LOGE(TAG, "TOF_INFO ch=%u error=%s", bin,
                     stc_error_name(error));
            continue;
        }
        const uint16_t fresh = stc_bin_distance((stc_bin_t)bin);
        if (fresh != 0) {
            info.last_mm = fresh;
        }
        ESP_LOGI(TAG,
                 "TOF_INFO ch=%u offset=%d xtalk=%u dist_mm=%u fault=%u",
                 bin, info.offset, info.xtalk, info.last_mm, info.fault);
    }
    ESP_LOGI(TAG, "TOF_FAULT_MASK 0x%02x", stc_bin_fault_mask());
}

static void run_selftest(void) {
    const stc_err_t error = stc_bin_selftest();
    const uint8_t mask = stc_bin_fault_mask();
    if (error == STC_OK && mask == 0) {
        ESP_LOGI(TAG, "RESULT H1 PASS fault_mask=0");
    } else {
        ESP_LOGE(TAG, "RESULT H1 FAIL error=%s fault_mask=0x%02x",
                 stc_error_name(error), mask);
    }
}

static void run_calibration(unsigned channel, unsigned target_mm) {
    if (channel >= STC_BIN_COUNT || target_mm == 0 || target_mm > 200) {
        ESP_LOGE(TAG, "usage: tof cal <ch 0-3> <known-mm 1-200>");
        return;
    }
    const stc_err_t error =
        stc_bin_calibrate_offset((stc_bin_t)channel, (uint16_t)target_mm);
    ESP_LOGI(TAG, "TOF_CAL ch=%u target_mm=%u result=%s", channel, target_mm,
             stc_error_name(error));
}

static void handle_command(char *line) {
    while (*line == ' ') {
        ++line;
    }
    if (*line == '\0') {
        return;
    }
    if (strcmp(line, "help") == 0) {
        print_help();
    } else if (strcmp(line, "tof info") == 0) {
        print_info();
    } else if (strcmp(line, "tof stream on") == 0) {
        stream_enabled = true;
        ESP_LOGI(TAG, "stream enabled");
    } else if (strcmp(line, "tof stream off") == 0) {
        stream_enabled = false;
        ESP_LOGI(TAG, "stream disabled");
    } else if (strcmp(line, "tof selftest") == 0) {
        run_selftest();
    } else if (strncmp(line, "tof cal ", 8) == 0) {
        unsigned channel = 0;
        unsigned target_mm = 0;
        if (sscanf(line + 8, "%u %u", &channel, &target_mm) == 2) {
            run_calibration(channel, target_mm);
        } else {
            ESP_LOGE(TAG, "usage: tof cal <ch 0-3> <known-mm>");
        }
    } else if (strcmp(line, "tof monitor off") == 0) {
        const stc_err_t error = stc_bin_stop_monitor();
        ESP_LOGI(TAG, "monitor stopped: %s", stc_error_name(error));
    } else if (strncmp(line, "tof monitor", 11) == 0) {
        unsigned timeout_ms = 15000;
        if (line[11] != '\0') {
            if (sscanf(line + 11, "%u", &timeout_ms) != 1 ||
                timeout_ms > UINT16_MAX) {
                ESP_LOGE(TAG, "usage: tof monitor [off|milliseconds]");
                return;
            }
        }
        const stc_err_t error =
            stc_bin_start_monitor((uint16_t)timeout_ms);
        ESP_LOGI(TAG, "monitor armed for %u ms: %s", timeout_ms,
                 stc_error_name(error));
    } else {
        ESP_LOGE(TAG, "unknown command: %s", line);
    }
}

static void console_task(void *argument) {
    (void)argument;
    char line[96];
    size_t length = 0;
    print_help();

    for (;;) {
        uint8_t byte = 0;
        const int read =
            uart_read_bytes(CONSOLE_UART, &byte, 1, pdMS_TO_TICKS(20));
        if (read != 1) {
            continue;
        }
        uart_write_bytes(CONSOLE_UART, (const char *)&byte, 1);
        if (byte == '\r' || byte == '\n') {
            line[length] = '\0';
            handle_command(line);
            length = 0;
            continue;
        }
        if (byte == 0x08 || byte == 0x7F) {
            if (length > 0) {
                --length;
            }
            continue;
        }
        if (byte >= 0x20 && byte < 0x7F && length + 1 < sizeof(line)) {
            line[length++] = (char)byte;
        }
    }
}

static void init_console(void) {
    const uart_config_t config = {
        .baud_rate = CONSOLE_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(CONSOLE_UART, CONSOLE_BUFFER_SIZE, 0, 0,
                                        NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(CONSOLE_UART, &config));
    ESP_ERROR_CHECK(uart_set_pin(CONSOLE_UART, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
    xTaskCreate(console_task, "tof_console", 4096, NULL, 4, NULL);
}

void app_main(void) {
    init_console();
    const stc_err_t init_error = stc_bin_init();
    ESP_LOGI(TAG, "stc_bin_init=%s fault_mask=0x%02x",
             stc_error_name(init_error), stc_bin_fault_mask());

    uint32_t last_stream_ms = 0;
    for (;;) {
        stc_drop_t event = {0};
        const int result = stc_bin_poll(&event);
        if (result == 1) {
            ESP_LOGI(TAG, "EVENT DROP bin=%d dist_mm=%u t_ms=%lu",
                     (int)event.bin, event.dist_mm,
                     (unsigned long)event.t_ms);
        }

        const uint32_t now_ms =
            (uint32_t)(esp_timer_get_time() / 1000);
        if (stream_enabled && now_ms - last_stream_ms >= 100) {
            last_stream_ms = now_ms;
            for (unsigned bin = 0; bin < STC_BIN_COUNT; ++bin) {
                stc_bin_debug_info_t info = {0};
                if (stc_bin_debug_info((stc_bin_t)bin, &info) == STC_OK) {
                    printf("tof,%lu,%u,%u,%u\n", (unsigned long)now_ms, bin,
                           info.last_mm, info.fault);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
