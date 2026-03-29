#pragma once

#include "renderer/backend/gl_context.h"
#include "core/types.h"

#include <cstddef>

namespace swbf {

// ---------------------------------------------------------------------------
// VertexBuffer — wraps a GL_ARRAY_BUFFER
// ---------------------------------------------------------------------------

class VertexBuffer {
public:
    VertexBuffer() = default;
    ~VertexBuffer();

    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;

    VertexBuffer(VertexBuffer&& other) noexcept;
    VertexBuffer& operator=(VertexBuffer&& other) noexcept;

    /// Create the buffer and upload data. Pass nullptr for data to allocate
    /// without uploading (use update() later).
    void create(const void* data, std::size_t size_bytes, GLenum usage = GL_STATIC_DRAW);

    /// Re-upload data into the existing buffer (must not exceed original size).
    void update(const void* data, std::size_t size_bytes, std::size_t offset = 0);

    void bind() const;
    static void unbind();
    void destroy();

    GLuint handle() const { return m_vbo; }

private:
    GLuint m_vbo = 0;
};

// ---------------------------------------------------------------------------
// IndexBuffer — wraps a GL_ELEMENT_ARRAY_BUFFER
// ---------------------------------------------------------------------------

class IndexBuffer {
public:
    IndexBuffer() = default;
    ~IndexBuffer();

    IndexBuffer(const IndexBuffer&) = delete;
    IndexBuffer& operator=(const IndexBuffer&) = delete;

    IndexBuffer(IndexBuffer&& other) noexcept;
    IndexBuffer& operator=(IndexBuffer&& other) noexcept;

    /// Create the buffer and upload index data.
    void create(const void* data, std::size_t size_bytes, GLenum usage = GL_STATIC_DRAW);

    void bind() const;
    static void unbind();
    void destroy();

    GLuint handle() const { return m_ebo; }

private:
    GLuint m_ebo = 0;
};

// ---------------------------------------------------------------------------
// VertexArray — wraps a Vertex Array Object (VAO)
// ---------------------------------------------------------------------------

class VertexArray {
public:
    VertexArray() = default;
    ~VertexArray();

    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;

    VertexArray(VertexArray&& other) noexcept;
    VertexArray& operator=(VertexArray&& other) noexcept;

    void create();
    void bind() const;
    static void unbind();
    void destroy();

    /// Configure a vertex attribute pointer. The VAO must be bound.
    /// Calls glEnableVertexAttribArray + glVertexAttribPointer.
    void attrib(GLuint index, GLint components, GLenum type,
                GLboolean normalized, GLsizei stride, std::size_t offset);

    GLuint handle() const { return m_vao; }

private:
    GLuint m_vao = 0;
};

} // namespace swbf
