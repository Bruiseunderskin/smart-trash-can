idf_component_register(
    SRCS "cam_test.c"
    INCLUDE_DIRS "."
    REQUIRES esp32_s31_korvo esp_timer esp_new_jpeg esp_driver_uart
)
