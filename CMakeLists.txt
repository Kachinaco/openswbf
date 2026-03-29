cmake_minimum_required(VERSION 3.20)
project(OpenSWBF
    VERSION 0.1.0
    DESCRIPTION "Open-source reimplementation of Star Wars: Battlefront (2004)"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
option(OPENSWBF_BUILD_TESTS "Build unit and integration tests" OFF)
option(OPENSWBF_BUILD_TOOLS "Build asset viewer and lvl_dump tools" OFF)

# -----------------------------------------------------------------------------
# Compiler warnings (all targets)
# -----------------------------------------------------------------------------
include(cmake/CompilerWarnings.cmake)

# -----------------------------------------------------------------------------
# Emscripten detection
# -----------------------------------------------------------------------------
if(EMSCRIPTEN)
    message(STATUS "Building for Emscripten / WebAssembly")
    include(cmake/EmscriptenToolchain.cmake)
endif()

# -----------------------------------------------------------------------------
# Third-party dependencies via FetchContent
# -----------------------------------------------------------------------------
include(FetchContent)

# GLM — header-only math library
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)

# Lua 5.4 — scripting runtime (Lua 4.0 compat shim added later)
FetchContent_Declare(
    lua
    URL            https://www.lua.org/ftp/lua-5.4.7.tar.gz
    URL_HASH       SHA256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# stb — single-file public-domain libraries (image, vorbis, etc.)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(glm stb)

# ---------------------------------------------------------------------------
# Lua needs a small CMake wrapper because upstream doesn't ship CMakeLists.
# We build it as a static library ourselves.
# ---------------------------------------------------------------------------
FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)
endif()

file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/src/*.c")
# Remove lua.c (standalone interpreter) and luac.c (compiler) — we only want the library.
list(FILTER LUA_SOURCES EXCLUDE REGEX "lua\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX "luac\\.c$")

add_library(lua_static STATIC ${LUA_SOURCES})
target_include_directories(lua_static PUBLIC "${lua_SOURCE_DIR}/src")
# Silence warnings in third-party code
target_compile_options(lua_static PRIVATE -w)
if(NOT WIN32)
    target_compile_definitions(lua_static PRIVATE LUA_USE_POSIX)
endif()
add_library(Lua::Lua ALIAS lua_static)

# ---------------------------------------------------------------------------
# stb interface library (header-only)
# ---------------------------------------------------------------------------
add_library(stb_headers INTERFACE)
target_include_directories(stb_headers INTERFACE "${stb_SOURCE_DIR}")
add_library(STB::STB ALIAS stb_headers)

# -----------------------------------------------------------------------------
# Native-only: find system SDL2 and OpenAL
# -----------------------------------------------------------------------------
if(NOT EMSCRIPTEN)
    find_package(SDL2 REQUIRED)
    find_package(OpenAL REQUIRED)
endif()

# -----------------------------------------------------------------------------
# Source modules
# -----------------------------------------------------------------------------
add_subdirectory(src/core)
add_subdirectory(src/assets)
add_subdirectory(src/renderer)
add_subdirectory(src/audio)
add_subdirectory(src/physics)
add_subdirectory(src/input)
add_subdirectory(src/scripting)
add_subdirectory(src/platform)
add_subdirectory(src/game)

# -----------------------------------------------------------------------------
# Main executable
# -----------------------------------------------------------------------------
add_subdirectory(app/openswbf)

# -----------------------------------------------------------------------------
# Optional: tools
# -----------------------------------------------------------------------------
if(OPENSWBF_BUILD_TOOLS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app/asset_viewer/CMakeLists.txt")
        add_subdirectory(app/asset_viewer)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app/lvl_dump/CMakeLists.txt")
        add_subdirectory(app/lvl_dump)
    endif()
endif()

# -----------------------------------------------------------------------------
# Optional: tests
# -----------------------------------------------------------------------------
if(OPENSWBF_BUILD_TESTS)
    enable_testing()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()
endif()
