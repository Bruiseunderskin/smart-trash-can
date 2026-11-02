set(model_file "${CMAKE_CURRENT_LIST_DIR}/../../../runs/issue12/model/waste4_grouped_int8.espdl")
set(case_dir "${CMAKE_CURRENT_LIST_DIR}/../on_device_cases")
if(NOT EXISTS "${model_file}")
    message(FATAL_ERROR "Missing model artifact: ${model_file}")
endif()
if(NOT EXISTS "${case_dir}/cases.h")
    message(FATAL_ERROR "Missing generated on-device cases: ${case_dir}/cases.h")
endif()

idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "." "${case_dir}"
    REQUIRES esp-dl esp_timer
)

idf_component_get_property(espdl_dir espressif__esp-dl COMPONENT_DIR)
set(cmake_dir "${espdl_dir}/fbs_loader/cmake")

include("${cmake_dir}/utilities.cmake")
target_add_aligned_binary_data(${COMPONENT_LIB} "${model_file}" BINARY)

file(GLOB case_files "${case_dir}/case_*.jpg")
list(SORT case_files)
foreach(case_file IN LISTS case_files)
    target_add_aligned_binary_data(${COMPONENT_LIB} "${case_file}" BINARY)
endforeach()
file(GLOB case_rgb_files "${case_dir}/case_*.rgb")
list(SORT case_rgb_files)
foreach(case_file IN LISTS case_rgb_files)
    target_add_aligned_binary_data(${COMPONENT_LIB} "${case_file}" BINARY)
endforeach()
