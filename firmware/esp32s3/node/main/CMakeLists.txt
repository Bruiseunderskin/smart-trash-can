idf_component_register(
    SRCS "node_main.c" "servo.c" "tof.c" "now_link.c"
         "../../../common/stc_lidmsg.c"
    INCLUDE_DIRS "." "../../../common"
    REQUIRES esp_driver_ledc esp_driver_i2c esp_driver_gpio
             esp_wifi esp_netif esp_event nvs_flash)
