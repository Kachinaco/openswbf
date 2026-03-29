# src/assets/CMakeLists.txt
# SWBF asset loading: UCFB chunks, .lvl containers, .odf object definitions.

file(GLOB_RECURSE ASSETS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_library(openswbf_assets STATIC ${ASSETS_SOURCES})

target_include_directories(openswbf_assets
    PUBLIC
        "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(openswbf_assets
    PUBLIC
        openswbf_core
        STB::STB
)
