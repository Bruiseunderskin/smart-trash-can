cmake_minimum_required(VERSION 3.16)
project(stc_common_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")

set(STC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
get_filename_component(STC_ROOT "${STC_ROOT}" ABSOLUTE)
set(COMMON "${STC_ROOT}/firmware/common")

enable_testing()

set(SRC_VISION_MAP ${COMMON}/stc_vision_map.c)
set(SRC_PREPROCESS ${COMMON}/stc_preprocess.c)
set(SRC_FRAME ${COMMON}/stc_frame_replay.c ${COMMON}/stc_frame_esp.c)

add_library(stc_common STATIC
    ${SRC_VISION_MAP} ${SRC_PREPROCESS} ${SRC_FRAME} ${COMMON}/stc_game.c ${COMMON}/stc_mode.c
    ${COMMON}/stc_lid_loopback.c ${COMMON}/stc_lidmsg.c)
target_include_directories(stc_common PUBLIC
    "${COMMON}" "${STC_ROOT}/contracts")
target_compile_definitions(stc_common PRIVATE STC_HOST_TEST=1)

function(stc_test name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE stc_common)
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

stc_test(test_contract_ui)
stc_test(test_classes_order)
stc_test(test_label_map)
stc_test(test_preprocess_golden)
stc_test(test_mode_gate)
stc_test(test_game)
stc_test(test_lid)
stc_test(test_e2e)

target_compile_definitions(test_classes_order PRIVATE
    CLASSES_JSON_PATH="${STC_ROOT}/contracts/model/classes.json"
    LABELS_PATH="${STC_ROOT}/models/wordless-padded/labels.txt")
