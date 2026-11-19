#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_log.h"

#define SERVO_COUNT 4
#define SERVO_FREQ_HZ 50
#define SERVO_PERIOD_US 20000
#define SERVO_MIN_US 1000
#define SERVO_MAX_US 2000
#define SERVO_DUTY_BITS 14
#define SERVO_DUTY_MAX ((1u << SERVO_DUTY_BITS) - 1u)
#define SERVO_MODE LEDC_LOW_SPEED_MODE
#define SERVO_TIMER LEDC_TIMER_0

static const char *TAG = "servo_lid_test";

static const gpio_num_t s_gpio[SERVO_COUNT] = {
    GPIO_NUM_12,
    GPIO_NUM_13,
    GPIO_NUM_14,
    GPIO_NUM_15,
};

static const ledc_channel_t s_channel[SERVO_COUNT] = {
    LEDC_CHANNEL_0,
    LEDC_CHANNEL_1,
    LEDC_CHANNEL_2,
    LEDC_CHANNEL_3,
};

static const char *const s_bin_name[SERVO_COUNT] = {
    "recyclable",
    "food",
    "hazardous",
    "other",
};

/* Calibrated against the installed lid mechanism. Keep each bin independent. */
static const uint32_t s_close_us[SERVO_COUNT] = {1450, 1450, 1450, 1450};
static const uint32_t s_open_us[SERVO_COUNT] = {1575, 1575, 1575, 1575};

static int s_angle[SERVO_COUNT] = {-1, -1, -1, -1};

static esp_err_t set_pulse_us(int bin, uint32_t pulse_us)
{
    if (bin < 0 || bin >= SERVO_COUNT ||
        pulse_us < SERVO_MIN_US || pulse_us > SERVO_MAX_US) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint32_t duty =
        (pulse_us * SERVO_DUTY_MAX + SERVO_PERIOD_US / 2u) / SERVO_PERIOD_US;
    ESP_RETURN_ON_ERROR(
        ledc_set_duty(SERVO_MODE, s_channel[bin], duty),
        TAG, "set duty failed");
    ESP_RETURN_ON_ERROR(
        ledc_update_duty(SERVO_MODE, s_channel[bin]),
        TAG, "update duty failed");
    return ESP_OK;
}

static esp_err_t set_angle(int bin, int angle)
{
    if (angle < 0 || angle > 180) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint32_t pulse_us =
        SERVO_MIN_US +
        ((uint32_t)angle * (SERVO_MAX_US - SERVO_MIN_US)) / 180u;
    const esp_err_t err = set_pulse_us(bin, pulse_us);
    if (err == ESP_OK) {
        s_angle[bin] = angle;
        ESP_LOGI(TAG,
                 "SERVO_MOVE bin=%d name=%s gpio=%d angle=%d pulse_us=%" PRIu32,
                 bin, s_bin_name[bin], (int)s_gpio[bin], angle, pulse_us);
    }
    return err;
}

static bool parse_bin(const char *text, int *bin)
{
    char *end = NULL;
    const long value = strtol(text, &end, 10);
    if (text == end || *end != '\0' || value < 0 || value >= SERVO_COUNT) {
        return false;
    }
    *bin = (int)value;
    return true;
}

static int command_open(int argc, char **argv)
{
    int bin;
    if (argc != 2 || !parse_bin(argv[1], &bin)) {
        printf("usage: open <0..3>\n");
        return 1;
    }

    const esp_err_t err = set_pulse_us(bin, s_open_us[bin]);
    if (err == ESP_OK) {
        s_angle[bin] = -1;
        ESP_LOGI(TAG,
                 "SERVO_OPEN bin=%d name=%s gpio=%d pulse_us=%" PRIu32,
                 bin, s_bin_name[bin], (int)s_gpio[bin], s_open_us[bin]);
    }
    return err == ESP_OK ? 0 : 1;
}

static int command_close(int argc, char **argv)
{
    int bin;
    if (argc != 2 || !parse_bin(argv[1], &bin)) {
        printf("usage: close <0..3>\n");
        return 1;
    }

    const esp_err_t err = set_pulse_us(bin, s_close_us[bin]);
    if (err == ESP_OK) {
        s_angle[bin] = -1;
        ESP_LOGI(TAG,
                 "SERVO_CLOSE bin=%d name=%s gpio=%d pulse_us=%" PRIu32,
                 bin, s_bin_name[bin], (int)s_gpio[bin], s_close_us[bin]);
    }
    return err == ESP_OK ? 0 : 1;
}

static int command_angle(int argc, char **argv)
{
    int bin;
    char *end = NULL;
    const long angle = argc == 3 ? strtol(argv[2], &end, 10) : -1;
    if (argc != 3 || !parse_bin(argv[1], &bin) ||
        argv[2] == end || *end != '\0' || angle < 0 || angle > 180) {
        printf("usage: angle <0..3> <0..180>\n");
        return 1;
    }
    return set_angle(bin, (int)angle) == ESP_OK ? 0 : 1;
}

static int command_pulse(int argc, char **argv)
{
    int bin;
    char *end = NULL;
    const long pulse_us = argc == 3 ? strtol(argv[2], &end, 10) : -1;
    if (argc != 3 || !parse_bin(argv[1], &bin) ||
        argv[2] == end || *end != '\0' ||
        pulse_us < SERVO_MIN_US || pulse_us > SERVO_MAX_US) {
        printf("usage: pulse <0..3> <1000..2000>\n");
        return 1;
    }

    const esp_err_t err = set_pulse_us(bin, (uint32_t)pulse_us);
    if (err == ESP_OK) {
        s_angle[bin] = -1;
        ESP_LOGI(TAG,
                 "SERVO_PULSE bin=%d name=%s gpio=%d pulse_us=%ld",
                 bin, s_bin_name[bin], (int)s_gpio[bin], pulse_us);
    }
    return err == ESP_OK ? 0 : 1;
}

static int command_open_all(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        printf("usage: openall\n");
        return 1;
    }
    for (int bin = 0; bin < SERVO_COUNT; ++bin) {
        const esp_err_t err = set_pulse_us(bin, s_open_us[bin]);
        if (err != ESP_OK) {
            return 1;
        }
        s_angle[bin] = -1;
        ESP_LOGI(TAG,
                 "SERVO_OPEN bin=%d name=%s gpio=%d pulse_us=%" PRIu32,
                 bin, s_bin_name[bin], (int)s_gpio[bin], s_open_us[bin]);
    }
    return 0;
}

static int command_close_all(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        printf("usage: closeall\n");
        return 1;
    }
    for (int bin = 0; bin < SERVO_COUNT; ++bin) {
        const esp_err_t err = set_pulse_us(bin, s_close_us[bin]);
        if (err != ESP_OK) {
            return 1;
        }
        s_angle[bin] = -1;
        ESP_LOGI(TAG,
                 "SERVO_CLOSE bin=%d name=%s gpio=%d pulse_us=%" PRIu32,
                 bin, s_bin_name[bin], (int)s_gpio[bin], s_close_us[bin]);
    }
    return 0;
}

static int command_stop(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        printf("usage: stop\n");
        return 1;
    }
    for (int bin = 0; bin < SERVO_COUNT; ++bin) {
        ESP_RETURN_ON_ERROR(
            ledc_stop(SERVO_MODE, s_channel[bin], 0),
            TAG, "stop channel failed");
        s_angle[bin] = -1;
    }
    ESP_LOGW(TAG, "SERVO_OUTPUTS_STOPPED; servos may not hold position");
    return 0;
}

static int command_status(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        printf("usage: status\n");
        return 1;
    }
    for (int bin = 0; bin < SERVO_COUNT; ++bin) {
        printf("bin=%d gpio=%d name=%s angle=%d close_us=%" PRIu32
               " open_us=%" PRIu32 "\n",
               bin, (int)s_gpio[bin], s_bin_name[bin], s_angle[bin],
               s_close_us[bin], s_open_us[bin]);
    }
    return 0;
}

static esp_err_t init_ledc(void)
{
    const ledc_timer_config_t timer_config = {
        .speed_mode = SERVO_MODE,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .timer_num = SERVO_TIMER,
        .freq_hz = SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_config), TAG, "timer config failed");

    for (int bin = 0; bin < SERVO_COUNT; ++bin) {
        const ledc_channel_config_t channel_config = {
            .gpio_num = s_gpio[bin],
            .speed_mode = SERVO_MODE,
            .channel = s_channel[bin],
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = SERVO_TIMER,
            .duty = 0,
            .hpoint = 0,
        };
        ESP_RETURN_ON_ERROR(
            ledc_channel_config(&channel_config), TAG, "channel config failed");
    }
    return ESP_OK;
}

static void register_commands(void)
{
    const esp_console_cmd_t commands[] = {
        {.command = "open", .help = "Open one lid", .hint = "<0..3>", .func = command_open},
        {.command = "close", .help = "Close one lid", .hint = "<0..3>", .func = command_close},
        {.command = "angle", .help = "Set one lid angle", .hint = "<0..3> <0..180>", .func = command_angle},
        {.command = "pulse", .help = "Set one pulse width", .hint = "<0..3> <1000..2000>", .func = command_pulse},
        {.command = "openall", .help = "Open all four lids", .func = command_open_all},
        {.command = "closeall", .help = "Close all four lids", .func = command_close_all},
        {.command = "stop", .help = "Stop all PWM outputs", .func = command_stop},
        {.command = "status", .help = "Show current bin angles", .func = command_status},
    };

    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&commands[i]));
    }
    ESP_ERROR_CHECK(esp_console_register_help_command());
}

void app_main(void)
{
    ESP_LOGI(TAG, "VIRTUAL TEST pins: bin0=GPIO12 bin1=GPIO13 bin2=GPIO14 bin3=GPIO15");
    ESP_LOGI(TAG,
             "SERVO_CONFIG freq_hz=%d pulse_us=%d..%d close_us=%" PRIu32
             " open_us=%" PRIu32,
             SERVO_FREQ_HZ, SERVO_MIN_US, SERVO_MAX_US,
             s_close_us[0], s_open_us[0]);
    ESP_ERROR_CHECK(init_ledc());
    ESP_LOGW(TAG,
             "SERVO_OUTPUTS_IDLE at boot; no lid moves until a command is entered");

    register_commands();
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "lid>";
    repl_config.max_cmdline_length = 128;
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    ESP_LOGI(TAG, "SERVO_LID_TEST=READY");
    ESP_LOGI(TAG, "Commands: open 0..3 | close 0..3 | openall | closeall | angle | pulse | stop | status | help");
}
