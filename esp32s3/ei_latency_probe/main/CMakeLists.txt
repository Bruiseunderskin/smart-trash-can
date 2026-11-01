idf_component_register(
    SRCS "probe.cc"
    INCLUDE_DIRS "."
    REQUIRES esp-tflite-micro esp_hw_support esp_timer
    EMBED_FILES "models/waste4_int8.tflite"
)

# esp-tflite-micro 1.4.1 enables ESP-NN unconditionally in its CMake file.
# Apply -U after that definition for the reference-kernel A/B build.
idf_component_get_property(tflite_micro_lib espressif__esp-tflite-micro COMPONENT_LIB)
if(NOT tflite_micro_lib)
    message(FATAL_ERROR "Could not resolve the esp-tflite-micro component library")
endif()

if(NOT CONFIG_TFLITE_PROBE_USE_ESP_NN)
    target_compile_options(${tflite_micro_lib} PRIVATE -UESP_NN)
endif()
