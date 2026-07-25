set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR xtensa)

if(NOT DEFINED IDF_PATH AND DEFINED ENV{IDF_PATH})
    set(IDF_PATH "$ENV{IDF_PATH}")
endif()

if(NOT IDF_PATH)
    message(FATAL_ERROR "IDF_PATH not set. Source esp-idf/export.sh first.")
endif()

include("${IDF_PATH}/tools/cmake/toolchain-esp32s3.cmake")
