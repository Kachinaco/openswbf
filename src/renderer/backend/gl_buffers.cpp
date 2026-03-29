#include "renderer/backend/gl_buffers.h"

namespace swbf {

// ===========================================================================
// VertexBuffer
// ===========================================================================

VertexBuffer::~VertexBuffer() { destroy(); }

VertexBuffer::VertexBuffer(VertexBuffer&& other) noexcept
    : m_vbo(other.m_vbo) { other.m_vbo = 0; }

VertexBuffer& VertexBuffer::operator=(VertexBuffer&& other) noexcept {
    if (this != &other) { destroy(); m_vbo = other.m_vbo; other.m_vbo = 0; }
    return *this;
}

void VertexBuffer::create(const void* data, std::size_t size_bytes, GLenum usage) {
    destroy();
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(size_bytes), data, usage);
}

void VertexBuffer::update(const void* data, std::size_t size_bytes, std::size_t offset) {
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offset),
                    static_cast<GLsizeiptr>(size_bytes), data);
}

void VertexBuffer::bind() const { glBindBuffer(GL_ARRAY_BUFFER, m_vbo); }
void VertexBuffer::unbind()     { glBindBuffer(GL_ARRAY_BUFFER, 0); }

void VertexBuffer::destroy() {
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
}

// ===========================================================================
// IndexBuffer
// ===========================================================================

IndexBuffer::~IndexBuffer() { destroy(); }

IndexBuffer::IndexBuffer(IndexBuffer&& other) noexcept
    : m_ebo(other.m_ebo) { other.m_ebo = 0; }

IndexBuffer& IndexBuffer::operator=(IndexBuffer&& other) noexcept {
    if (this != &other) { destroy(); m_ebo = other.m_ebo; other.m_ebo = 0; }
    return *this;
}

void IndexBuffer::create(const void* data, std::size_t size_bytes, GLenum usage) {
    destroy();
    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(size_bytes), data, usage);
}

void IndexBuffer::bind() const { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo); }
void IndexBuffer::unbind()     { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); }

void IndexBuffer::destroy() {
    if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
}

// ===========================================================================
// VertexArray
// ===========================================================================

VertexArray::~VertexArray() { destroy(); }

VertexArray::VertexArray(VertexArray&& other) noexcept
    : m_vao(other.m_vao) { other.m_vao = 0; }

VertexArray& VertexArray::operator=(VertexArray&& other) noexcept {
    if (this != &other) { destroy(); m_vao = other.m_vao; other.m_vao = 0; }
    return *this;
}

void VertexArray::create() {
    destroy();
    glGenVertexArrays(1, &m_vao);
}

void VertexArray::bind() const { glBindVertexArray(m_vao); }
void VertexArray::unbind()     { glBindVertexArray(0); }

void VertexArray::destroy() {
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
}

void VertexArray::attrib(GLuint index, GLint components, GLenum type,
                         GLboolean normalized, GLsizei stride, std::size_t offset) {
    glEnableVertexAttribArray(index);
    glVertexAttribPointer(index, components, type, normalized, stride,
                          reinterpret_cast<const void*>(offset));
}

} // namespace swbf
