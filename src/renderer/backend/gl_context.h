#pragma once

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#else
#include <GL/glew.h>
#include <SDL.h>
#include <SDL_opengl.h>
#endif

namespace swbf {

/// Manages the OpenGL ES 3.0 / WebGL 2 rendering context.
///
/// On native platforms the context is created through SDL2.
/// Under Emscripten the context is obtained from the HTML5 canvas.
class GLContext {
public:
    GLContext() = default;
    ~GLContext();

    GLContext(const GLContext&) = delete;
    GLContext& operator=(const GLContext&) = delete;

    /// Create the GL context for the given window dimensions.
    /// On native builds this expects an SDL_Window to already exist (it will be
    /// created internally).  On Emscripten the canvas element is used instead.
    bool init(int width, int height);

    /// Call at the start of each frame — clears color/depth buffers and sets
    /// the viewport to the current width/height.
    void begin_frame();

    /// Call at the end of each frame — swaps front/back buffers.
    void end_frame();

    /// Current framebuffer dimensions.
    int width() const { return m_width; }
    int height() const { return m_height; }

    /// Update the stored dimensions and glViewport.
    void resize(int w, int h);

#ifndef __EMSCRIPTEN__
    /// Access the underlying SDL window (native only).
    SDL_Window* sdl_window() const { return m_window; }
#endif

private:
    int m_width  = 0;
    int m_height = 0;

#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE m_context = 0;
#else
    SDL_Window*   m_window  = nullptr;
    SDL_GLContext  m_gl_ctx  = nullptr;
#endif
};

} // namespace swbf
