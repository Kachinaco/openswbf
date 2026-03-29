#include "renderer/sky_renderer.h"
#include "core/log.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace swbf {

// -------------------------------------------------------------------------
// GLSL ES 300 shaders (embedded as string literals)
// -------------------------------------------------------------------------

static const char* k_sky_vert_src = R"(#version 100
precision highp float;

attribute vec3 a_position;

uniform mat4 u_view;
uniform mat4 u_projection;

varying float v_height;

void main() {
    // Normalized Y of the vertex (0 at horizon, 1 at zenith).
    v_height = a_position.y;

    // Remove translation from the view matrix so the sky dome is always
    // centered on the camera.
    mat4 view_no_translate = u_view;
    view_no_translate[3] = vec4(0.0, 0.0, 0.0, 1.0);

    gl_Position = u_projection * view_no_translate * vec4(a_position, 1.0);
}
)";

static const char* k_sky_frag_src = R"(#version 100
precision highp float;

uniform vec3 u_top_color;
uniform vec3 u_bottom_color;

varying float v_height;

void main() {
    // Smoothstep gives a slightly more pleasing gradient than a raw lerp.
    float t = smoothstep(0.0, 1.0, v_height);
    vec3 color = mix(u_bottom_color, u_top_color, t);
    gl_FragColor = vec4(color, 1.0);
}
)";

// -------------------------------------------------------------------------
// Shader compilation helpers
// -------------------------------------------------------------------------

static GLuint compile_gl_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
        LOG_ERROR("SkyRenderer: shader compile error: %s", info);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint link_gl_program(GLuint vert, GLuint frag) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);

    // Bind attribute locations before linking (required for GLSL ES 1.00).
    glBindAttribLocation(program, 0, "a_position");

    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(program, sizeof(info), nullptr, info);
        LOG_ERROR("SkyRenderer: program link error: %s", info);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

// -------------------------------------------------------------------------
// compile_shader
// -------------------------------------------------------------------------
bool SkyRenderer::compile_shader() {
    GLuint vert = compile_gl_shader(GL_VERTEX_SHADER, k_sky_vert_src);
    if (!vert) return false;

    GLuint frag = compile_gl_shader(GL_FRAGMENT_SHADER, k_sky_frag_src);
    if (!frag) {
        glDeleteShader(vert);
        return false;
    }

    m_program = link_gl_program(vert, frag);

    // Shaders can be detached and deleted after linking.
    glDeleteShader(vert);
    glDeleteShader(frag);

    if (!m_program) return false;

    // Cache uniform locations.
    m_loc_view       = glGetUniformLocation(m_program, "u_view");
    m_loc_projection = glGetUniformLocation(m_program, "u_projection");
    m_loc_top_color  = glGetUniformLocation(m_program, "u_top_color");
    m_loc_bot_color  = glGetUniformLocation(m_program, "u_bottom_color");

    return true;
}

// -------------------------------------------------------------------------
// generate_dome
//
// Generates the upper hemisphere of a UV sphere.
//
//   longitude_segments: slices around the Y axis (like meridians)
//   latitude_segments:  rings from equator to pole
//
// Vertices are laid out as (latitude_segments + 1) rings of
// (longitude_segments + 1) vertices each.  The +1 on longitude duplicates
// the seam column so texture coordinates would wrap cleanly (not needed here
// but keeps the topology simple).
//
// Each vertex stores only position (vec3).  The height (y) is used in the
// shader for the gradient, so no extra attributes are needed.
// -------------------------------------------------------------------------
void SkyRenderer::generate_dome(u32 lon_segs, u32 lat_segs) {
    const float radius = 500.0f;  // Large enough to enclose the near plane.
    const float pi     = 3.14159265358979323846f;

    // --- vertices ---
    std::vector<float> vertices;
    vertices.reserve((lat_segs + 1) * (lon_segs + 1) * 3);

    for (u32 lat = 0; lat <= lat_segs; ++lat) {
        // phi goes from 0 (equator) to pi/2 (north pole).
        float phi = (static_cast<float>(lat) / static_cast<float>(lat_segs))
                    * (pi * 0.5f);
        float y   = std::sin(phi);
        float xz  = std::cos(phi);  // radius in the xz-plane at this latitude

        for (u32 lon = 0; lon <= lon_segs; ++lon) {
            float theta = (static_cast<float>(lon) / static_cast<float>(lon_segs))
                          * (2.0f * pi);
            float x = xz * std::cos(theta);
            float z = xz * std::sin(theta);

            vertices.push_back(x * radius);
            vertices.push_back(y * radius);
            vertices.push_back(z * radius);
        }
    }

    // --- indices (triangles) ---
    std::vector<u32> indices;
    indices.reserve(lat_segs * lon_segs * 6);

    u32 ring_size = lon_segs + 1;
    for (u32 lat = 0; lat < lat_segs; ++lat) {
        for (u32 lon = 0; lon < lon_segs; ++lon) {
            u32 curr = lat * ring_size + lon;
            u32 next = curr + ring_size;

            // Two triangles per quad, wound counter-clockwise when viewed
            // from outside (we render back-faces since the camera is inside
            // the dome — culling is disabled during sky rendering).
            indices.push_back(curr);
            indices.push_back(curr + 1);
            indices.push_back(next);

            indices.push_back(next);
            indices.push_back(curr + 1);
            indices.push_back(next + 1);
        }
    }

    m_index_count = static_cast<u32>(indices.size());

    // --- Upload to GPU ---
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ibo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);

    // layout(location = 0) in vec3 a_position;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                          nullptr);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(u32)),
                 indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    LOG_INFO("SkyRenderer: dome generated — %u verts, %u indices "
             "(%ux%u segments)",
             static_cast<unsigned>(vertices.size() / 3),
             static_cast<unsigned>(m_index_count),
             lon_segs, lat_segs);
}

// -------------------------------------------------------------------------
// init
// -------------------------------------------------------------------------
bool SkyRenderer::init() {
    if (!compile_shader()) {
        LOG_ERROR("SkyRenderer: failed to compile sky shader");
        return false;
    }

    generate_dome(/*longitude_segments=*/32, /*latitude_segments=*/16);

    LOG_INFO("SkyRenderer: initialised");
    return true;
}

// -------------------------------------------------------------------------
// render
// -------------------------------------------------------------------------
void SkyRenderer::render(const float* view_matrix, const float* proj_matrix) {
    if (!m_program || !m_vao) return;

    // The sky must render behind everything: disable depth writes and depth
    // testing, and disable back-face culling (the camera is inside the dome).
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glUseProgram(m_program);

    // Upload the view matrix with translation zeroed out.  The shader also
    // strips translation, but we do it CPU-side as well so the uniform
    // reflects the intent and avoids any precision issues with large
    // translation values.
    float view_no_translate[16];
    std::memcpy(view_no_translate, view_matrix, 16 * sizeof(float));
    // Column-major: translation is in elements [12], [13], [14].
    view_no_translate[12] = 0.0f;
    view_no_translate[13] = 0.0f;
    view_no_translate[14] = 0.0f;

    glUniformMatrix4fv(m_loc_view, 1, GL_FALSE, view_no_translate);
    glUniformMatrix4fv(m_loc_projection, 1, GL_FALSE, proj_matrix);

    glUniform3fv(m_loc_top_color, 1, m_top_color);
    glUniform3fv(m_loc_bot_color, 1, m_bottom_color);

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_index_count),
                   GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    glUseProgram(0);

    // Restore default GL state.
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}

// -------------------------------------------------------------------------
// set_colors
// -------------------------------------------------------------------------
void SkyRenderer::set_colors(float top_r, float top_g, float top_b,
                             float bottom_r, float bottom_g, float bottom_b) {
    m_top_color[0] = top_r;
    m_top_color[1] = top_g;
    m_top_color[2] = top_b;

    m_bottom_color[0] = bottom_r;
    m_bottom_color[1] = bottom_g;
    m_bottom_color[2] = bottom_b;
}

// -------------------------------------------------------------------------
// destroy
// -------------------------------------------------------------------------
void SkyRenderer::destroy() {
    if (m_ibo) {
        glDeleteBuffers(1, &m_ibo);
        m_ibo = 0;
    }
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    m_index_count = 0;

    LOG_INFO("SkyRenderer: destroyed");
}

} // namespace swbf
