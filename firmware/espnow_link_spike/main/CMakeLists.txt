idf_component_register(
    SRCS "espnow_link_spike.c" "../../common/stc_lidmsg.c"
    INCLUDE_DIRS "." "../../common"
    REQUIRES esp_wifi esp_event esp_netif esp_timer
)
