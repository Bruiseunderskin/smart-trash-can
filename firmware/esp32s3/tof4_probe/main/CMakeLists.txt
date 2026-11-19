idf_component_register(
    SRCS "tof4_probe.c" "../../../common/stc_tof_drop.c"
    INCLUDE_DIRS "." "../../../common" "../../../../contracts"
    REQUIRES driver esp_driver_gpio esp_driver_i2c esp_timer)

if(STC_TOF_BENCHMARK)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STC_TOF_BENCHMARK=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STC_TOF_BENCHMARK=0)
endif()
