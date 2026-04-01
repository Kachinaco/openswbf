// OpenSWBF — main application entry point.
//
// Phase 0: Opens a window, renders a sky dome and a procedural terrain grid,
//          with a free-fly camera.  No game assets required.
//
// Phase 1: If --data-dir is provided, loads actual SWBF .lvl files and renders
//          terrain and models from the game data.

#include "platform/platform.h"
#include "renderer/backend/gl_context.h"
#include "renderer/backend/gl_shader.h"
#include "renderer/camera.h"
#include "renderer/sky_renderer.h"
#include "renderer/particle_system.h"
#include "renderer/particle_renderer.h"
#include "renderer/particle_effects.h"
#include "input/input_system.h"
#include "audio/audio_device.h"
#include "audio/audio_manager.h"
#include "core/log.h"
#include "core/filesystem.h"
#include "assets/lvl/lvl_loader.h"

// Game systems
#include "game/game_systems.h"
#include "game/entity_manager.h"
#include "game/spawn_system.h"
#include "game/command_post_system.h"
#include "game/conquest_mode.h"
#include "game/weapon_system.h"
#include "game/ai_system.h"
#include "game/health_system.h"
#include "game/vehicle_system.h"
#include "game/pathfinder.h"
#include "game/hud.h"
#include "game/scoreboard.h"
#include "game/menu.h"
#include "physics/physics_world.h"

// Scripting
#include "scripting/lua_runtime.h"
#include "scripting/swbf_api.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Forward declaration of the browser file-upload handler.
// Implemented after all anonymous-namespace helpers so it can access them.
#ifdef __EMSCRIPTEN__
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void load_lvl_data(const uint8_t* data, int size, const char* filename);

    EMSCRIPTEN_KEEPALIVE
    void load_lvl_file(const char* common_path, const char* map_path);
}
#endif

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// =========================================================================
// Procedural terrain renderer (self-contained for Phase 0)
//
// Generates a 128x128 grid with sine-wave hills, uploads it once, then
// draws it each frame with a simple per-vertex color shader.
// =========================================================================

namespace {

struct TerrainVertex {
    float x, y, z;      // position
    float nx, ny, nz;   // normal
    float r, g, b;      // color
};

// GL handles for the terrain mesh
uint32_t g_terrain_vao         = 0;
uint32_t g_terrain_vbo         = 0;
uint32_t g_terrain_ibo         = 0;
uint32_t g_terrain_index_count = 0;
uint32_t g_terrain_program     = 0;

// Terrain shader sources (GLSL ES 1.00 — WebGL 1)
const char* k_terrain_vert = R"(#version 100
precision highp float;

attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec3 a_color;

uniform mat4 u_view;
uniform mat4 u_projection;

varying vec3 v_color;
varying vec3 v_normal;
varying vec3 v_world_pos;

void main() {
    v_color     = a_color;
    v_normal    = a_normal;
    v_world_pos = a_position;
    gl_Position = u_projection * u_view * vec4(a_position, 1.0);
}
)";

const char* k_terrain_frag = R"(#version 100
precision highp float;

varying vec3 v_color;
varying vec3 v_normal;
varying vec3 v_world_pos;

void main() {
    // Simple directional sunlight from upper-right.
    vec3 sun_dir = normalize(vec3(0.4, 0.8, 0.3));
    float ndl    = max(dot(normalize(v_normal), sun_dir), 0.0);

    // Ambient + diffuse lighting.
    vec3 ambient = v_color * 0.35;
    vec3 diffuse = v_color * ndl * 0.65;

    // Slight distance fog toward sky color.
    float dist     = length(v_world_pos);
    float fog      = clamp(dist / 300.0, 0.0, 1.0);
    vec3  fog_color = vec3(0.6, 0.7, 0.9);

    vec3 lit = ambient + diffuse;
    lit = mix(lit, fog_color, fog * fog);

    gl_FragColor = vec4(lit, 1.0);
}
)";

// Height function: sum of several sine waves to create rolling hills.
float terrain_height(float x, float z) {
    float h = 0.0f;
    h += 4.0f  * std::sin(x * 0.02f) * std::cos(z * 0.03f);
    h += 2.0f  * std::sin(x * 0.07f + 1.0f) * std::cos(z * 0.05f + 2.0f);
    h += 1.0f  * std::sin(x * 0.15f + 3.0f) * std::sin(z * 0.12f);
    h += 0.5f  * std::cos(x * 0.25f) * std::sin(z * 0.20f + 1.5f);
    return h;
}

// Compile and link the terrain shader program.  Returns 0 on failure.
uint32_t compile_terrain_shader() {
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &k_terrain_vert, nullptr);
    glCompileShader(vert);
    {
        GLint ok = 0;
        glGetShaderiv(vert, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char info[512];
            glGetShaderInfoLog(vert, sizeof(info), nullptr, info);
            LOG_ERROR("Terrain vert shader error: %s", info);
            glDeleteShader(vert);
            return 0;
        }
    }

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &k_terrain_frag, nullptr);
    glCompileShader(frag);
    {
        GLint ok = 0;
        glGetShaderiv(frag, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char info[512];
            glGetShaderInfoLog(frag, sizeof(info), nullptr, info);
            LOG_ERROR("Terrain frag shader error: %s", info);
            glDeleteShader(vert);
            glDeleteShader(frag);
            return 0;
        }
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);

    // Bind attribute locations before linking (required for GLSL ES 1.00).
    glBindAttribLocation(prog, 0, "a_position");
    glBindAttribLocation(prog, 1, "a_normal");
    glBindAttribLocation(prog, 2, "a_color");

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
            LOG_ERROR("Terrain program link error: %s", info);
            glDeleteProgram(prog);
            return 0;
        }
    }

    return prog;
}

// Generate the procedural terrain mesh and upload to the GPU.
bool init_terrain() {
    g_terrain_program = compile_terrain_shader();
    if (!g_terrain_program) return false;

    constexpr int GRID = 128;          // vertices per side
    constexpr float SCALE = 2.0f;      // world units between vertices
    constexpr float HALF = (GRID - 1) * SCALE * 0.5f;

    // Generate vertices.
    std::vector<TerrainVertex> verts;
    verts.reserve(GRID * GRID);

    for (int row = 0; row < GRID; ++row) {
        for (int col = 0; col < GRID; ++col) {
            float wx = static_cast<float>(col) * SCALE - HALF;
            float wz = static_cast<float>(row) * SCALE - HALF;
            float wy = terrain_height(wx, wz);

            // Compute normal via central differences.
            float eps = SCALE * 0.5f;
            float hL = terrain_height(wx - eps, wz);
            float hR = terrain_height(wx + eps, wz);
            float hD = terrain_height(wx, wz - eps);
            float hU = terrain_height(wx, wz + eps);

            float nx = hL - hR;
            float ny = 2.0f * eps;
            float nz = hD - hU;
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 1e-6f) { nx /= len; ny /= len; nz /= len; }

            // Color: green grass tinted by height and slope.
            float slope = 1.0f - ny;  // 0 = flat, ~1 = steep
            float t = (wy + 8.0f) / 16.0f;  // normalised height ~[0,1]
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

            // Base green, darkening at low spots, browning on steep slopes.
            float r = 0.20f + 0.15f * t + 0.25f * slope;
            float g = 0.45f + 0.20f * t - 0.10f * slope;
            float b = 0.10f + 0.08f * t + 0.05f * slope;

            verts.push_back({wx, wy, wz, nx, ny, nz, r, g, b});
        }
    }

    // Generate triangle indices (two triangles per quad).
    std::vector<uint32_t> indices;
    indices.reserve((GRID - 1) * (GRID - 1) * 6);

    for (int row = 0; row < GRID - 1; ++row) {
        for (int col = 0; col < GRID - 1; ++col) {
            uint32_t tl = static_cast<uint32_t>(row * GRID + col);
            uint32_t tr = tl + 1;
            uint32_t bl = tl + static_cast<uint32_t>(GRID);
            uint32_t br = bl + 1;

            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);

            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    g_terrain_index_count = static_cast<uint32_t>(indices.size());

    // Upload to GPU.
    glGenVertexArrays(1, &g_terrain_vao);
    glGenBuffers(1, &g_terrain_vbo);
    glGenBuffers(1, &g_terrain_ibo);

    glBindVertexArray(g_terrain_vao);

    glBindBuffer(GL_ARRAY_BUFFER, g_terrain_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(TerrainVertex)),
                 verts.data(), GL_STATIC_DRAW);

    constexpr GLsizei stride = sizeof(TerrainVertex);

    // layout(location = 0) in vec3 a_position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(TerrainVertex, x)));

    // layout(location = 1) in vec3 a_normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(TerrainVertex, nx)));

    // layout(location = 2) in vec3 a_color
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(TerrainVertex, r)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_terrain_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                 indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    LOG_INFO("Terrain: generated %d x %d grid (%u triangles)",
             GRID, GRID, g_terrain_index_count / 3);
    return true;
}

// Draw the terrain with the given view/projection matrices.
void render_terrain(const float* view_matrix, const float* proj_matrix) {
    if (!g_terrain_program || !g_terrain_vao) return;

    glUseProgram(g_terrain_program);

    GLint loc_view = glGetUniformLocation(g_terrain_program, "u_view");
    GLint loc_proj = glGetUniformLocation(g_terrain_program, "u_projection");
    if (loc_view >= 0) glUniformMatrix4fv(loc_view, 1, GL_FALSE, view_matrix);
    if (loc_proj >= 0) glUniformMatrix4fv(loc_proj, 1, GL_FALSE, proj_matrix);

    glBindVertexArray(g_terrain_vao);
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(g_terrain_index_count),
                   GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    glUseProgram(0);
}

// Release terrain GPU resources.
void destroy_terrain() {
    if (g_terrain_ibo) { glDeleteBuffers(1, &g_terrain_ibo); g_terrain_ibo = 0; }
    if (g_terrain_vbo) { glDeleteBuffers(1, &g_terrain_vbo); g_terrain_vbo = 0; }
    if (g_terrain_vao) { glDeleteVertexArrays(1, &g_terrain_vao); g_terrain_vao = 0; }
    if (g_terrain_program) { glDeleteProgram(g_terrain_program); g_terrain_program = 0; }
}

} // anonymous namespace


// =========================================================================
// HUD text overlay — minimal on-screen text using GL quads.
//
// Renders bitmap-style text by drawing small quads for each "pixel" of a
// built-in 5x7 bitmap font.  Simple and self-contained, no external font
// files needed.
// =========================================================================

namespace {

uint32_t g_hud_program = 0;

const char* k_hud_vert = R"(#version 100
precision highp float;

attribute vec2 a_position;
attribute vec4 a_color;

uniform vec2 u_screen_size;

varying vec4 v_color;

void main() {
    // Convert pixel coords to NDC: (0,0) = top-left, (W,H) = bottom-right.
    vec2 ndc = vec2(
        (a_position.x / u_screen_size.x) *  2.0 - 1.0,
        (a_position.y / u_screen_size.y) * -2.0 + 1.0
    );
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_color = a_color;
}
)";

const char* k_hud_frag = R"(#version 100
precision highp float;

varying vec4 v_color;

void main() {
    gl_FragColor = v_color;
}
)";

// Minimal 5x7 bitmap font for printable ASCII (32-126).
// Each character is 5 columns x 7 rows, packed as 5 bytes (one per column,
// LSB = top row).
// clang-format off
const uint8_t k_font_5x7[][5] = {
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

// HUD vertex: 2D position + RGBA color.
struct HudVertex {
    float x, y;
    float r, g, b, a;
};

// Compile HUD shader.  Returns 0 on failure.
uint32_t compile_hud_shader() {
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &k_hud_vert, nullptr);
    glCompileShader(vert);
    {
        GLint ok = 0;
        glGetShaderiv(vert, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char info[512];
            glGetShaderInfoLog(vert, sizeof(info), nullptr, info);
            LOG_ERROR("HUD vert shader error: %s", info);
            glDeleteShader(vert);
            return 0;
        }
    }

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &k_hud_frag, nullptr);
    glCompileShader(frag);
    {
        GLint ok = 0;
        glGetShaderiv(frag, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char info[512];
            glGetShaderInfoLog(frag, sizeof(info), nullptr, info);
            LOG_ERROR("HUD frag shader error: %s", info);
            glDeleteShader(vert);
            glDeleteShader(frag);
            return 0;
        }
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);

    // Bind attribute locations before linking (required for GLSL ES 1.00).
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
            LOG_ERROR("HUD program link error: %s", info);
            glDeleteProgram(prog);
            return 0;
        }
    }

    return prog;
}

bool init_hud() {
    g_hud_program = compile_hud_shader();
    return g_hud_program != 0;
}

void destroy_hud() {
    if (g_hud_program) { glDeleteProgram(g_hud_program); g_hud_program = 0; }
}

// Draw a string of text at pixel position (px, py) with the given color
// and scale factor.  Builds a vertex array each call and draws it.
void draw_text(const char* text, float px, float py,
               float cr, float cg, float cb, float ca,
               float scale, int screen_w, int screen_h) {
    if (!g_hud_program || !text) return;

    std::vector<HudVertex> verts;
    float cursor_x = px;
    float cursor_y = py;
    float pixel_w = scale;
    float pixel_h = scale;

    for (const char* ch = text; *ch; ++ch) {
        if (*ch == '\n') {
            cursor_x = px;
            cursor_y += 9.0f * scale;  // 7 rows + 2 spacing
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
                    float x1 = x0 + pixel_w;
                    float y1 = y0 + pixel_h;

                    verts.push_back({x0, y0, cr, cg, cb, ca});
                    verts.push_back({x1, y0, cr, cg, cb, ca});
                    verts.push_back({x0, y1, cr, cg, cb, ca});

                    verts.push_back({x1, y0, cr, cg, cb, ca});
                    verts.push_back({x1, y1, cr, cg, cb, ca});
                    verts.push_back({x0, y1, cr, cg, cb, ca});
                }
            }
        }
        cursor_x += 6.0f * scale;  // 5 columns + 1 spacing
    }

    if (verts.empty()) return;

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(HudVertex)),
                 verts.data(), GL_STREAM_DRAW);

    constexpr GLsizei hud_stride = sizeof(HudVertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, hud_stride,
                          reinterpret_cast<const void*>(offsetof(HudVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, hud_stride,
                          reinterpret_cast<const void*>(offsetof(HudVertex, r)));

    glUseProgram(g_hud_program);
    GLint loc_screen = glGetUniformLocation(g_hud_program, "u_screen_size");
    if (loc_screen >= 0)
        glUniform2f(loc_screen,
                    static_cast<float>(screen_w),
                    static_cast<float>(screen_h));

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size()));

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glUseProgram(0);
}

} // anonymous namespace


// =========================================================================
// LVL model GPU upload — self-contained to avoid type conflicts between
// model_loader.h and model_types.h (both define swbf::Model/Vertex).
//
// We use the model_loader.h types from lvl_loader.h and upload directly
// to GL, bypassing MeshRenderer which expects model_types.h types.
// =========================================================================

namespace {

// GPU-side model segment for LVL-loaded models.
struct LvlGPUSegment {
    uint32_t vao         = 0;
    uint32_t vbo         = 0;
    uint32_t ibo         = 0;
    uint32_t index_count = 0;
    uint32_t texture     = 0;  // GL texture handle, 0 = none
};

struct LvlGPUModel {
    std::string name;
    std::vector<LvlGPUSegment> segments;
};

uint32_t g_model_program = 0;

// Shader for LVL models: position, normal, UV, vertex color.
const char* k_model_vert = R"(#version 100
precision highp float;

attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec2 a_uv;
attribute vec4 a_color;

uniform mat4 u_view;
uniform mat4 u_projection;

varying vec3 v_normal;
varying vec2 v_uv;
varying vec4 v_color;
varying vec3 v_world_pos;

void main() {
    v_normal    = a_normal;
    v_uv        = a_uv;
    v_color     = a_color;
    v_world_pos = a_position;
    gl_Position = u_projection * u_view * vec4(a_position, 1.0);
}
)";

const char* k_model_frag = R"(#version 100
precision highp float;

varying vec3 v_normal;
varying vec2 v_uv;
varying vec4 v_color;
varying vec3 v_world_pos;

uniform sampler2D u_texture;
uniform int u_has_texture;

void main() {
    vec3 sun_dir = normalize(vec3(0.4, 0.8, 0.3));
    float ndl = max(dot(normalize(v_normal), sun_dir), 0.0);

    vec4 base_color = v_color;
    if (u_has_texture == 1) {
        base_color *= texture2D(u_texture, v_uv);
    }

    vec3 ambient = base_color.rgb * 0.35;
    vec3 diffuse = base_color.rgb * ndl * 0.65;

    float dist = length(v_world_pos);
    float fog = clamp(dist / 500.0, 0.0, 1.0);
    vec3 fog_color = vec3(0.6, 0.7, 0.9);

    vec3 lit = ambient + diffuse;
    lit = mix(lit, fog_color, fog * fog);

    gl_FragColor = vec4(lit, base_color.a);
}
)";

// Per-vertex layout matching swbf::Vertex from model_loader.h:
//   float position[3], float normal[3], float uv[2], uint32_t color
struct LvlModelVertex {
    float position[3];
    float normal[3];
    float uv[2];
    float color[4];  // expanded to floats for the GL attribute
};

bool init_model_shader() {
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &k_model_vert, nullptr);
    glCompileShader(vert);
    {
        GLint ok = 0;
        glGetShaderiv(vert, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char info[512];
            glGetShaderInfoLog(vert, sizeof(info), nullptr, info);
            LOG_ERROR("Model vert shader error: %s", info);
            glDeleteShader(vert);
            return false;
        }
    }

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &k_model_frag, nullptr);
    glCompileShader(frag);
    {
        GLint ok = 0;
        glGetShaderiv(frag, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char info[512];
            glGetShaderInfoLog(frag, sizeof(info), nullptr, info);
            LOG_ERROR("Model frag shader error: %s", info);
            glDeleteShader(vert);
            glDeleteShader(frag);
            return false;
        }
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);

    // Bind attribute locations before linking (required for GLSL ES 1.00).
    glBindAttribLocation(prog, 0, "a_position");
    glBindAttribLocation(prog, 1, "a_normal");
    glBindAttribLocation(prog, 2, "a_uv");
    glBindAttribLocation(prog, 3, "a_color");

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
            LOG_ERROR("Model program link error: %s", info);
            glDeleteProgram(prog);
            return false;
        }
    }

    g_model_program = prog;
    return true;
}

void destroy_model_shader() {
    if (g_model_program) { glDeleteProgram(g_model_program); g_model_program = 0; }
}

// Upload a texture from LVL data to the GPU.  Returns GL texture handle.
uint32_t upload_texture(const swbf::Texture& tex) {
    if (tex.pixel_data.empty() || tex.width == 0 || tex.height == 0) return 0;

    GLuint handle = 0;
    glGenTextures(1, &handle);
    glBindTexture(GL_TEXTURE_2D, handle);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 static_cast<GLsizei>(tex.width),
                 static_cast<GLsizei>(tex.height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.pixel_data.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    return handle;
}

// Upload a model from the LVL loader to the GPU.
LvlGPUModel upload_lvl_model(
    const swbf::Model& model,
    const std::vector<swbf::Texture>& textures)
{
    LvlGPUModel gpu;
    gpu.name = model.name;

    // Build a texture name -> GL handle map.
    // We upload textures on demand and cache the handles.
    struct TexEntry { std::string name; uint32_t handle; };
    static std::vector<TexEntry> tex_cache;

    auto find_or_upload = [&](const std::string& name) -> uint32_t {
        if (name.empty()) return 0;
        // Check cache.
        for (const auto& e : tex_cache) {
            if (e.name == name) return e.handle;
        }
        // Find in loaded textures and upload.
        for (const auto& tex : textures) {
            if (tex.name == name && !tex.pixel_data.empty()) {
                uint32_t h = upload_texture(tex);
                if (h) {
                    tex_cache.push_back({name, h});
                    LOG_DEBUG("Uploaded texture: %s (%ux%u)", name.c_str(),
                              tex.width, tex.height);
                }
                return h;
            }
        }
        return 0;
    };

    for (const auto& seg : model.segments) {
        if (seg.vertices.empty() || seg.indices.empty()) continue;

        LvlGPUSegment gseg;
        gseg.index_count = static_cast<uint32_t>(seg.indices.size());

        // Look up texture for this segment's material.
        if (seg.material_index < model.materials.size()) {
            const auto& mat = model.materials[seg.material_index];
            gseg.texture = find_or_upload(mat.textures[0]);
        }

        // Convert vertices: expand packed uint32 color to float[4].
        std::vector<LvlModelVertex> verts;
        verts.reserve(seg.vertices.size());
        for (const auto& v : seg.vertices) {
            LvlModelVertex mv;
            mv.position[0] = v.position[0];
            mv.position[1] = v.position[1];
            mv.position[2] = v.position[2];
            mv.normal[0]   = v.normal[0];
            mv.normal[1]   = v.normal[1];
            mv.normal[2]   = v.normal[2];
            mv.uv[0]       = v.uv[0];
            mv.uv[1]       = v.uv[1];
            mv.color[0] = static_cast<float>((v.color >>  0) & 0xFF) / 255.0f;
            mv.color[1] = static_cast<float>((v.color >>  8) & 0xFF) / 255.0f;
            mv.color[2] = static_cast<float>((v.color >> 16) & 0xFF) / 255.0f;
            mv.color[3] = static_cast<float>((v.color >> 24) & 0xFF) / 255.0f;
            verts.push_back(mv);
        }

        // Upload to GPU.
        glGenVertexArrays(1, &gseg.vao);
        glGenBuffers(1, &gseg.vbo);
        glGenBuffers(1, &gseg.ibo);

        glBindVertexArray(gseg.vao);

        glBindBuffer(GL_ARRAY_BUFFER, gseg.vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(verts.size() * sizeof(LvlModelVertex)),
                     verts.data(), GL_STATIC_DRAW);

        constexpr GLsizei mvstride = sizeof(LvlModelVertex);

        // location 0: position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, mvstride,
                              reinterpret_cast<const void*>(offsetof(LvlModelVertex, position)));
        // location 1: normal
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, mvstride,
                              reinterpret_cast<const void*>(offsetof(LvlModelVertex, normal)));
        // location 2: uv
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, mvstride,
                              reinterpret_cast<const void*>(offsetof(LvlModelVertex, uv)));
        // location 3: color
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, mvstride,
                              reinterpret_cast<const void*>(offsetof(LvlModelVertex, color)));

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gseg.ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(seg.indices.size() * sizeof(uint16_t)),
                     seg.indices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);

        gpu.segments.push_back(gseg);
    }

    return gpu;
}

// Render all uploaded LVL models.
void render_lvl_models(const std::vector<LvlGPUModel>& models,
                       const float* view_matrix, const float* proj_matrix) {
    if (!g_model_program || models.empty()) return;

    glUseProgram(g_model_program);

    GLint loc_view    = glGetUniformLocation(g_model_program, "u_view");
    GLint loc_proj    = glGetUniformLocation(g_model_program, "u_projection");
    GLint loc_tex     = glGetUniformLocation(g_model_program, "u_texture");
    GLint loc_has_tex = glGetUniformLocation(g_model_program, "u_has_texture");

    if (loc_view >= 0) glUniformMatrix4fv(loc_view, 1, GL_FALSE, view_matrix);
    if (loc_proj >= 0) glUniformMatrix4fv(loc_proj, 1, GL_FALSE, proj_matrix);
    if (loc_tex  >= 0) glUniform1i(loc_tex, 0);  // texture unit 0

    for (const auto& model : models) {
        for (const auto& seg : model.segments) {
            if (seg.vao == 0 || seg.index_count == 0) continue;

            // Bind texture if available.
            if (seg.texture != 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, seg.texture);
                if (loc_has_tex >= 0) glUniform1i(loc_has_tex, 1);
            } else {
                if (loc_has_tex >= 0) glUniform1i(loc_has_tex, 0);
            }

            glBindVertexArray(seg.vao);
            glDrawElements(GL_TRIANGLES,
                           static_cast<GLsizei>(seg.index_count),
                           GL_UNSIGNED_SHORT, nullptr);
        }
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void destroy_lvl_models(std::vector<LvlGPUModel>& models) {
    for (auto& model : models) {
        for (auto& seg : model.segments) {
            if (seg.ibo) { glDeleteBuffers(1, &seg.ibo); seg.ibo = 0; }
            if (seg.vbo) { glDeleteBuffers(1, &seg.vbo); seg.vbo = 0; }
            if (seg.vao) { glDeleteVertexArrays(1, &seg.vao); seg.vao = 0; }
            // Note: textures are shared via cache; not deleted per-segment.
        }
    }
    models.clear();
}

} // anonymous namespace


// =========================================================================
// LVL terrain loading — converts TerrainData to the procedural terrain's
// VBO/IBO format so it can be rendered with the same shader.
// =========================================================================

namespace {

bool init_terrain_from_lvl(const swbf::TerrainData& td) {
    g_terrain_program = compile_terrain_shader();
    if (!g_terrain_program) return false;

    uint32_t gs = td.grid_size;
    if (gs == 0 || td.heights.empty()) {
        LOG_ERROR("LVL terrain data is empty (grid_size=%u)", gs);
        return false;
    }

    std::vector<TerrainVertex> verts;
    verts.reserve(static_cast<size_t>(gs) * gs);

    for (uint32_t row = 0; row < gs; ++row) {
        for (uint32_t col = 0; col < gs; ++col) {
            uint32_t idx = row * gs + col;

            float wx = (static_cast<float>(col) - static_cast<float>(gs) / 2.0f)
                       * td.grid_scale;
            float wz = (static_cast<float>(row) - static_cast<float>(gs) / 2.0f)
                       * td.grid_scale;
            float wy = td.heights[idx] * td.height_scale;

            // Compute normal via central differences.
            float hL = (col > 0)      ? td.heights[row * gs + (col - 1)] * td.height_scale : wy;
            float hR = (col < gs - 1) ? td.heights[row * gs + (col + 1)] * td.height_scale : wy;
            float hD = (row > 0)      ? td.heights[(row - 1) * gs + col] * td.height_scale : wy;
            float hU = (row < gs - 1) ? td.heights[(row + 1) * gs + col] * td.height_scale : wy;

            float nx = hL - hR;
            float ny = 2.0f * td.grid_scale;
            float nz = hD - hU;
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 1e-6f) { nx /= len; ny /= len; nz /= len; }

            // Color from vertex colors if available, otherwise height-based.
            float r = 0.35f, g = 0.55f, b = 0.15f;
            if (!td.colors.empty() && idx < static_cast<uint32_t>(td.colors.size())) {
                uint32_t rgba = td.colors[idx];
                r = static_cast<float>((rgba >>  0) & 0xFF) / 255.0f;
                g = static_cast<float>((rgba >>  8) & 0xFF) / 255.0f;
                b = static_cast<float>((rgba >> 16) & 0xFF) / 255.0f;
            } else {
                float t = (wy + 20.0f) / 40.0f;
                t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
                r = 0.20f + 0.15f * t;
                g = 0.45f + 0.20f * t;
                b = 0.10f + 0.08f * t;
            }

            verts.push_back({wx, wy, wz, nx, ny, nz, r, g, b});
        }
    }

    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(gs - 1) * (gs - 1) * 6);

    for (uint32_t row = 0; row < gs - 1; ++row) {
        for (uint32_t col = 0; col < gs - 1; ++col) {
            uint32_t tl = row * gs + col;
            uint32_t tr = tl + 1;
            uint32_t bl = tl + gs;
            uint32_t br = bl + 1;

            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);

            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    g_terrain_index_count = static_cast<uint32_t>(indices.size());

    glGenVertexArrays(1, &g_terrain_vao);
    glGenBuffers(1, &g_terrain_vbo);
    glGenBuffers(1, &g_terrain_ibo);

    glBindVertexArray(g_terrain_vao);

    glBindBuffer(GL_ARRAY_BUFFER, g_terrain_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(TerrainVertex)),
                 verts.data(), GL_STATIC_DRAW);

    constexpr GLsizei tstride = sizeof(TerrainVertex);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, tstride,
                          reinterpret_cast<const void*>(offsetof(TerrainVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, tstride,
                          reinterpret_cast<const void*>(offsetof(TerrainVertex, nx)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, tstride,
                          reinterpret_cast<const void*>(offsetof(TerrainVertex, r)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_terrain_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                 indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    LOG_INFO("LVL Terrain: uploaded %u x %u grid (%u triangles)",
             gs, gs, g_terrain_index_count / 3);
    return true;
}

} // anonymous namespace


// =========================================================================
// Application state — kept in a single struct so it can be accessed from
// the Emscripten main-loop callback.
// =========================================================================

struct AppState {
    swbf::GLContext          gl_context;
    swbf::InputSystem        input;
    swbf::AudioDevice        audio;
    swbf::AudioManager       audio_mgr;
    swbf::SkyRenderer        sky;
    swbf::Camera             camera;
    swbf::ParticleSystem     particles;
    swbf::ParticleRenderer   particle_renderer;
    swbf::PhysicsWorld       physics;

    // -- Game systems (integrated from all branches) ----------------------
    swbf::EntityManager      entity_manager;
    swbf::SpawnSystem        spawn_system;
    swbf::CommandPostSystem  command_posts;
    swbf::ConquestMode       conquest;
    swbf::WeaponSystem       weapon;
    swbf::AISystem           ai_system;
    swbf::HealthSystem       health;
    swbf::VehicleSystem      vehicles;
    swbf::Pathfinder         pathfinder;
    swbf::LuaRuntime         lua_runtime;
    swbf::VFS               vfs;

    // -- HUD / UI ---------------------------------------------------------
    swbf::HUD               game_hud;
    swbf::Scoreboard         scoreboard;
    swbf::MenuSystem         menu;
    swbf::Team               player_team = swbf::Team::REPUBLIC;
    bool                     game_mode_active = false;
    float                    sim_timer = 0.0f;

    // Timing
    uint32_t last_tick_ms     = 0;
    uint32_t fps_timer_ms     = 0;
    uint32_t frame_count      = 0;

    // Camera movement parameters
    static constexpr float MOVE_SPEED  = 30.0f;   // world units per second
    static constexpr float MOUSE_SENS  = 0.002f;   // radians per pixel

    bool running = true;

    // -- Vehicle state ----------------------------------------------------
    bool player_in_vehicle  = false;
    uint32_t player_vehicle = 0;

    // -- Phase 1: LVL asset loading state ---------------------------------
    std::string data_dir;            // --data-dir path, empty = procedural demo
    bool using_lvl_assets = false;   // true if LVL terrain/models were loaded

    // GPU models uploaded from LVL files.
    std::vector<LvlGPUModel> gpu_models;

    // HUD state.
    bool show_help = false;          // toggled by F1
    float hud_hint_timer = 5.0f;     // seconds to show "Press F1" hint

    // -- Particle demo state ----------------------------------------------
    float particle_demo_timer = 0.0f;  // timer for spawning demo effects

    // Status info.
    int loaded_textures = 0;
    int loaded_models   = 0;
    int loaded_terrains = 0;
    float last_fps      = 0.0f;
};

static AppState g_app;


// =========================================================================
// Per-frame tick — shared by native and Emscripten paths.
// =========================================================================

static void frame_tick() {
    // -- Delta time -------------------------------------------------------
#ifdef __EMSCRIPTEN__
    // emscripten_get_now() returns milliseconds as a double.
    static double s_prev_ms = emscripten_get_now();
    double now_ms = emscripten_get_now();
    float dt = static_cast<float>(now_ms - s_prev_ms) / 1000.0f;
    s_prev_ms = now_ms;
    uint32_t now_tick = static_cast<uint32_t>(now_ms);
#else
    uint32_t now_tick = SDL_GetTicks();
    float dt = static_cast<float>(now_tick - g_app.last_tick_ms) / 1000.0f;
#endif

    g_app.last_tick_ms = now_tick;

    // Clamp dt to avoid huge jumps (e.g. after a tab switch or breakpoint).
    if (dt > 0.1f) dt = 0.1f;
    if (dt < 0.0f) dt = 0.0f;

    // -- Input ------------------------------------------------------------
    g_app.input.update();

    if (g_app.input.quit_requested()) {
        g_app.running = false;
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
        return;
    }

    // Escape releases pointer lock.
    if (g_app.input.key_pressed(SDL_SCANCODE_ESCAPE)) {
        if (g_app.input.pointer_locked()) {
            g_app.input.release_pointer_lock();
        }
    }

    // Click to lock pointer (for mouse-look).
    if (g_app.input.mouse_clicked(SDL_BUTTON_LEFT)) {
        if (!g_app.input.pointer_locked()) {
            g_app.input.request_pointer_lock();
        }
    }

    // F1 toggles help overlay.
    if (g_app.input.key_pressed(SDL_SCANCODE_F1)) {
        g_app.show_help = !g_app.show_help;
    }

    // -- Camera movement --------------------------------------------------
    float speed = AppState::MOVE_SPEED * dt;

    // Hold Ctrl to go faster.
    if (g_app.input.key_down(SDL_SCANCODE_LCTRL) ||
        g_app.input.key_down(SDL_SCANCODE_RCTRL)) {
        speed *= 3.0f;
    }

    if (g_app.input.key_down(SDL_SCANCODE_W))
        g_app.camera.move_forward(speed);
    if (g_app.input.key_down(SDL_SCANCODE_S))
        g_app.camera.move_forward(-speed);
    if (g_app.input.key_down(SDL_SCANCODE_A))
        g_app.camera.move_right(-speed);
    if (g_app.input.key_down(SDL_SCANCODE_D))
        g_app.camera.move_right(speed);
    if (g_app.input.key_down(SDL_SCANCODE_SPACE))
        g_app.camera.move_up(speed);
    if (g_app.input.key_down(SDL_SCANCODE_LSHIFT) ||
        g_app.input.key_down(SDL_SCANCODE_RSHIFT))
        g_app.camera.move_up(-speed);

    // Mouse look (only when pointer is locked).
    if (g_app.input.pointer_locked()) {
        float dx = g_app.input.mouse_dx();
        float dy = g_app.input.mouse_dy();
        g_app.camera.rotate(-dy * AppState::MOUSE_SENS,
                             dx * AppState::MOUSE_SENS);
    }

    g_app.camera.update(dt);

    // -- Particle system update -------------------------------------------
    g_app.particles.update(dt);

    // -- Particle demo effects (P key to spawn explosion at camera) -------
    if (g_app.input.key_pressed(SDL_SCANCODE_P)) {
        swbf::effects::explosion(g_app.particles,
                                  g_app.camera.x(),
                                  g_app.camera.y() - 5.0f,
                                  g_app.camera.z() - 15.0f);
        LOG_INFO("Particle demo: explosion spawned");
    }
    // E key: sparks at a point ahead of the camera
    if (g_app.input.key_pressed(SDL_SCANCODE_E)) {
        swbf::effects::sparks(g_app.particles,
                               g_app.camera.x(),
                               g_app.camera.y() - 3.0f,
                               g_app.camera.z() - 10.0f,
                               0.0f, 1.0f, 0.0f);
        LOG_INFO("Particle demo: sparks spawned");
    }
    // M key: muzzle flash at a point ahead of the camera
    if (g_app.input.key_pressed(SDL_SCANCODE_M)) {
        swbf::effects::muzzle_flash(g_app.particles,
                                     g_app.camera.x(),
                                     g_app.camera.y(),
                                     g_app.camera.z() - 2.0f,
                                     0.0f, 0.0f, -1.0f);
        LOG_INFO("Particle demo: muzzle flash spawned");
    }

    // -- Window resize ----------------------------------------------------
    if (g_app.input.window_resized()) {
        int w = g_app.input.window_width();
        int h = g_app.input.window_height();
        g_app.gl_context.resize(w, h);
        float aspect = (h > 0) ? static_cast<float>(w) / static_cast<float>(h)
                                : 1.0f;
        g_app.camera.set_perspective(1.0472f, aspect, 0.1f, 2000.0f);
    }

    // -- Render -----------------------------------------------------------
    g_app.gl_context.begin_frame();

    const float* view = g_app.camera.view_matrix();
    const float* proj = g_app.camera.projection_matrix();

    // Sky dome first (renders behind everything).
    g_app.sky.render(view, proj);

    // Terrain (either procedural or LVL-loaded — same render path).
    render_terrain(view, proj);

    // LVL models (Phase 1).
    if (g_app.using_lvl_assets) {
        render_lvl_models(g_app.gpu_models, view, proj);
    }

    // Particle effects (rendered after opaque geometry, before HUD).
    g_app.particle_renderer.render(g_app.particles, view, proj);

    // -- HUD overlay ------------------------------------------------------
    int screen_w = g_app.gl_context.width();
    int screen_h = g_app.gl_context.height();

    if (g_app.hud_hint_timer > 0.0f) {
        g_app.hud_hint_timer -= dt;
    }

    // Show "Press F1 for help" hint during the first few seconds.
    if (!g_app.show_help && g_app.hud_hint_timer > 0.0f) {
        float alpha = g_app.hud_hint_timer < 1.0f ? g_app.hud_hint_timer : 1.0f;
        // Drop shadow.
        draw_text("Press F1 for help", 12.0f, 12.0f,
                  0.0f, 0.0f, 0.0f, alpha * 0.5f, 2.0f, screen_w, screen_h);
        draw_text("Press F1 for help", 10.0f, 10.0f,
                  1.0f, 1.0f, 1.0f, alpha, 2.0f, screen_w, screen_h);
    }

    // Full help overlay when F1 is toggled on.
    if (g_app.show_help) {
        const char* help_text =
            "OpenSWBF Controls\n"
            "\n"
            "WASD       Move camera\n"
            "Mouse      Look around (click to capture)\n"
            "Space      Move up\n"
            "Shift      Move down\n"
            "Ctrl       Move faster\n"
            "Escape     Release mouse\n"
            "F1         Toggle this help\n"
            "\n"
            "Particle Effects\n"
            "P          Spawn explosion\n"
            "E          Spawn sparks\n"
            "M          Muzzle flash\n";

        // Drop shadow then foreground text.
        draw_text(help_text, 12.0f, 12.0f,
                  0.0f, 0.0f, 0.0f, 0.6f, 2.0f, screen_w, screen_h);
        draw_text(help_text, 10.0f, 10.0f,
                  1.0f, 1.0f, 0.7f, 0.95f, 2.0f, screen_w, screen_h);

        // Status line at the bottom.
        char info_buf[256];
        if (g_app.using_lvl_assets) {
            std::snprintf(info_buf, sizeof(info_buf),
                "Data: %s\n"
                "Textures: %d  Models: %d  Terrains: %d\n"
                "FPS: %.0f",
                g_app.data_dir.c_str(),
                g_app.loaded_textures,
                g_app.loaded_models,
                g_app.loaded_terrains,
                static_cast<double>(g_app.last_fps));
        } else {
            std::snprintf(info_buf, sizeof(info_buf),
                "Mode: Procedural demo (no --data-dir)\n"
                "FPS: %.0f",
                static_cast<double>(g_app.last_fps));
        }
        float info_y = static_cast<float>(screen_h) - 60.0f;
        draw_text(info_buf, 12.0f, info_y + 2.0f,
                  0.0f, 0.0f, 0.0f, 0.5f, 2.0f, screen_w, screen_h);
        draw_text(info_buf, 10.0f, info_y,
                  0.7f, 0.9f, 1.0f, 0.9f, 2.0f, screen_w, screen_h);
    }

    g_app.gl_context.end_frame();

    // -- FPS counter (every 2 seconds) ------------------------------------
    g_app.frame_count++;
    uint32_t elapsed = now_tick - g_app.fps_timer_ms;
    if (elapsed >= 2000) {
        float fps = static_cast<float>(g_app.frame_count) * 1000.0f
                    / static_cast<float>(elapsed);
        g_app.last_fps = fps;
        LOG_INFO("FPS: %.1f", static_cast<double>(fps));
        g_app.frame_count  = 0;
        g_app.fps_timer_ms = now_tick;
    }

    // -- Update audio listener to match camera ----------------------------
    if (g_app.audio.is_initialized()) {
        g_app.audio.set_listener_position(
            g_app.camera.x(), g_app.camera.y(), g_app.camera.z());
    }
}


// =========================================================================
// Emscripten main-loop trampoline
// =========================================================================

#ifdef __EMSCRIPTEN__
static void em_main_loop() {
    frame_tick();
}
#endif


// =========================================================================
// Command-line parsing
// =========================================================================

static std::string parse_data_dir(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if ((std::strcmp(argv[i], "--data-dir") == 0 ||
             std::strcmp(argv[i], "-d") == 0) && i + 1 < argc) {
            return argv[i + 1];
        }
    }
    return {};
}

static void print_usage(const char* prog) {
    LOG_INFO("Usage: %s [options]", prog);
    LOG_INFO("  --data-dir <path>   Path to SWBF GameData directory");
    LOG_INFO("  -d <path>           Short form of --data-dir");
    LOG_INFO("%s", "");
    LOG_INFO("If --data-dir is not specified, runs the procedural terrain demo.");
}


// =========================================================================
// LVL asset loading sequence
// =========================================================================

static bool load_lvl_assets(const std::string& data_dir) {
    LOG_INFO("Loading SWBF assets from: %s", data_dir.c_str());

    swbf::VFS vfs;
    if (!vfs.mount(data_dir)) {
        LOG_ERROR("Failed to mount data directory: %s", data_dir.c_str());
        return false;
    }

    // Try common subdirectory layouts.
    vfs.mount(data_dir + "/data");
    vfs.mount(data_dir + "/Data");
    vfs.mount(data_dir + "/data/_lvl_pc");
    vfs.mount(data_dir + "/Data/_lvl_pc");

    // .lvl files to try loading, in priority order.
    struct LvlEntry {
        const char* path;
        const char* description;
    };

    const LvlEntry lvl_files[] = {
        {"common.lvl",        "Common assets"},
        {"nab/nab2.lvl",      "Naboo: Plains"},
        {"geo/geo1.lvl",      "Geonosis: Spire"},
        {"hot/hot1.lvl",      "Hoth: Echo Base"},
        {"end/end1.lvl",      "Endor: Bunker"},
        {"tat/tat2.lvl",      "Tatooine: Dune Sea"},
        {"kas/kas2.lvl",      "Kashyyyk: Docks"},
        {"kam/kam1.lvl",      "Kamino: Tipoca City"},
        {"bes/bes2.lvl",      "Bespin: Cloud City"},
        {"yav/yav1.lvl",      "Yavin 4: Temple"},
        {"ren/ren1.lvl",      "Rhen Var: Harbor"},
    };

    // Collect all loaded textures across files so models can reference them.
    std::vector<swbf::Texture> all_textures;

    int files_loaded = 0;

    for (const auto& entry : lvl_files) {
        std::string paths[] = {
            entry.path,
            std::string("_lvl_pc/") + entry.path,
            std::string("data/_lvl_pc/") + entry.path,
        };

        for (const auto& path : paths) {
            if (vfs.file_exists(path)) {
                LOG_INFO("Loading %s: %s", entry.description, path.c_str());
                swbf::LVLLoader loader;
                if (loader.load(path, vfs)) {
                    LOG_INFO("  Loaded: %zu textures, %zu models, %zu terrains",
                             loader.textures().size(),
                             loader.models().size(),
                             loader.terrains().size());

                    g_app.loaded_textures += static_cast<int>(loader.textures().size());
                    g_app.loaded_models   += static_cast<int>(loader.models().size());
                    g_app.loaded_terrains += static_cast<int>(loader.terrains().size());

                    // Accumulate textures for model material lookups.
                    for (const auto& tex : loader.textures()) {
                        all_textures.push_back(tex);
                    }

                    // Upload terrain if found and not yet uploaded.
                    if (!loader.terrains().empty() && g_terrain_vao == 0) {
                        const auto& td = loader.terrains()[0];
                        if (init_terrain_from_lvl(td)) {
                            LOG_INFO("  Terrain uploaded: %ux%u grid",
                                     td.grid_size, td.grid_size);
                        }
                    }

                    // Upload models.
                    for (const auto& model : loader.models()) {
                        if (model.segments.empty()) continue;
                        LvlGPUModel gpu = upload_lvl_model(model, all_textures);
                        if (!gpu.segments.empty()) {
                            LOG_INFO("  Model uploaded: %s (%zu segments)",
                                     model.name.c_str(), gpu.segments.size());
                            g_app.gpu_models.push_back(std::move(gpu));
                        }
                    }

                    files_loaded++;
                    break;  // Found and loaded this entry; move to next.
                } else {
                    LOG_WARN("  Failed to parse: %s", path.c_str());
                }
            }
        }
    }

    if (files_loaded == 0) {
        LOG_WARN("No .lvl files found in data directory.");
        LOG_WARN("Expected structure: <data-dir>/data/_lvl_pc/*.lvl");
        LOG_WARN("Falling back to procedural terrain demo.");
        return false;
    }

    LOG_INFO("LVL loading complete: %d files, %d textures, %d models, %d terrains",
             files_loaded, g_app.loaded_textures,
             g_app.loaded_models, g_app.loaded_terrains);
    return true;
}


// =========================================================================
// Browser file-upload handler (Emscripten only)
// =========================================================================

#ifdef __EMSCRIPTEN__
extern "C" {

EMSCRIPTEN_KEEPALIVE
void load_lvl_data(const uint8_t* data, int size, const char* filename) {
    LOG_INFO("load_lvl_data: received %d bytes from '%s'", size, filename);

    if (!data || size <= 0) {
        LOG_ERROR("load_lvl_data: invalid data (null or zero size)");
        return;
    }

    // Parse the raw bytes with LVLLoader.
    std::vector<uint8_t> buf(data, data + size);
    swbf::LVLLoader loader;
    if (!loader.load(buf)) {
        LOG_WARN("load_lvl_data: LVLLoader failed to parse '%s'", filename);
        // Even on failure, some assets may have been partially parsed --
        // fall through to check what was recovered.
    }

    LOG_INFO("load_lvl_data: parsed %zu textures, %zu models, %zu terrains",
             loader.textures().size(),
             loader.models().size(),
             loader.terrains().size());

    // --- Ensure the model shader is ready (first upload might need it) ---
    if (g_model_program == 0) {
        if (!init_model_shader()) {
            LOG_ERROR("load_lvl_data: failed to compile model shader");
        }
    }

    // --- Terrain: if we got terrain data, replace the current terrain ------
    if (!loader.terrains().empty()) {
        // Destroy old terrain buffers so init_terrain_from_lvl can recreate.
        destroy_terrain();

        const auto& td = loader.terrains()[0];
        if (init_terrain_from_lvl(td)) {
            LOG_INFO("load_lvl_data: terrain uploaded (%ux%u grid)",
                     td.grid_size, td.grid_size);
            g_app.loaded_terrains += static_cast<int>(loader.terrains().size());
        } else {
            LOG_WARN("load_lvl_data: terrain upload failed — regenerating procedural");
            init_terrain();
        }
    }

    // --- Textures: accumulate for model material lookups -------------------
    // We need a persistent collection so successive uploads can cross-ref.
    static std::vector<swbf::Texture> s_all_textures;

    for (const auto& tex : loader.textures()) {
        s_all_textures.push_back(tex);
    }
    g_app.loaded_textures += static_cast<int>(loader.textures().size());

    // --- Models: upload to GPU ---------------------------------------------
    for (const auto& model : loader.models()) {
        if (model.segments.empty()) continue;
        LvlGPUModel gpu = upload_lvl_model(model, s_all_textures);
        if (!gpu.segments.empty()) {
            LOG_INFO("load_lvl_data: model uploaded '%s' (%zu segments)",
                     model.name.c_str(), gpu.segments.size());
            g_app.gpu_models.push_back(std::move(gpu));
        }
    }
    g_app.loaded_models += static_cast<int>(loader.models().size());

    // Mark that we are now showing LVL assets.
    if (!loader.textures().empty() || !loader.models().empty() ||
        !loader.terrains().empty()) {
        g_app.using_lvl_assets = true;
    }

    LOG_INFO("load_lvl_data: done.  Totals: %d textures, %d models, %d terrains",
             g_app.loaded_textures, g_app.loaded_models, g_app.loaded_terrains);
}

EMSCRIPTEN_KEEPALIVE
void load_lvl_file(const char* common_path, const char* map_path) {
    LOG_INFO("load_lvl_file: common='%s' map='%s'", common_path, map_path);

    // Helper: read file from Emscripten virtual filesystem into a byte vector.
    auto read_vfs_file = [](const char* path) -> std::vector<uint8_t> {
        if (!path || path[0] == '\0') return {};

        FILE* f = std::fopen(path, "rb");
        if (!f) {
            LOG_WARN("load_lvl_file: cannot open '%s'", path);
            return {};
        }
        std::fseek(f, 0, SEEK_END);
        long sz = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (sz <= 0) { std::fclose(f); return {}; }

        std::vector<uint8_t> buf(static_cast<size_t>(sz));
        size_t got = std::fread(buf.data(), 1, buf.size(), f);
        std::fclose(f);
        buf.resize(got);
        LOG_INFO("load_lvl_file: read %zu bytes from '%s'", got, path);
        return buf;
    };

    // --- Ensure the model shader is ready ---
    if (g_model_program == 0) {
        if (!init_model_shader()) {
            LOG_ERROR("load_lvl_file: failed to compile model shader");
        }
    }

    // Persistent texture collection across loads for material lookups.
    static std::vector<swbf::Texture> s_all_textures;

    // Parse common.lvl first (if provided and non-empty path).
    if (common_path && common_path[0] != '\0') {
        auto common_data = read_vfs_file(common_path);
        if (!common_data.empty()) {
            swbf::LVLLoader common_loader;
            if (common_loader.load(common_data)) {
                LOG_INFO("load_lvl_file: common.lvl parsed: %zu textures, %zu models",
                         common_loader.textures().size(),
                         common_loader.models().size());

                for (const auto& tex : common_loader.textures()) {
                    s_all_textures.push_back(tex);
                }
                g_app.loaded_textures += static_cast<int>(common_loader.textures().size());

                for (const auto& model : common_loader.models()) {
                    if (model.segments.empty()) continue;
                    LvlGPUModel gpu = upload_lvl_model(model, s_all_textures);
                    if (!gpu.segments.empty()) {
                        g_app.gpu_models.push_back(std::move(gpu));
                    }
                }
                g_app.loaded_models += static_cast<int>(common_loader.models().size());
            } else {
                LOG_WARN("load_lvl_file: failed to parse common.lvl");
            }
        }
    }

    // Parse the map .lvl file.
    if (map_path && map_path[0] != '\0') {
        auto map_data = read_vfs_file(map_path);
        if (!map_data.empty()) {
            swbf::LVLLoader loader;
            if (!loader.load(map_data)) {
                LOG_WARN("load_lvl_file: LVLLoader returned false for '%s' "
                         "(partial data may still be available)", map_path);
            }

            LOG_INFO("load_lvl_file: map parsed: %zu textures, %zu models, %zu terrains",
                     loader.textures().size(),
                     loader.models().size(),
                     loader.terrains().size());

            // Terrain: replace current terrain with LVL terrain data.
            if (!loader.terrains().empty()) {
                destroy_terrain();
                const auto& td = loader.terrains()[0];
                if (init_terrain_from_lvl(td)) {
                    LOG_INFO("load_lvl_file: terrain uploaded (%ux%u grid)",
                             td.grid_size, td.grid_size);
                    g_app.loaded_terrains += static_cast<int>(loader.terrains().size());

                    // Adjust camera for potentially larger LVL terrain.
                    g_app.camera.set_position(0.0f, 50.0f, 100.0f);
                    g_app.camera.set_rotation(-0.3f, 0.0f);
                } else {
                    LOG_WARN("load_lvl_file: terrain upload failed, restoring procedural");
                    init_terrain();
                }
            }

            // Textures: accumulate for model material lookups.
            for (const auto& tex : loader.textures()) {
                s_all_textures.push_back(tex);
            }
            g_app.loaded_textures += static_cast<int>(loader.textures().size());

            // Models: upload to GPU.
            for (const auto& model : loader.models()) {
                if (model.segments.empty()) continue;
                LvlGPUModel gpu = upload_lvl_model(model, s_all_textures);
                if (!gpu.segments.empty()) {
                    LOG_INFO("load_lvl_file: model '%s' uploaded (%zu segments)",
                             model.name.c_str(), gpu.segments.size());
                    g_app.gpu_models.push_back(std::move(gpu));
                }
            }
            g_app.loaded_models += static_cast<int>(loader.models().size());
        } else {
            LOG_ERROR("load_lvl_file: could not read map file '%s'", map_path);
        }
    }

    // Mark that we are now showing LVL assets.
    g_app.using_lvl_assets = true;

    LOG_INFO("load_lvl_file: done. Totals: %d textures, %d models, %d terrains",
             g_app.loaded_textures, g_app.loaded_models, g_app.loaded_terrains);
}

} // extern "C"
#endif


// =========================================================================
// main
// =========================================================================

int main(int argc, char* argv[]) {
    swbf::log_set_level(swbf::LogLevel::DEBUG);
    LOG_INFO("OpenSWBF starting...");

    // -- Parse command line -----------------------------------------------
    g_app.data_dir = parse_data_dir(argc, argv);

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (!g_app.data_dir.empty()) {
        LOG_INFO("Data directory: %s", g_app.data_dir.c_str());
    } else {
        LOG_INFO("No --data-dir specified; using procedural demo.");
    }

    // -- Platform / GL context --------------------------------------------
    constexpr int INIT_W = 1280;
    constexpr int INIT_H = 720;

    if (!g_app.gl_context.init(INIT_W, INIT_H)) {
        LOG_ERROR("Failed to initialise GL context — aborting.");
        return 1;
    }

    // -- Input system -----------------------------------------------------
    g_app.input.init();

    // -- Audio (non-fatal if it fails) ------------------------------------
    if (!g_app.audio.init()) {
        LOG_WARN("Audio initialisation failed — continuing without sound.");
    }

    // -- Camera -----------------------------------------------------------
    g_app.camera.set_position(0.0f, 15.0f, 40.0f);
    g_app.camera.set_rotation(-0.25f, 0.0f);  // slight downward look

    float aspect = static_cast<float>(INIT_W) / static_cast<float>(INIT_H);
    g_app.camera.set_perspective(1.0472f, aspect, 0.1f, 2000.0f);

    // -- Sky renderer -----------------------------------------------------
    if (!g_app.sky.init()) {
        LOG_ERROR("Failed to initialise sky renderer — aborting.");
        return 1;
    }

    // -- Particle renderer ------------------------------------------------
    if (!g_app.particle_renderer.init()) {
        LOG_WARN("Failed to initialise particle renderer — continuing without particles.");
    }

    // -- Audio manager (high-level) ----------------------------------------
    if (!g_app.audio_mgr.init()) {
        LOG_WARN("AudioManager init failed — continuing without managed audio.");
    }

    // -- Game systems init ------------------------------------------------
    g_app.entity_manager.init();
    g_app.command_posts.init();
    g_app.ai_system.init(&g_app.entity_manager, &g_app.command_posts,
                         &g_app.pathfinder, &g_app.physics);
    g_app.pathfinder.init();
    g_app.vehicles.init();

    // -- Game systems service locator (for Lua API) -----------------------
    {
        auto& sys = swbf::GameSystems::instance();
        sys.entity_manager      = &g_app.entity_manager;
        sys.spawn_system        = &g_app.spawn_system;
        sys.command_post_system = &g_app.command_posts;
        sys.conquest_mode       = &g_app.conquest;
        sys.weapon_system       = &g_app.weapon;
        sys.ai_system           = &g_app.ai_system;
        sys.audio_device        = &g_app.audio;
        sys.camera              = &g_app.camera;
        sys.vfs                 = &g_app.vfs;
    }

    // -- Lua scripting runtime --------------------------------------------
    g_app.lua_runtime.init();
    swbf::register_swbf_api(g_app.lua_runtime);

    // -- HUD overlay ------------------------------------------------------
    if (!init_hud()) {
        LOG_WARN("Failed to initialise HUD — continuing without overlay.");
    }

    // -- Game HUD (Phase 2: full UI stack) --------------------------------
    if (!g_app.game_hud.init()) {
        LOG_WARN("Failed to initialise game HUD — continuing without game UI.");
    }

    // -- Menu system ------------------------------------------------------
    g_app.menu.init();

    // -- Asset loading (Phase 1 or procedural fallback) -------------------
    if (!g_app.data_dir.empty()) {
        // Initialize the model shader for LVL models.
        if (!init_model_shader()) {
            LOG_ERROR("Failed to initialise model shader — aborting.");
            return 1;
        }

        // Try to load LVL assets.
        if (load_lvl_assets(g_app.data_dir)) {
            g_app.using_lvl_assets = true;

            // If no terrain was loaded, fall back to procedural.
            if (g_terrain_vao == 0) {
                LOG_INFO("No terrain in LVL data; generating procedural terrain.");
                if (!init_terrain()) {
                    LOG_ERROR("Failed to initialise fallback terrain — aborting.");
                    return 1;
                }
            }

            // Adjust camera for LVL terrain which may be larger.
            g_app.camera.set_position(0.0f, 50.0f, 100.0f);
            g_app.camera.set_rotation(-0.3f, 0.0f);
        } else {
            // LVL loading failed — fall back to procedural demo.
            LOG_INFO("Falling back to procedural terrain demo.");
            if (!init_terrain()) {
                LOG_ERROR("Failed to initialise terrain — aborting.");
                return 1;
            }
        }
    } else {
        // No --data-dir: pure procedural demo (original Phase 0 behavior).
        if (!init_terrain()) {
            LOG_ERROR("Failed to initialise terrain — aborting.");
            return 1;
        }
    }

    if (g_app.using_lvl_assets) {
        LOG_INFO("OpenSWBF ready (LVL assets loaded).  "
                 "WASD to move, mouse to look, F1 for help.");
    } else {
        LOG_INFO("OpenSWBF ready (procedural demo).  "
                 "WASD to move, mouse to look, F1 for help.");
    }

    // -- Timing -----------------------------------------------------------
#ifdef __EMSCRIPTEN__
    g_app.fps_timer_ms = static_cast<uint32_t>(emscripten_get_now());
#else
    g_app.last_tick_ms = SDL_GetTicks();
    g_app.fps_timer_ms = g_app.last_tick_ms;
#endif

    // -- Main loop --------------------------------------------------------
#ifdef __EMSCRIPTEN__
    // 0 = use requestAnimationFrame; 1 = simulate infinite loop.
    emscripten_set_main_loop(em_main_loop, 0, 1);
#else
    while (g_app.running) {
        frame_tick();
    }
#endif

    // -- Cleanup ----------------------------------------------------------
    destroy_terrain();
    destroy_hud();
    g_app.game_hud.destroy();
    destroy_lvl_models(g_app.gpu_models);
    destroy_model_shader();
    g_app.particles.clear();
    g_app.particle_renderer.destroy();
    g_app.sky.destroy();

    // Shutdown game systems.
    g_app.lua_runtime.shutdown();
    swbf::GameSystems::instance().clear();
    g_app.vehicles.shutdown();
    g_app.pathfinder.shutdown();
    g_app.ai_system.shutdown();
    g_app.command_posts.shutdown();
    g_app.entity_manager.shutdown();
    g_app.spawn_system.clear();
    g_app.conquest.clear();
    g_app.weapon.clear();
    g_app.health.clear();

    g_app.audio_mgr.shutdown();
    g_app.audio.shutdown();

    LOG_INFO("OpenSWBF shut down cleanly.");
    return 0;
}
