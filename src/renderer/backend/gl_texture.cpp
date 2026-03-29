#include "renderer/backend/gl_texture.h"

namespace swbf {

GLTexture::~GLTexture() { destroy(); }

GLTexture::GLTexture(GLTexture&& other) noexcept
    : m_texture(other.m_texture), m_width(other.m_width), m_height(other.m_height) {
    other.m_texture = 0;
    other.m_width = 0;
    other.m_height = 0;
}

GLTexture& GLTexture::operator=(GLTexture&& other) noexcept {
    if (this != &other) {
        destroy();
        m_texture = other.m_texture;
        m_width = other.m_width;
        m_height = other.m_height;
        other.m_texture = 0;
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

void GLTexture::create(u32 width, u32 height, const void* pixels,
                       GLenum internal_format, GLenum format, GLenum type) {
    destroy();

    m_width = width;
    m_height = height;

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internal_format),
                 static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                 0, format, type, pixels);

    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GLTexture::bind(u32 unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_texture);
}

void GLTexture::destroy() {
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
        m_width = 0;
        m_height = 0;
    }
}

} // namespace swbf
