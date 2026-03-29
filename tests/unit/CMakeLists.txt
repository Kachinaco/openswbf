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

# Suppress compiler warnings inside GoogleTest/GoogleMock for our strict warning flags.
foreach(_gt_compile_target gtest gtest_main gmock gmock_main)
    if(TARGET ${_gt_compile_target})
        target_compile_options(${_gt_compile_target} PRIVATE -w)
    endif()
endforeach()

# Mark GoogleTest/GoogleMock include directories as SYSTEM to suppress warnings
# from third-party headers when included by our test code.
foreach(_gt_target gtest gtest_main gmock gmock_main)
    if(TARGET ${_gt_target})
        get_target_property(_gt_inc ${_gt_target} INTERFACE_INCLUDE_DIRECTORIES)
        if(_gt_inc)
            set_target_properties(${_gt_target} PROPERTIES
                INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_gt_inc}"
            )
        endif()
    endif()
endforeach()

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
