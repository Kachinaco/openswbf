#pragma once

#include "renderer/backend/gl_context.h"
#include "core/types.h"

namespace swbf {

// ---------------------------------------------------------------------------
// GLTexture — wraps a GL_TEXTURE_2D handle
// ---------------------------------------------------------------------------

class GLTexture {
public:
    GLTexture() = default;
    ~GLTexture();

    GLTexture(const GLTexture&) = delete;
    GLTexture& operator=(const GLTexture&) = delete;

    GLTexture(GLTexture&& other) noexcept;
    GLTexture& operator=(GLTexture&& other) noexcept;

    /// Create a 2D texture from RGBA8 pixel data.
    void create(u32 width, u32 height, const void* pixels,
                GLenum internal_format = GL_RGBA,
                GLenum format = GL_RGBA,
                GLenum type = GL_UNSIGNED_BYTE);

    /// Bind to the specified texture unit (0-based).
    void bind(u32 unit = 0) const;

    void destroy();

    GLuint handle() const { return m_texture; }
    u32 width() const { return m_width; }
    u32 height() const { return m_height; }
    bool valid() const { return m_texture != 0; }

private:
    GLuint m_texture = 0;
    u32 m_width = 0;
    u32 m_height = 0;
};

} // namespace swbf
