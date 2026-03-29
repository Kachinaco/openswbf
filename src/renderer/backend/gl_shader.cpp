#include "renderer/backend/gl_shader.h"
#include "core/log.h"

#include <vector>

namespace swbf {

Shader::~Shader() {
    destroy();
}

Shader::Shader(Shader&& other) noexcept
    : m_program(other.m_program) {
    other.m_program = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        destroy();
        m_program = other.m_program;
        other.m_program = 0;
    }
    return *this;
}

GLuint Shader::compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint log_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
        std::vector<char> log_buf(static_cast<std::size_t>(log_len + 1), '\0');
        glGetShaderInfoLog(shader, log_len, nullptr, log_buf.data());
        const char* type_str = (type == GL_VERTEX_SHADER) ? "vertex" : "fragment";
        LOG_ERROR("Shader compile error (%s): %s", type_str, log_buf.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Shader::compile(const char* vert_src, const char* frag_src) {
    GLuint vert = compile_shader(GL_VERTEX_SHADER, vert_src);
    if (!vert) return false;

    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!frag) {
        glDeleteShader(vert);
        return false;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint log_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
        std::vector<char> log_buf(static_cast<std::size_t>(log_len + 1), '\0');
        glGetProgramInfoLog(program, log_len, nullptr, log_buf.data());
        LOG_ERROR("Shader link error: %s", log_buf.data());
        glDeleteProgram(program);
        glDeleteShader(vert);
        glDeleteShader(frag);
        return false;
    }

    // Shaders can be detached and deleted after linking.
    glDetachShader(program, vert);
    glDetachShader(program, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);

    // Destroy any previous program before storing the new one.
    destroy();
    m_program = program;

    LOG_INFO("Shader compiled and linked successfully (program %u)", m_program);
    return true;
}

void Shader::bind() const {
    glUseProgram(m_program);
}

void Shader::unbind() {
    glUseProgram(0);
}

void Shader::destroy() {
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

GLint Shader::uniform_location(const char* name) const {
    return glGetUniformLocation(m_program, name);
}

void Shader::set_int(const char* name, int value) const {
    glUniform1i(uniform_location(name), value);
}

void Shader::set_float(const char* name, float value) const {
    glUniform1f(uniform_location(name), value);
}

void Shader::set_vec3(const char* name, const float* value) const {
    glUniform3fv(uniform_location(name), 1, value);
}

void Shader::set_vec4(const char* name, const float* value) const {
    glUniform4fv(uniform_location(name), 1, value);
}

void Shader::set_mat4(const char* name, const float* value) const {
    glUniformMatrix4fv(uniform_location(name), 1, GL_FALSE, value);
}

} // namespace swbf
