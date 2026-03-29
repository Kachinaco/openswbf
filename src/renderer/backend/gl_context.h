#pragma once

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#include <SDL.h>
#else
#include <GL/glew.h>
#include <SDL.h>
#include <SDL_opengl.h>
#endif

namespace swbf {

/// Manages the OpenGL ES 2.0 / WebGL 1 rendering context via SDL2.
/// Works on both native desktop and Emscripten (WebGL 1).
class GLContext {
public:
    GLContext() = default;
    ~GLContext();

    GLContext(const GLContext&) = delete;
    GLContext& operator=(const GLContext&) = delete;

    bool init(int width, int height);
    void begin_frame();
    void end_frame();

    int width() const { return m_width; }
    int height() const { return m_height; }
    void resize(int w, int h);

    SDL_Window* sdl_window() const { return m_window; }

private:
    int m_width  = 0;
    int m_height = 0;
    SDL_Window*   m_window  = nullptr;
    SDL_GLContext  m_gl_ctx  = nullptr;
};

} // namespace swbf
