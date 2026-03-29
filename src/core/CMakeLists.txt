# src/core/CMakeLists.txt
# Foundation types, logging, math helpers, memory utilities, string handling.

file(GLOB_RECURSE CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_library(openswbf_core STATIC ${CORE_SOURCES})

target_include_directories(openswbf_core
    PUBLIC
        "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(openswbf_core
    PUBLIC
        glm::glm
)
