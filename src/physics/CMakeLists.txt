# src/physics/CMakeLists.txt
# Collision detection, raycasting, simple rigid-body physics.

file(GLOB_RECURSE PHYSICS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_library(openswbf_physics STATIC ${PHYSICS_SOURCES})

target_include_directories(openswbf_physics
    PUBLIC
        "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(openswbf_physics
    PUBLIC
        openswbf_core
)
