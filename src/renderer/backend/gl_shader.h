#pragma once

#include "renderer/backend/gl_context.h"
#include "core/types.h"

#include <initializer_list>
#include <string>
#include <utility>

namespace swbf {

/// Attribute binding: (location index, attribute name).
using AttribBinding = std::pair<GLuint, const char*>;

// ---------------------------------------------------------------------------
// Shader — compiles and links a vertex + fragment GLSL ES 1.00 program
// ---------------------------------------------------------------------------

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    /// Compile and link a program from vertex and fragment shader source.
    /// Returns true on success. On failure, logs the error and returns false.
    bool compile(const char* vert_src, const char* frag_src);

    /// Compile and link with explicit attribute location bindings.
    /// Bindings are applied before linking (required for GLSL ES 1.00).
    bool compile(const char* vert_src, const char* frag_src,
                 std::initializer_list<AttribBinding> attribs);

    /// Bind this program for rendering.
    void bind() const;

    /// Alias for bind() — used by some renderers.
    void use() const { bind(); }

    /// Unbind (bind program 0).
    static void unbind();

    /// Destroy the GL program.
    void destroy();

    /// Get the native GL program handle.
    GLuint handle() const { return m_program; }

    // Uniform setters --------------------------------------------------------

    GLint uniform_location(const char* name) const;

    void set_int(const char* name, int value) const;
    void set_float(const char* name, float value) const;
    void set_vec3(const char* name, const float* value) const;
    void set_vec4(const char* name, const float* value) const;
    void set_mat4(const char* name, const float* value) const;

private:
    GLuint m_program = 0;

    static GLuint compile_shader(GLenum type, const char* source);
};

} // namespace swbf
