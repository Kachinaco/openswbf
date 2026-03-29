# src/game/CMakeLists.txt
# High-level game logic: world, entities, components, game modes, AI.

file(GLOB_RECURSE GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_library(openswbf_game STATIC ${GAME_SOURCES})

target_include_directories(openswbf_game
    PUBLIC
        "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(openswbf_game
    PUBLIC
        openswbf_core
        openswbf_assets
        openswbf_renderer
        openswbf_audio
        openswbf_physics
        openswbf_input
        openswbf_scripting
        openswbf_platform
)
