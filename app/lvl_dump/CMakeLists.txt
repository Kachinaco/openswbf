# app/lvl_dump/CMakeLists.txt
# Diagnostic tool — reads a .lvl file and prints its chunk structure.
# Only built on native platforms (not Emscripten).

if(EMSCRIPTEN)
    return()
endif()

add_executable(lvl_dump main.cpp)

target_include_directories(lvl_dump
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(lvl_dump
    PRIVATE
        openswbf_core
        openswbf_assets
)

install(TARGETS lvl_dump RUNTIME DESTINATION bin)
