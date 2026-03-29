#pragma once

#ifdef __EMSCRIPTEN__
#define OPENSWBF_PLATFORM_WEB 1
#include <emscripten.h>
#include <emscripten/html5.h>
#else
#define OPENSWBF_PLATFORM_NATIVE 1
#endif

namespace swbf {

struct PlatformConfig {
    int window_width = 1280;
    int window_height = 720;
    const char* title = "OpenSWBF";
    bool fullscreen = false;
};

// SDL window handle (opaque to callers)
struct PlatformWindow;

PlatformWindow* platform_init(const PlatformConfig& config);
void platform_shutdown(PlatformWindow* window);
void platform_swap_buffers(PlatformWindow* window);
void platform_get_size(PlatformWindow* window, int* w, int* h);

} // namespace swbf
