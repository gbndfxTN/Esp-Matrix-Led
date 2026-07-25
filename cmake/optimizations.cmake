include_guard(GLOBAL)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type: Debug or Release" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

function(apply_optimizations_to_target TARGET)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${TARGET} PRIVATE
            -Og
            -ggdb3
            -fno-inline-functions
            -fno-omit-frame-pointer
        )
        target_link_options(${TARGET} PRIVATE
            -Wl,--print-memory-usage
        )
        message(STATUS "${TARGET}: Debug (-Og, full symbols)")

    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(${TARGET} PRIVATE
            -O2
            -g0
            -fomit-frame-pointer
            -DNDEBUG
        )
        target_link_options(${TARGET} PRIVATE
            -Wl,-O2
            -Wl,--gc-sections
        )
        message(STATUS "${TARGET}: Release (-O2, stripped)")

    else()
        message(FATAL_ERROR "Unsupported CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
    endif()
endfunction()

add_compile_options($<$<CONFIG:Debug>:-Og -ggdb3 -fno-inline-functions -fno-omit-frame-pointer>)
add_compile_options($<$<CONFIG:Release>:-O2 -g0 -fomit-frame-pointer -DNDEBUG>)
add_link_options($<$<CONFIG:Debug>:-Wl,--print-memory-usage>)
add_link_options($<$<CONFIG:Release>:-Wl,-O2 -Wl,--gc-sections>)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
