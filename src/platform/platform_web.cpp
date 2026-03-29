#include "platform/platform.h"

#ifndef OPENSWBF_PLATFORM_WEB
// This translation unit is only compiled for Emscripten (WebAssembly) builds.
// When building natively, this file produces no symbols.
#else

#include "core/log.h"

#include <SDL.h>

namespace swbf {

struct PlatformWindow {
    SDL_Window*   sdl_window  = nullptr;
    SDL_GLContext  gl_context  = nullptr;
    int           canvas_w    = 0;
    int           canvas_h    = 0;
};

PlatformWindow* platform_init(const PlatformConfig& config) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return nullptr;
    }

    // On Emscripten, SDL2 targets WebGL. Request WebGL 1 (OpenGL ES 2.0).
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Set the HTML canvas size before creating the window.
    emscripten_set_canvas_element_size("#canvas", config.window_width, config.window_height);

    Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    if (config.fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN;
    }

    SDL_Window* sdl_window = SDL_CreateWindow(
        config.title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        config.window_width,
        config.window_height,
        window_flags
    );

    if (!sdl_window) {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return nullptr;
    }

    // On Emscripten, SDL_CreateWindow implicitly creates a WebGL context,
    // but we create one explicitly to capture the handle for cleanup.
    SDL_GLContext gl_context = SDL_GL_CreateContext(sdl_window);
    if (!gl_context) {
        LOG_ERROR("SDL_GL_CreateContext failed: %s", SDL_GetError());
        SDL_DestroyWindow(sdl_window);
        SDL_Quit();
        return nullptr;
    }

    auto* window = new PlatformWindow();
    window->sdl_window = sdl_window;
    window->gl_context = gl_context;
    window->canvas_w   = config.window_width;
    window->canvas_h   = config.window_height;

    LOG_INFO("Platform (web/Emscripten) initialised: %dx%d",
             config.window_width, config.window_height);

    return window;
}

void platform_shutdown(PlatformWindow* window) {
    if (!window) return;

    if (window->gl_context) {
        SDL_GL_DeleteContext(window->gl_context);
    }
    if (window->sdl_window) {
        SDL_DestroyWindow(window->sdl_window);
    }

    SDL_Quit();
    delete window;

    LOG_INFO("Platform (web) shut down");
}

void platform_swap_buffers(PlatformWindow* window) {
    if (window && window->sdl_window) {
        SDL_GL_SwapWindow(window->sdl_window);
    }
}

void platform_get_size(PlatformWindow* window, int* w, int* h) {
    if (window && window->sdl_window) {
        // On Emscripten, query the actual canvas size which may differ from
        // the initial config if the browser resized it.
        int cw = 0, ch = 0;
        emscripten_get_canvas_element_size("#canvas", &cw, &ch);
        if (w) *w = cw;
        if (h) *h = ch;
    } else {
        if (w) *w = 0;
        if (h) *h = 0;
    }
}

} // namespace swbf

#endif // OPENSWBF_PLATFORM_WEB
