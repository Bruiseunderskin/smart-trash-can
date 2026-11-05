#include "stc_bin.h"

#include <stdio.h>
#include <string.h>

#include "calibration.h"
#include "drop_detector.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "stc_bin_debug.h"
#include "tof_driver.h"

#define STC_BIN_READ_TIMEOUT_MS 35
#define STC_BIN_CALIBRATION_SAMPLES 5

static const char *TAG = "stc_bin";
static SemaphoreHandle_t state_mutex;
static bool initialized;
static uint8_t fault_mask;
static int8_t offsets[4];
static uint16_t xtalk[4];
static uint16_t last_distance[4];
static stc_drop_detector_t detector;
static bool monitor_active;
static uint32_t monitor_started_ms;
static uint32_t monitor_timeout_ms;

static stc_err_t map_error(esp_err_t error) {
    switch (error) {
    case ESP_OK:
        return STC_OK;
    case ESP_ERR_TIMEOUT:
        return STC_ERR_TIMEOUT;
    case ESP_ERR_NOT_FOUND:
    case ESP_ERR_INVALID_STATE:
        return STC_ERR_NOT_READY;
    case ESP_ERR_INVALID_ARG:
        return STC_ERR_CONTRACT;
    default:
        return STC_ERR_IO;
    }
}

static bool lock_state(void) {
    return state_mutex &&
           xSemaphoreTake(state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE;
}

static void unlock_state(void) {
    xSemaphoreGive(state_mutex);
}

static esp_err_t ensure_nvs(void) {
    esp_err_t error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES ||
        error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "erase NVS");
        error = nvs_flash_init();
    }
    return error;
}

static void calibration_key(char *key, size_t key_size, size_t bin,
                            const char *suffix) {
    snprintf(key, key_size, "tof_ch%u_%s", (unsigned)bin, suffix);
}

static esp_err_t save_calibration(size_t bin) {
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open("stc_bin", NVS_READWRITE, &handle), TAG,
                        "open calibration NVS");
    char key[16];
    calibration_key(key, sizeof(key), bin, "offset");
    esp_err_t error = nvs_set_i8(handle, key, offsets[bin]);
    calibration_key(key, sizeof(key), bin, "xtalk");
    if (error == ESP_OK) {
        error = nvs_set_u16(handle, key, xtalk[bin]);
    }
    if (error == ESP_OK) {
        error = nvs_commit(handle);
    }
    nvs_close(handle);
    return error;
}

static void load_calibration(size_t bin, int8_t factory_offset,
                             uint16_t factory_xtalk) {
    offsets[bin] = factory_offset;
    xtalk[bin] = factory_xtalk;

    nvs_handle_t handle;
    if (nvs_open("stc_bin", NVS_READONLY, &handle) != ESP_OK) {
        return;
    }

    char key[16];
    calibration_key(key, sizeof(key), bin, "offset");
    int8_t offset = factory_offset;
    if (nvs_get_i8(handle, key, &offset) == ESP_OK) {
        offsets[bin] = offset;
    }
    calibration_key(key, sizeof(key), bin, "xtalk");
    uint16_t rate = factory_xtalk;
    if (nvs_get_u16(handle, key, &rate) == ESP_OK) {
        xtalk[bin] = rate;
    }
    nvs_close(handle);
}

static esp_err_t init_sensor(size_t bin) {
    int8_t factory_offset = 0;
    uint16_t factory_xtalk = 0;
    ESP_RETURN_ON_ERROR(stc_tof_probe((uint8_t)bin), TAG,
                        "probe bin %u", (unsigned)bin);
    ESP_RETURN_ON_ERROR(
        stc_tof_sensor_init((uint8_t)bin, &factory_offset, &factory_xtalk), TAG,
        "initialize bin %u", (unsigned)bin);
    load_calibration(bin, factory_offset, factory_xtalk);
    ESP_RETURN_ON_ERROR(stc_tof_set_offset((uint8_t)bin, offsets[bin]), TAG,
                        "apply bin %u offset", (unsigned)bin);
    ESP_RETURN_ON_ERROR(stc_tof_set_xtalk((uint8_t)bin, xtalk[bin]), TAG,
                        "apply bin %u xtalk", (unsigned)bin);
    ESP_RETURN_ON_ERROR(save_calibration(bin), TAG,
                        "save bin %u calibration", (unsigned)bin);
    return stc_tof_start_continuous((uint8_t)bin);
}

static esp_err_t read_sensor(size_t bin) {
    uint16_t distance = 0;
    esp_err_t error =
        stc_tof_read((uint8_t)bin, &distance, STC_BIN_READ_TIMEOUT_MS);
    if (error == ESP_OK && distance == 0) {
        error = ESP_ERR_INVALID_RESPONSE;
    }
    if (error == ESP_OK) {
        last_distance[bin] = distance;
        fault_mask &= (uint8_t)~(1U << bin);
    } else if (error != ESP_ERR_INVALID_RESPONSE &&
               error != ESP_ERR_TIMEOUT) {
        const esp_err_t probe_error = stc_tof_probe((uint8_t)bin);
        if (probe_error != ESP_OK) {
            fault_mask |= (uint8_t)(1U << bin);
            error = probe_error;
        }
    }
    return error;
}

stc_err_t stc_bin_init(void) {
    if (!state_mutex) {
        state_mutex = xSemaphoreCreateMutex();
        if (!state_mutex) {
            return STC_ERR_IO;
        }
    }
    if (!lock_state()) {
        return STC_ERR_TIMEOUT;
    }

    initialized = false;
    fault_mask = 0;
    memset(last_distance, 0, sizeof(last_distance));
    stc_drop_detector_reset(&detector);
    monitor_active = false;

    esp_err_t error = ensure_nvs();
    if (error == ESP_OK) {
        error = stc_tof_bus_init();
    }
    if (error != ESP_OK) {
        unlock_state();
        return map_error(error);
    }

    initialized = true;
    for (size_t bin = 0; bin < 4; ++bin) {
        error = init_sensor(bin);
        if (error != ESP_OK) {
            fault_mask |= (uint8_t)(1U << bin);
            ESP_LOGE(TAG, "bin %u unavailable: %s", (unsigned)bin,
                     esp_err_to_name(error));
        }
    }
    ESP_LOGI(TAG, "initialized fault_mask=0x%02x", fault_mask);
    unlock_state();
    return STC_OK;
}

stc_err_t stc_bin_selftest(void) {
    if (!lock_state()) {
        return STC_ERR_TIMEOUT;
    }
    if (!initialized) {
        unlock_state();
        return STC_ERR_NOT_READY;
    }

    esp_err_t first_error = ESP_OK;
    for (size_t bin = 0; bin < 4; ++bin) {
        esp_err_t error = stc_tof_probe((uint8_t)bin);
        if (error != ESP_OK) {
            error = init_sensor(bin);
        }
        if (error == ESP_OK) {
            fault_mask &= (uint8_t)~(1U << bin);
            (void)read_sensor(bin);
        } else {
            fault_mask |= (uint8_t)(1U << bin);
            if (first_error == ESP_OK) {
                first_error = error;
            }
        }
    }
    unlock_state();
    return map_error(first_error);
}

uint8_t stc_bin_fault_mask(void) {
    if (!lock_state()) {
        return 0x0F;
    }
    const uint8_t result = fault_mask;
    unlock_state();
    return result;
}

stc_err_t stc_bin_open_all(void) {
    return STC_ERR_UNSUPPORTED;
}

stc_err_t stc_bin_close_all(void) {
    return STC_ERR_UNSUPPORTED;
}

stc_err_t stc_bin_start_monitor(uint16_t timeout_ms) {
    if (!lock_state()) {
        return STC_ERR_TIMEOUT;
    }
    if (!initialized) {
        unlock_state();
        return STC_ERR_NOT_READY;
    }
    monitor_active = true;
    monitor_started_ms = (uint32_t)(esp_timer_get_time() / 1000);
    monitor_timeout_ms = timeout_ms;
    stc_drop_detector_set_monitor(&detector, true, monitor_started_ms);
    unlock_state();
    return STC_OK;
}

stc_err_t stc_bin_stop_monitor(void) {
    if (!lock_state()) {
        return STC_ERR_TIMEOUT;
    }
    monitor_active = false;
    stc_drop_detector_set_monitor(
        &detector, false, (uint32_t)(esp_timer_get_time() / 1000));
    unlock_state();
    return STC_OK;
}

int stc_bin_poll(stc_drop_t *event) {
    if (!event || !lock_state()) {
        return STC_ERR_TIMEOUT;
    }
    if (!initialized) {
        unlock_state();
        return STC_ERR_NOT_READY;
    }

    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if (monitor_active && monitor_timeout_ms > 0 &&
        now_ms - monitor_started_ms >= monitor_timeout_ms) {
        monitor_active = false;
        stc_drop_detector_set_monitor(&detector, false, now_ms);
    }

    uint16_t distances[4] = {0};
    bool valid[4] = {false};
    for (size_t bin = 0; bin < 4; ++bin) {
        if (read_sensor(bin) == ESP_OK) {
            distances[bin] = last_distance[bin];
            valid[bin] = true;
        }
    }

    const int result =
        stc_drop_detector_feed(&detector, distances, valid, now_ms, event);
    if (result == 1) {
        monitor_active = false;
    }
    unlock_state();
    return result;
}

uint16_t stc_bin_distance(stc_bin_t bin) {
    if ((size_t)bin >= 4 || !lock_state()) {
        return 0;
    }
    if (!initialized || read_sensor((size_t)bin) != ESP_OK) {
        unlock_state();
        return 0;
    }
    const uint16_t distance = last_distance[bin];
    unlock_state();
    return distance;
}

stc_err_t stc_bin_debug_info(stc_bin_t bin, stc_bin_debug_info_t *info) {
    if (!info || (size_t)bin >= 4 || !lock_state()) {
        return STC_ERR_CONTRACT;
    }
    info->offset = offsets[bin];
    info->xtalk = xtalk[bin];
    info->last_mm = last_distance[bin];
    info->fault = (fault_mask & (uint8_t)(1U << bin)) != 0;
    unlock_state();
    return STC_OK;
}

stc_err_t stc_bin_calibrate_offset(stc_bin_t bin, uint16_t target_mm) {
    if ((size_t)bin >= 4 || target_mm == 0 || target_mm > 200 ||
        !lock_state()) {
        return STC_ERR_CONTRACT;
    }
    if (!initialized) {
        unlock_state();
        return STC_ERR_NOT_READY;
    }

    uint32_t total_mm = 0;
    uint8_t samples = 0;
    for (uint8_t i = 0; i < STC_BIN_CALIBRATION_SAMPLES; ++i) {
        uint16_t distance = 0;
        if (stc_tof_read((uint8_t)bin, &distance,
                         STC_BIN_READ_TIMEOUT_MS) == ESP_OK &&
            distance > 0) {
            total_mm += distance;
            ++samples;
        }
    }
    if (samples < 3) {
        fault_mask |= (uint8_t)(1U << bin);
        unlock_state();
        return STC_ERR_IO;
    }

    const uint16_t measured_mm =
        (uint16_t)((total_mm + samples / 2U) / samples);
    int8_t corrected = offsets[bin];
    if (!stc_offset_correct(offsets[bin], measured_mm, target_mm,
                            &corrected)) {
        unlock_state();
        return STC_ERR_CONTRACT;
    }
    esp_err_t error = stc_tof_set_offset((uint8_t)bin, corrected);
    int8_t readback = 0;
    if (error == ESP_OK) {
        error = stc_tof_get_offset((uint8_t)bin, &readback);
    }
    if (error != ESP_OK || readback != corrected) {
        unlock_state();
        return STC_ERR_IO;
    }
    offsets[bin] = corrected;
    error = save_calibration((size_t)bin);
    if (error != ESP_OK) {
        unlock_state();
        return STC_ERR_IO;
    }
    ESP_LOGI(TAG, "bin=%u measured=%u target=%u offset=%d", (unsigned)bin,
             measured_mm, target_mm, corrected);
    unlock_state();
    return STC_OK;
}
