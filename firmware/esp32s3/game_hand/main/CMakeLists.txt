set(STC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../..")
get_filename_component(STC_ROOT "${STC_ROOT}" ABSOLUTE)
set(COMMON "${STC_ROOT}/firmware/common")

idf_component_register(
    SRCS
        "app_main.c"
        "servo_lids.c"
        "tof4.c"
        "${COMMON}/stc_tof_drop.c"
        "${COMMON}/stc_hand_round.c"
        "${COMMON}/stc_hand_msg.c"
        "${COMMON}/stc_lidmsg.c"
    INCLUDE_DIRS "." "${COMMON}" "${STC_ROOT}/contracts"
    REQUIRES console driver esp_driver_gpio esp_driver_i2c esp_driver_ledc
             esp_timer esp_wifi esp_event esp_netif nvs_flash)
