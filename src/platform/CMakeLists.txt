# src/platform/CMakeLists.txt
# Platform abstraction: window creation, main loop, file I/O, timers.

set(PLATFORM_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/platform.h"
)

if(EMSCRIPTEN)
    set(PLATFORM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/platform_web.cpp")
else()
    set(PLATFORM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/platform_native.cpp")
endif()

add_library(openswbf_platform STATIC ${PLATFORM_HEADERS} ${PLATFORM_SOURCES})

target_include_directories(openswbf_platform
    PUBLIC
        "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(openswbf_platform
    PUBLIC
        openswbf_core
)

if(NOT EMSCRIPTEN)
    target_link_libraries(openswbf_platform PUBLIC SDL2::SDL2)
endif()
