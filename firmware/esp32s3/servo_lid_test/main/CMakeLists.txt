idf_component_register(
    SRCS "servo_lid_test.c"
    INCLUDE_DIRS "."
    REQUIRES console driver esp_driver_gpio esp_driver_ledc)
