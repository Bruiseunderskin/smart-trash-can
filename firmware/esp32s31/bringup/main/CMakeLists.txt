idf_component_register(
    SRCS
        "bringup_main.c"
        "bringup_common.c"
        "t1_lcd.c"
        "t2_camera.c"
        "t3_mic.c"
        "t4_speaker.c"
        "t5_loopback.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2s esp_driver_gpio esp_lcd esp_timer driver
)
