# src/input/CMakeLists.txt
# Input handling: keyboard, mouse, gamepad via SDL2 events.

file(GLOB_RECURSE INPUT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_library(openswbf_input STATIC ${INPUT_SOURCES})

target_include_directories(openswbf_input
    PUBLIC
        "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(openswbf_input
    PUBLIC
        openswbf_core
)

if(NOT EMSCRIPTEN)
    target_link_libraries(openswbf_input PUBLIC SDL2::SDL2)
endif()
