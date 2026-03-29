# src/scripting/CMakeLists.txt
# Lua scripting runtime — executes SWBF mission scripts.
# Uses Lua 5.4 with a Lua 4.0 compatibility shim (added later).

file(GLOB_RECURSE SCRIPTING_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_library(openswbf_scripting STATIC ${SCRIPTING_SOURCES})

target_include_directories(openswbf_scripting
    PUBLIC
        "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(openswbf_scripting
    PUBLIC
        openswbf_core
        Lua::Lua
)
