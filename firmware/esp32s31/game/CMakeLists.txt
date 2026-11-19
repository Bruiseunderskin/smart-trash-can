cmake_minimum_required(VERSION 3.16)

option(STC_CAMERA_PROBE "Run the S31 camera-to-model probe instead of the game" OFF)
option(STC_GAME_LIVE_CAMERA "Use the SC101IOT camera for game scans" ON)
option(STC_GAME_DEV_FLOW "Allow model selftest 39/40 and run five-round autopilot" OFF)
option(STC_GAME_IGNORE_MODEL_SELFTEST "Allow game start when the model selftest does not reach 40/40" ON)
option(STC_GAME_TOUCH_BIN "Use four on-screen bin buttons instead of the S3 hand" OFF)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(stc_s31_game)
