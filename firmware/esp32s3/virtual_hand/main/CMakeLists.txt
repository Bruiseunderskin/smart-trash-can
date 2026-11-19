set(STC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../..")
get_filename_component(STC_ROOT "${STC_ROOT}" ABSOLUTE)
set(COMMON "${STC_ROOT}/firmware/common")

idf_component_register(
    SRCS
        "app_main.c"
        "${COMMON}/stc_hand_msg.c"
        "${COMMON}/stc_virtual_hand.c"
        "${COMMON}/stc_lidmsg.c"
    INCLUDE_DIRS "." "${COMMON}" "${STC_ROOT}/contracts"
    REQUIRES esp_wifi esp_event esp_netif esp_timer nvs_flash driver
             esp_driver_usb_serial_jtag)
