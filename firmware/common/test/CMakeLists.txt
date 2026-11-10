cmake_minimum_required(VERSION 3.16)
project(stc_common_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")

set(STC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
get_filename_component(STC_ROOT "${STC_ROOT}" ABSOLUTE)

enable_testing()

add_executable(test_contract_ui test_contract_ui.c)
target_include_directories(test_contract_ui PRIVATE "${STC_ROOT}/contracts")
add_test(NAME test_contract_ui COMMAND test_contract_ui)

add_executable(test_classes_order test_classes_order.c)
target_include_directories(test_classes_order PRIVATE "${STC_ROOT}/contracts")
target_compile_definitions(test_classes_order PRIVATE
    CLASSES_JSON_PATH="${STC_ROOT}/contracts/model/classes.json"
    LABELS_PATH="${STC_ROOT}/models/wordless-padded/labels.txt")
add_test(NAME test_classes_order COMMAND test_classes_order)
