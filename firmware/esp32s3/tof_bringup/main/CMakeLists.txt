set(STC_CONTRACTS_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../contracts")

idf_component_register(
    SRCS
        "main.c"
        "calibration.c"
        "drop_detector.c"
        "stc_bin.c"
        "tof_driver.c"
    INCLUDE_DIRS
        "."
        "${STC_CONTRACTS_DIR}"
    REQUIRES
        esp_driver_i2c
        esp_driver_uart
        esp_timer
        nvs_flash
)
