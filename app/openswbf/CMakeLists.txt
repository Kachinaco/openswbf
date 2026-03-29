# app/openswbf/CMakeLists.txt
# Main executable — links all engine libraries and provides the entry point.

file(GLOB_RECURSE APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(openswbf ${APP_SOURCES})

target_include_directories(openswbf
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(openswbf
    PRIVATE
        openswbf_game
        openswbf_core
        openswbf_assets
        openswbf_renderer
        openswbf_audio
        openswbf_physics
        openswbf_input
        openswbf_scripting
        openswbf_platform
)

# Emscripten: output .html shell + .wasm + .data
if(EMSCRIPTEN)
    set(EMSCRIPTEN_LINK_FLAGS
        "--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html"
        " -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,FS"
        " -sEXPORTED_FUNCTIONS=_main,_load_lvl_data,_load_lvl_file"
        " -sALLOW_MEMORY_GROWTH=1"
        " -sFORCE_FILESYSTEM=1"
    )
    string(CONCAT EMSCRIPTEN_FLAGS_STR ${EMSCRIPTEN_LINK_FLAGS})
    set_target_properties(openswbf PROPERTIES
        SUFFIX ".html"
        LINK_FLAGS "${EMSCRIPTEN_FLAGS_STR}"
    )
endif()

# Native: ensure SDL2 main linkage on platforms that need it
if(NOT EMSCRIPTEN)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(openswbf PRIVATE SDL2::SDL2main)
    endif()
    target_link_libraries(openswbf PRIVATE SDL2::SDL2)
    find_package(OpenGL REQUIRED)
    target_link_libraries(openswbf PRIVATE OpenGL::GL)
endif()

install(TARGETS openswbf RUNTIME DESTINATION bin)
