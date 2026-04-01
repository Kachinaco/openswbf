# src/audio/CMakeLists.txt
# OpenAL-based audio: sound effects, music, 3D positional audio.

file(GLOB_RECURSE AUDIO_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_library(openswbf_audio STATIC ${AUDIO_SOURCES})

target_include_directories(openswbf_audio
    PUBLIC
        "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(openswbf_audio
    PUBLIC
        openswbf_core
        openswbf_input
)

if(NOT EMSCRIPTEN)
    target_link_libraries(openswbf_audio PUBLIC OpenAL::OpenAL)
endif()
