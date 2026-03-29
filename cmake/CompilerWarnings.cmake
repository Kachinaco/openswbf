# cmake/CompilerWarnings.cmake
# Standard warning flags applied to all OpenSWBF targets.

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wcast-align
    -Wunused
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
)

# GCC-specific warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
    )
endif()

# Treat warnings as errors in CI builds (opt-in)
option(OPENSWBF_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
if(OPENSWBF_WARNINGS_AS_ERRORS)
    add_compile_options(-Werror)
endif()
