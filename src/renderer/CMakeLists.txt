# src/renderer/CMakeLists.txt
# OpenGL ES 3.0 / WebGL 2 renderer, mesh drawing, terrain, sky, HUD.

file(GLOB_RECURSE RENDERER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_library(openswbf_renderer STATIC ${RENDERER_SOURCES})

target_include_directories(openswbf_renderer
    PUBLIC
        "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(openswbf_renderer
    PUBLIC
        openswbf_core
        openswbf_assets
)

if(NOT EMSCRIPTEN)
    find_package(GLEW REQUIRED)
    target_link_libraries(openswbf_renderer PUBLIC SDL2::SDL2 GLEW::GLEW)
endif()
