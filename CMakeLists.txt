cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/optimizations.cmake")

set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/Drivers
    ${CMAKE_CURRENT_LIST_DIR}/Interpreter/core0_wifi_bt
    ${CMAKE_CURRENT_LIST_DIR}/Interpreter/core1_led_matrix
    ${CMAKE_CURRENT_LIST_DIR}/Application/core0_wifi_bt
    ${CMAKE_CURRENT_LIST_DIR}/Application/core1_led_matrix
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_matrix_led)
