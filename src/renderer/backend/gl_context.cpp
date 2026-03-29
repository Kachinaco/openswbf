#include "renderer/backend/gl_context.h"
#include "core/log.h"

namespace swbf {

// -------------------------------------------------------------------------
// Destructor
// -------------------------------------------------------------------------
GLContext::~GLContext() {
#ifdef __EMSCRIPTEN__
    if (m_context) {
        emscripten_webgl_destroy_context(m_context);
        m_context = 0;
    }
#else
    if (m_gl_ctx) {
        SDL_GL_DeleteContext(m_gl_ctx);
        m_gl_ctx = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
#endif
}

// -------------------------------------------------------------------------
// init
// -------------------------------------------------------------------------
bool GLContext::init(int width, int height) {
    m_width  = width;
    m_height = height;

#ifdef __EMSCRIPTEN__
    // ------- Emscripten / WebGL 2 path ---------------------------------
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.majorVersion = 2;  // WebGL 2 == ES 3.0
    attrs.minorVersion = 0;
    attrs.alpha        = false;
    attrs.depth        = true;
    attrs.stencil      = false;
    attrs.antialias    = true;

    // Target the default canvas element ("#canvas").
    m_context = emscripten_webgl_create_context("#canvas", &attrs);
    if (m_context <= 0) {
        LOG_ERROR("GLContext: failed to create WebGL 2 context (error %d)",
                  static_cast<int>(m_context));
        return false;
    }
    emscripten_webgl_make_context_current(m_context);
    LOG_INFO("GLContext: WebGL 2 context created (%dx%d)", m_width, m_height);
#else
    // ------- Native SDL2 path ------------------------------------------
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            LOG_ERROR("GLContext: SDL_Init failed: %s", SDL_GetError());
            return false;
        }
    }

    // Request an OpenGL ES 3.0 context.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
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
        LOG_ERROR("GLContext: SDL_GL_CreateContext failed: %s",
                  SDL_GetError());
        return false;
    }

    // Enable vsync.
    SDL_GL_SetSwapInterval(1);

    // Initialize GLEW for desktop GL function loading.
    glewExperimental = GL_TRUE;
    GLenum glew_err = glewInit();
    if (glew_err != GLEW_OK) {
        LOG_ERROR("GLContext: glewInit failed: %s",
                  reinterpret_cast<const char*>(glewGetErrorString(glew_err)));
        return false;
    }

    LOG_INFO("GLContext: OpenGL context created (%dx%d)", m_width, m_height);
    LOG_INFO("GLContext: GL_RENDERER  = %s",
             reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    LOG_INFO("GLContext: GL_VERSION   = %s",
             reinterpret_cast<const char*>(glGetString(GL_VERSION)));
#endif

    // Common GL state.
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    return true;
}

// -------------------------------------------------------------------------
// begin_frame
// -------------------------------------------------------------------------
void GLContext::begin_frame() {
    glViewport(0, 0, m_width, m_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// -------------------------------------------------------------------------
// end_frame
// -------------------------------------------------------------------------
void GLContext::end_frame() {
#ifdef __EMSCRIPTEN__
    // Under Emscripten the browser composites the canvas automatically;
    // an explicit swap is not necessary.  If a double-buffered context is
    // used, emscripten_webgl_commit_frame() can be called, but the default
    // requestAnimationFrame loop handles this.
#else
    SDL_GL_SwapWindow(m_window);
#endif
}

// -------------------------------------------------------------------------
// resize
// -------------------------------------------------------------------------
void GLContext::resize(int w, int h) {
    m_width  = w;
    m_height = h;
    glViewport(0, 0, m_width, m_height);
    LOG_DEBUG("GLContext: resized to %dx%d", m_width, m_height);
}

} // namespace swbf
