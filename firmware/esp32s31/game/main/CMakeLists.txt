set(STC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../..")
get_filename_component(STC_ROOT "${STC_ROOT}" ABSOLUTE)
set(COMMON "${STC_ROOT}/firmware/common")
set(STC_UI_ROOT "${STC_ROOT}/ui/lvglpro")
set(MODEL "${STC_ROOT}/models/wordless-padded/wordless_s31.espdl")
set(REPLAY_PACK "${CMAKE_CURRENT_BINARY_DIR}/game_replay_pack.bin")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${STC_ROOT}/tools/game_replay.py"
            --scene-dir
            "${STC_ROOT}/data/mixed-card-object/wordless-v1/test-scenes"
            --output "${REPLAY_PACK}"
            --seed 20261111
    RESULT_VARIABLE REPLAY_RESULT
    OUTPUT_QUIET
    ERROR_VARIABLE REPLAY_ERROR)
if(NOT REPLAY_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not build deterministic S31 replay pack: ${REPLAY_ERROR}")
endif()

file(SHA256 "${MODEL}" MODEL_SHA)
if(NOT MODEL_SHA STREQUAL
   "5fb3ac6e7b1dcbe7d3ecb9e0db26f286bfebeeb4608fa40bdb8665165fdcbaf0")
    message(FATAL_ERROR "wordless_s31.espdl SHA256 mismatch: ${MODEL_SHA}")
endif()
file(SHA256 "${REPLAY_PACK}" REPLAY_SHA)
if(NOT REPLAY_SHA STREQUAL
   "287c8106c4be0569d15c06c1498d69f562af216030e2ccfe165545c9dd62722e")
    message(FATAL_ERROR "game_replay_pack.bin SHA256 mismatch: ${REPLAY_SHA}")
endif()
# The SHA-256 gate above identifies the exact artifact. This CRC was computed
# from that artifact and is also checked at runtime by replay_classifier.cpp.
set(MODEL_CRC "0x1c55ab7a")

file(GLOB STC_UI_SOURCES
     "${STC_UI_ROOT}/src/stc_ui.c"
     "${STC_UI_ROOT}/src/stc_chat_ui.c"
     "${STC_UI_ROOT}/generated/stc_lp_*.c"
     "${STC_UI_ROOT}/generated/stc_lvglpro_assets.c")

idf_component_register(
    SRCS
        "app_main.cpp"
        "replay_classifier.cpp"
        "camera_probe.cpp"
        "camera_capture.cpp"
        "local_voice.c"
        "ui_port.c"
        "ui_bridge.c"
        "espnow_controller.c"
        "${STC_ROOT}/app/stc_app.c"
        "${STC_ROOT}/app/stc_game_controller.c"
        "${STC_ROOT}/app/stc_shell_mode.c"
        "${COMMON}/stc_game.c"
        "${COMMON}/stc_game_dev_flow.c"
        "${COMMON}/stc_game_touch_profile.c"
        "${COMMON}/stc_hand_msg.c"
        "${COMMON}/stc_hand_link.c"
        "${COMMON}/stc_lidmsg.c"
        "${COMMON}/stc_replay_pack.c"
        "${COMMON}/stc_preprocess.c"
        "${COMMON}/stc_vision_aggregate.c"
        "${COMMON}/stc_vision_map.c"
        ${STC_UI_SOURCES}
    INCLUDE_DIRS
        "."
        "${STC_ROOT}/app"
        "${STC_ROOT}/contracts"
        "${COMMON}"
        "${STC_UI_ROOT}/include"
        "${STC_UI_ROOT}/src"
        "${STC_UI_ROOT}/generated"
    REQUIRES
        esp32_s31_korvo_1
        esp-dl
        esp_wifi
        esp_event
        esp_netif
        esp_timer
        esp_codec_dev
        lvgl
        esp_lvgl_port
        driver
        nvs_flash)

idf_component_get_property(espdl_dir espressif__esp-dl COMPONENT_DIR)
set(cmake_dir "${espdl_dir}/fbs_loader/cmake")
include("${espdl_dir}/fbs_loader/cmake/utilities.cmake")
target_add_aligned_binary_data(
    ${COMPONENT_LIB} "${MODEL}" BINARY RENAME_TO wordless_s31_espdl)
target_add_aligned_binary_data(${COMPONENT_LIB} "${REPLAY_PACK}" BINARY)
foreach(clip open correct wrong timeout retry summary cheer_correct_a cheer_correct_b encourage_wrong_a encourage_wrong_b summary_tip)
    target_add_aligned_binary_data(
        ${COMPONENT_LIB}
        "${STC_ROOT}/firmware/assets/voice/stc_${clip}.pcm"
        BINARY)
endforeach()

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    STC_GAME_MODEL_CRC=${MODEL_CRC}
    STC_GAME_STAGE1=1)

if(STC_CAMERA_PROBE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STC_CAMERA_PROBE=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STC_CAMERA_PROBE=0)
endif()

if(STC_GAME_LIVE_CAMERA)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STC_GAME_LIVE_CAMERA=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STC_GAME_LIVE_CAMERA=0)
endif()

if(STC_GAME_DEV_FLOW)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STC_GAME_DEV_FLOW=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STC_GAME_DEV_FLOW=0)
endif()

if(STC_GAME_IGNORE_MODEL_SELFTEST)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STC_GAME_IGNORE_MODEL_SELFTEST=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STC_GAME_IGNORE_MODEL_SELFTEST=0)
endif()

if(STC_GAME_TOUCH_BIN)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STC_GAME_TOUCH_BIN=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STC_GAME_TOUCH_BIN=0)
endif()
