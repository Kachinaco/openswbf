#include "renderer/ui_renderer.h"
#include "core/log.h"

#include <cmath>
#include <cstring>

namespace swbf {

// =========================================================================
// Shader sources — GLSL ES 1.00 (WebGL 1 compatible)
// =========================================================================

static const char* k_ui_vert = R"(#version 100
precision highp float;

attribute vec2 a_position;
attribute vec4 a_color;

uniform vec2 u_screen_size;

varying vec4 v_color;

void main() {
    vec2 ndc = vec2(
        (a_position.x / u_screen_size.x) *  2.0 - 1.0,
        (a_position.y / u_screen_size.y) * -2.0 + 1.0
    );
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_color = a_color;
}
)";

static const char* k_ui_frag = R"(#version 100
precision highp float;

varying vec4 v_color;

void main() {
    gl_FragColor = v_color;
}
)";

// =========================================================================
// Minimal 5x7 bitmap font for printable ASCII (32-126).
// Each character is 5 columns x 7 rows, packed as 5 bytes (one per column,
// LSB = top row).
// =========================================================================

// clang-format off
static const uint8_t k_font_5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // 32 ' '
    {0x00,0x00,0x5F,0x00,0x00}, // 33 '!'
    {0x00,0x07,0x00,0x07,0x00}, // 34 '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // 35 '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // 36 '$'
    {0x23,0x13,0x08,0x64,0x62}, // 37 '%'
    {0x36,0x49,0x55,0x22,0x50}, // 38 '&'
    {0x00,0x05,0x03,0x00,0x00}, // 39 '''
    {0x00,0x1C,0x22,0x41,0x00}, // 40 '('
    {0x00,0x41,0x22,0x1C,0x00}, // 41 ')'
    {0x14,0x08,0x3E,0x08,0x14}, // 42 '*'
    {0x08,0x08,0x3E,0x08,0x08}, // 43 '+'
    {0x00,0x50,0x30,0x00,0x00}, // 44 ','
    {0x08,0x08,0x08,0x08,0x08}, // 45 '-'
    {0x00,0x60,0x60,0x00,0x00}, // 46 '.'
    {0x20,0x10,0x08,0x04,0x02}, // 47 '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // 48 '0'
    {0x00,0x42,0x7F,0x40,0x00}, // 49 '1'
    {0x42,0x61,0x51,0x49,0x46}, // 50 '2'
    {0x21,0x41,0x45,0x4B,0x31}, // 51 '3'
    {0x18,0x14,0x12,0x7F,0x10}, // 52 '4'
    {0x27,0x45,0x45,0x45,0x39}, // 53 '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // 54 '6'
    {0x01,0x71,0x09,0x05,0x03}, // 55 '7'
    {0x36,0x49,0x49,0x49,0x36}, // 56 '8'
    {0x06,0x49,0x49,0x29,0x1E}, // 57 '9'
    {0x00,0x36,0x36,0x00,0x00}, // 58 ':'
    {0x00,0x56,0x36,0x00,0x00}, // 59 ';'
    {0x08,0x14,0x22,0x41,0x00}, // 60 '<'
    {0x14,0x14,0x14,0x14,0x14}, // 61 '='
    {0x00,0x41,0x22,0x14,0x08}, // 62 '>'
    {0x02,0x01,0x51,0x09,0x06}, // 63 '?'
    {0x32,0x49,0x79,0x41,0x3E}, // 64 '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 65 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 66 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 67 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 68 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 69 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 70 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 71 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 72 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 73 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 74 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 75 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 76 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 77 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 78 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 79 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 80 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 81 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 82 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 83 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 84 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 85 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 86 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 87 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 88 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 89 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 90 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // 91 '['
    {0x02,0x04,0x08,0x10,0x20}, // 92 backslash
    {0x00,0x41,0x41,0x7F,0x00}, // 93 ']'
    {0x04,0x02,0x01,0x02,0x04}, // 94 '^'
    {0x40,0x40,0x40,0x40,0x40}, // 95 '_'
    {0x00,0x01,0x02,0x04,0x00}, // 96 '`'
    {0x20,0x54,0x54,0x54,0x78}, // 97 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 98 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 99 'c'
    {0x38,0x44,0x44,0x48,0x7F}, //100 'd'
    {0x38,0x54,0x54,0x54,0x18}, //101 'e'
    {0x08,0x7E,0x09,0x01,0x02}, //102 'f'
    {0x0C,0x52,0x52,0x52,0x3E}, //103 'g'
    {0x7F,0x08,0x04,0x04,0x78}, //104 'h'
    {0x00,0x44,0x7D,0x40,0x00}, //105 'i'
    {0x20,0x40,0x44,0x3D,0x00}, //106 'j'
    {0x7F,0x10,0x28,0x44,0x00}, //107 'k'
    {0x00,0x41,0x7F,0x40,0x00}, //108 'l'
    {0x7C,0x04,0x18,0x04,0x78}, //109 'm'
    {0x7C,0x08,0x04,0x04,0x78}, //110 'n'
    {0x38,0x44,0x44,0x44,0x38}, //111 'o'
    {0x7C,0x14,0x14,0x14,0x08}, //112 'p'
    {0x08,0x14,0x14,0x18,0x7C}, //113 'q'
    {0x7C,0x08,0x04,0x04,0x08}, //114 'r'
    {0x48,0x54,0x54,0x54,0x20}, //115 's'
    {0x04,0x3F,0x44,0x40,0x20}, //116 't'
    {0x3C,0x40,0x40,0x20,0x7C}, //117 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, //118 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, //119 'w'
    {0x44,0x28,0x10,0x28,0x44}, //120 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, //121 'y'
    {0x44,0x64,0x54,0x4C,0x44}, //122 'z'
    {0x00,0x08,0x36,0x41,0x00}, //123 '{'
    {0x00,0x00,0x7F,0x00,0x00}, //124 '|'
    {0x00,0x41,0x36,0x08,0x00}, //125 '}'
    {0x10,0x08,0x08,0x10,0x08}, //126 '~'
};
// clang-format on

// =========================================================================
// Shader compilation
// =========================================================================

static u32 compile_ui_shader() {
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &k_ui_vert, nullptr);
    glCompileShader(vert);
    {
        GLint ok = 0;
        glGetShaderiv(vert, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char info[512];
            glGetShaderInfoLog(vert, sizeof(info), nullptr, info);
            LOG_ERROR("UI vert shader error: %s", info);
            glDeleteShader(vert);
            return 0;
        }
    }

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &k_ui_frag, nullptr);
    glCompileShader(frag);
    {
        GLint ok = 0;
        glGetShaderiv(frag, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char info[512];
            glGetShaderInfoLog(frag, sizeof(info), nullptr, info);
            LOG_ERROR("UI frag shader error: %s", info);
            glDeleteShader(vert);
            glDeleteShader(frag);
            return 0;
        }
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);

    glBindAttribLocation(prog, 0, "a_position");
    glBindAttribLocation(prog, 1, "a_color");

    glLinkProgram(prog);

    glDetachShader(prog, vert);
    glDetachShader(prog, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);

    {
        GLint ok = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            char info[512];
            glGetProgramInfoLog(prog, sizeof(info), nullptr, info);
            LOG_ERROR("UI program link error: %s", info);
            glDeleteProgram(prog);
            return 0;
        }
    }

    return prog;
}

// =========================================================================
// UIRenderer implementation
// =========================================================================

bool UIRenderer::init() {
    m_program = compile_ui_shader();
    if (!m_program) return false;
    LOG_INFO("UIRenderer initialised");
    return true;
}

void UIRenderer::destroy() {
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

void UIRenderer::begin(int screen_w, int screen_h) {
    m_screen_w = screen_w;
    m_screen_h = screen_h;
    m_vertices.clear();
}

void UIRenderer::flush() {
    if (m_vertices.empty() || !m_program) return;

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)),
                 m_vertices.data(), GL_STREAM_DRAW);

    constexpr GLsizei stride = sizeof(Vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(Vertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(Vertex, r)));

    glUseProgram(m_program);
    GLint loc_screen = glGetUniformLocation(m_program, "u_screen_size");
    if (loc_screen >= 0)
        glUniform2f(loc_screen,
                    static_cast<float>(m_screen_w),
                    static_cast<float>(m_screen_h));

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glUseProgram(0);

    m_vertices.clear();
}

// =========================================================================
// Primitive helpers
// =========================================================================

void UIRenderer::push_quad(float x0, float y0, float x1, float y1,
                           float r, float g, float b, float a) {
    m_vertices.push_back({x0, y0, r, g, b, a});
    m_vertices.push_back({x1, y0, r, g, b, a});
    m_vertices.push_back({x0, y1, r, g, b, a});

    m_vertices.push_back({x1, y0, r, g, b, a});
    m_vertices.push_back({x1, y1, r, g, b, a});
    m_vertices.push_back({x0, y1, r, g, b, a});
}

void UIRenderer::draw_rect(float x, float y, float w, float h,
                            float r, float g, float b, float a) {
    push_quad(x, y, x + w, y + h, r, g, b, a);
}

void UIRenderer::draw_rect_outline(float x, float y, float w, float h,
                                    float r, float g, float b, float a,
                                    float t) {
    // Top
    push_quad(x, y, x + w, y + t, r, g, b, a);
    // Bottom
    push_quad(x, y + h - t, x + w, y + h, r, g, b, a);
    // Left
    push_quad(x, y + t, x + t, y + h - t, r, g, b, a);
    // Right
    push_quad(x + w - t, y + t, x + w, y + h - t, r, g, b, a);
}

void UIRenderer::draw_circle(float cx, float cy, float radius,
                              float r, float g, float b, float a,
                              int segments) {
    constexpr float PI2 = 6.28318530718f;
    for (int i = 0; i < segments; ++i) {
        float angle0 = PI2 * static_cast<float>(i) / static_cast<float>(segments);
        float angle1 = PI2 * static_cast<float>(i + 1) / static_cast<float>(segments);

        float x0 = cx + radius * std::cos(angle0);
        float y0 = cy + radius * std::sin(angle0);
        float x1 = cx + radius * std::cos(angle1);
        float y1 = cy + radius * std::sin(angle1);

        m_vertices.push_back({cx, cy, r, g, b, a});
        m_vertices.push_back({x0, y0, r, g, b, a});
        m_vertices.push_back({x1, y1, r, g, b, a});
    }
}

void UIRenderer::draw_line(float x0, float y0, float x1, float y1,
                            float r, float g, float b, float a,
                            float thickness) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6f) return;

    float nx = -dy / len * thickness * 0.5f;
    float ny =  dx / len * thickness * 0.5f;

    m_vertices.push_back({x0 + nx, y0 + ny, r, g, b, a});
    m_vertices.push_back({x0 - nx, y0 - ny, r, g, b, a});
    m_vertices.push_back({x1 + nx, y1 + ny, r, g, b, a});

    m_vertices.push_back({x0 - nx, y0 - ny, r, g, b, a});
    m_vertices.push_back({x1 - nx, y1 - ny, r, g, b, a});
    m_vertices.push_back({x1 + nx, y1 + ny, r, g, b, a});
}

void UIRenderer::draw_triangle(float x0, float y0, float x1, float y1,
                                float x2, float y2,
                                float r, float g, float b, float a) {
    m_vertices.push_back({x0, y0, r, g, b, a});
    m_vertices.push_back({x1, y1, r, g, b, a});
    m_vertices.push_back({x2, y2, r, g, b, a});
}

// =========================================================================
// Text drawing
// =========================================================================

void UIRenderer::draw_text(const char* text, float px, float py,
                            float r, float g, float b, float a,
                            float scale) {
    if (!text) return;

    float cursor_x = px;
    float cursor_y = py;
    float pixel_w = scale;
    float pixel_h = scale;

    for (const char* ch = text; *ch; ++ch) {
        if (*ch == '\n') {
            cursor_x = px;
            cursor_y += 9.0f * scale;
            continue;
        }

        int idx = static_cast<int>(*ch) - 32;
        if (idx < 0 || idx > 94) idx = 0;

        const uint8_t* glyph = k_font_5x7[idx];
        for (int col = 0; col < 5; ++col) {
            uint8_t column_bits = glyph[col];
            for (int row = 0; row < 7; ++row) {
                if (column_bits & (1 << row)) {
                    float x0 = cursor_x + static_cast<float>(col) * pixel_w;
                    float y0 = cursor_y + static_cast<float>(row) * pixel_h;
                    push_quad(x0, y0, x0 + pixel_w, y0 + pixel_h, r, g, b, a);
                }
            }
        }
        cursor_x += 6.0f * scale;
    }
}

void UIRenderer::draw_text_shadow(const char* text, float px, float py,
                                   float r, float g, float b, float a,
                                   float scale) {
    draw_text(text, px + scale, py + scale, 0.0f, 0.0f, 0.0f, a * 0.6f, scale);
    draw_text(text, px, py, r, g, b, a, scale);
}

float UIRenderer::text_width(const char* text, float scale) const {
    if (!text) return 0.0f;
    float max_w = 0.0f;
    float cur_w = 0.0f;
    for (const char* ch = text; *ch; ++ch) {
        if (*ch == '\n') {
            if (cur_w > max_w) max_w = cur_w;
            cur_w = 0.0f;
        } else {
            cur_w += 6.0f * scale;
        }
    }
    if (cur_w > max_w) max_w = cur_w;
    return max_w;
}

float UIRenderer::text_height(float scale) const {
    return 7.0f * scale;
}

} // namespace swbf
