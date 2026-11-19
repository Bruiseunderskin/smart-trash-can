cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(stc_tof4_probe)
option(STC_TOF_BENCHMARK "Run timing benchmark instead of drop detection" OFF)
