#include "renderer/backend/gl_context.h"
#include "core/log.h"

namespace swbf {

GLContext::~GLContext() {
    if (m_gl_ctx) {
        SDL_GL_DeleteContext(m_gl_ctx);
        m_gl_ctx = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

bool GLContext::init(int width, int height) {
    m_width  = width;
    m_height = height;

    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            LOG_ERROR("GLContext: SDL_Init failed: %s", SDL_GetError());
            return false;
        }
    }

    // Request OpenGL ES 2.0 (maps to WebGL 1 on Emscripten).
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    m_window = SDL_CreateWindow(
        "OpenSWBF",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        m_width, m_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);

    if (!m_window) {
        LOG_ERROR("GLContext: SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    m_gl_ctx = SDL_GL_CreateContext(m_window);
    if (!m_gl_ctx) {
        LOG_ERROR("GLContext: SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }

    SDL_GL_SetSwapInterval(1);

#ifndef __EMSCRIPTEN__
    // Desktop: initialize GLEW for GL function loading.
    glewExperimental = GL_TRUE;
    GLenum glew_err = glewInit();
    if (glew_err != GLEW_OK) {
        LOG_ERROR("GLContext: glewInit failed: %s",
                  reinterpret_cast<const char*>(glewGetErrorString(glew_err)));
        return false;
    }
#endif

    LOG_INFO("GLContext: context created (%dx%d)", m_width, m_height);
    LOG_INFO("GLContext: GL_RENDERER = %s",
             reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    LOG_INFO("GLContext: GL_VERSION  = %s",
             reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    // Common GL state.
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    return true;
}

void GLContext::begin_frame() {
    glViewport(0, 0, m_width, m_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLContext::end_frame() {
    SDL_GL_SwapWindow(m_window);
}

void GLContext::resize(int w, int h) {
    m_width  = w;
    m_height = h;
    glViewport(0, 0, m_width, m_height);
}

} // namespace swbf
