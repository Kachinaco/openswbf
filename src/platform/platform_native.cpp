#include "platform/platform.h"

#ifndef OPENSWBF_PLATFORM_NATIVE
// This translation unit is only compiled for native (desktop) builds.
// When building for Emscripten, this file produces no symbols.
#else

#include "core/log.h"

#include <SDL.h>
#include <SDL_opengl.h>

namespace swbf {

struct PlatformWindow {
    SDL_Window*   sdl_window  = nullptr;
    SDL_GLContext  gl_context  = nullptr;
};

PlatformWindow* platform_init(const PlatformConfig& config) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return nullptr;
    }

    // Try OpenGL ES 3.0 first (matches WebGL 2 / Emscripten path).
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (config.fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
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
        LOG_WARN("Failed to create OpenGL ES 3.0 window: %s", SDL_GetError());
        LOG_INFO("Falling back to OpenGL 3.3 core profile");

        // Fallback: OpenGL 3.3 core profile (common on desktop Linux/macOS/Windows).
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

        sdl_window = SDL_CreateWindow(
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
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(sdl_window);
    if (!gl_context) {
        LOG_ERROR("SDL_GL_CreateContext failed: %s", SDL_GetError());
        SDL_DestroyWindow(sdl_window);
        SDL_Quit();
        return nullptr;
    }

    // Enable vsync by default (adaptive if available, otherwise standard).
    if (SDL_GL_SetSwapInterval(-1) != 0) {
        SDL_GL_SetSwapInterval(1);
    }

    LOG_INFO("Platform (native) initialised: %dx%d", config.window_width, config.window_height);
    LOG_INFO("  GL Vendor:   %s", glGetString(GL_VENDOR));
    LOG_INFO("  GL Renderer: %s", glGetString(GL_RENDERER));
    LOG_INFO("  GL Version:  %s", glGetString(GL_VERSION));

    auto* window = new PlatformWindow();
    window->sdl_window = sdl_window;
    window->gl_context = gl_context;
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

    LOG_INFO("Platform (native) shut down");
}

void platform_swap_buffers(PlatformWindow* window) {
    if (window && window->sdl_window) {
        SDL_GL_SwapWindow(window->sdl_window);
    }
}

void platform_get_size(PlatformWindow* window, int* w, int* h) {
    if (window && window->sdl_window) {
        SDL_GL_GetDrawableSize(window->sdl_window, w, h);
    } else {
        if (w) *w = 0;
        if (h) *h = 0;
    }
}

} // namespace swbf

#endif // OPENSWBF_PLATFORM_NATIVE
