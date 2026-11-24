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
    ${SRC_VISION_MAP} ${SRC_PREPROCESS} ${SRC_FRAME} ${COMMON}/stc_game.c
    ${COMMON}/stc_game_dev_flow.c ${COMMON}/stc_game_touch_profile.c
    ${COMMON}/stc_mode.c
    ${COMMON}/stc_lid_loopback.c ${COMMON}/stc_lidmsg.c ${COMMON}/stc_lid_link.c
    ${COMMON}/stc_hand_msg.c ${COMMON}/stc_hand_link.c
    ${COMMON}/stc_replay_pack.c ${COMMON}/stc_vision_aggregate.c
    ${COMMON}/stc_tof_drop.c
    ${COMMON}/stc_hand_round.c
    ${COMMON}/stc_virtual_hand.c
    ${STC_ROOT}/app/stc_app.c ${STC_ROOT}/app/stc_game_controller.c
    ${STC_ROOT}/app/stc_shell_mode.c
    ${STC_ROOT}/app/stc_mode_request.c)
target_include_directories(stc_common PUBLIC
    "${COMMON}" "${STC_ROOT}/contracts" "${STC_ROOT}/app")
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
stc_test(test_preview_preprocess)
stc_test(test_rgb565)
stc_test(test_mode_gate)
stc_test(test_shell_mode)
stc_test(test_mode_request)
stc_test(test_game)
stc_test(test_game_spec)
stc_test(test_lid)
stc_test(test_e2e)
stc_test(test_app_wiring)
stc_test(test_lid_retry)
stc_test(test_node_offline)
stc_test(test_hand_protocol)
stc_test(test_hand_link)
stc_test(test_game_controller)
stc_test(test_game_dev_flow)
stc_test(test_game_touch_profile)
stc_test(test_replay_pack)
stc_test(test_vision_aggregate)
stc_test(test_tof_drop)
stc_test(test_hand_round)
stc_test(test_virtual_hand)

target_compile_definitions(test_classes_order PRIVATE
    CLASSES_JSON_PATH="${STC_ROOT}/contracts/model/classes.json"
    LABELS_PATH="${STC_ROOT}/models/wordless-padded/labels.txt")
