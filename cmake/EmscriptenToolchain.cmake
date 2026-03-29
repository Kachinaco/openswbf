# cmake/EmscriptenToolchain.cmake
# Emscripten-specific compile and link flags for the OpenSWBF WebAssembly build.
#
# This file is included from the root CMakeLists.txt when EMSCRIPTEN is detected.
# It sets global flags; individual targets can add more as needed.

# ---------------------------------------------------------------------------
# SDL2 via Emscripten ports
# ---------------------------------------------------------------------------
set(EMSCRIPTEN_SDL2_FLAGS "-sUSE_SDL=2")

# ---------------------------------------------------------------------------
# WebGL 1 (OpenGL ES 2.0)
# ---------------------------------------------------------------------------
set(EMSCRIPTEN_GL_FLAGS "-sMAX_WEBGL_VERSION=1 -sMIN_WEBGL_VERSION=1 -sFULL_ES2=1")

# ---------------------------------------------------------------------------
# Memory
# ---------------------------------------------------------------------------
set(EMSCRIPTEN_MEMORY_FLAGS
    "-sINITIAL_MEMORY=67108864"    # 64 MiB fixed
)

# ---------------------------------------------------------------------------
# Networking / asset fetch (disabled for now — causes memory init issues)
# ---------------------------------------------------------------------------
set(EMSCRIPTEN_FETCH_FLAGS "")

# ---------------------------------------------------------------------------
# Audio — OpenAL via Emscripten ports
# ---------------------------------------------------------------------------
set(EMSCRIPTEN_AUDIO_FLAGS "-lopenal")

# ---------------------------------------------------------------------------
# Aggregate link flags applied to every Emscripten executable
# ---------------------------------------------------------------------------
set(OPENSWBF_EMSCRIPTEN_LINK_FLAGS
    "${EMSCRIPTEN_SDL2_FLAGS}"
    "${EMSCRIPTEN_GL_FLAGS}"
    "${EMSCRIPTEN_MEMORY_FLAGS}"
    "${EMSCRIPTEN_FETCH_FLAGS}"
    "${EMSCRIPTEN_AUDIO_FLAGS}"
)

# Join the list into a space-separated string for CMAKE_EXE_LINKER_FLAGS
list(JOIN OPENSWBF_EMSCRIPTEN_LINK_FLAGS " " _em_link_flags_str)

# Append to global linker flags so every executable picks them up
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${_em_link_flags_str}")

# SDL2 compile flag is also needed at compile time for headers
add_compile_options(${EMSCRIPTEN_SDL2_FLAGS})

message(STATUS "Emscripten link flags: ${_em_link_flags_str}")
