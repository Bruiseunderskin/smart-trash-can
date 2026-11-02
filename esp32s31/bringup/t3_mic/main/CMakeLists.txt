idf_component_register(
    SRCS "mic_test.c"
    INCLUDE_DIRS "."
    REQUIRES esp32_s31_korvo s31_bringup_audio esp_timer
)
