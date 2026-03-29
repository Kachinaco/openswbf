# tests/unit/CMakeLists.txt
# Unit tests for OpenSWBF core, assets, and renderer (camera math).

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
# Prevent GoogleTest from overriding our compiler/linker settings on Windows.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# Silence warnings in third-party test framework code.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Suppress compiler warnings inside GoogleTest headers for our strict warning flags.
target_compile_options(gtest PRIVATE -w)
target_compile_options(gtest_main PRIVATE -w)

# ---------------------------------------------------------------------------
# Test executable
# ---------------------------------------------------------------------------
add_executable(openswbf_tests
    test_crc.cpp
    test_chunk_reader.cpp
    test_odf_parser.cpp
    test_camera.cpp
)

target_link_libraries(openswbf_tests
    PRIVATE
        openswbf_core
        openswbf_assets
        gtest_main
)

# Camera tests: compile camera.cpp directly to avoid pulling in the full
# renderer library (which requires OpenGL / SDL2 for the GL backend files).
target_sources(openswbf_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/src/renderer/camera.cpp"
)

target_include_directories(openswbf_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

# ---------------------------------------------------------------------------
# Register tests with CTest
# ---------------------------------------------------------------------------
include(GoogleTest)
gtest_discover_tests(openswbf_tests)
