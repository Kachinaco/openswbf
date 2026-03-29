// OpenSWBF — main application entry point.
//
// Phase 0: Opens a window, renders a sky dome and a procedural terrain grid,
//          with a free-fly camera.  No game assets required.
//
// Phase 1 (future): Load actual SWBF .lvl files and render terrain/models.

#include "platform/platform.h"
#include "renderer/backend/gl_context.h"
#include "renderer/backend/gl_shader.h"
#include "renderer/camera.h"
#include "renderer/sky_renderer.h"
#include "input/input_system.h"
#include "audio/audio_device.h"
#include "core/log.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <cmath>
#include <cstdint>
#include <cstdio>
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

// Terrain shader sources (GLSL ES 3.00)
const char* k_terrain_vert = R"(#version 300 es
precision highp float;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_color;

uniform mat4 u_view;
uniform mat4 u_projection;

out vec3 v_color;
out vec3 v_normal;
out vec3 v_world_pos;

void main() {
    v_color     = a_color;
    v_normal    = a_normal;
    v_world_pos = a_position;
    gl_Position = u_projection * u_view * vec4(a_position, 1.0);
}
)";

const char* k_terrain_frag = R"(#version 300 es
precision highp float;

in vec3 v_color;
in vec3 v_normal;
in vec3 v_world_pos;

out vec4 frag_color;

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

    frag_color = vec4(lit, 1.0);
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
            float wx = col * SCALE - HALF;
            float wz = row * SCALE - HALF;
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
// Application state — kept in a single struct so it can be accessed from
// the Emscripten main-loop callback.
// =========================================================================

struct AppState {
    swbf::GLContext    gl_context;
    swbf::InputSystem  input;
    swbf::AudioDevice  audio;
    swbf::SkyRenderer  sky;
    swbf::Camera       camera;

    // Timing
    uint32_t last_tick_ms     = 0;
    uint32_t fps_timer_ms     = 0;
    uint32_t frame_count      = 0;

    // Camera movement parameters
    static constexpr float MOVE_SPEED  = 30.0f;   // world units per second
    static constexpr float MOUSE_SENS  = 0.002f;   // radians per pixel

    bool running = true;
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

    // Terrain.
    render_terrain(view, proj);

    g_app.gl_context.end_frame();

    // -- FPS counter (every 2 seconds) ------------------------------------
    g_app.frame_count++;
    uint32_t elapsed = now_tick - g_app.fps_timer_ms;
    if (elapsed >= 2000) {
        float fps = static_cast<float>(g_app.frame_count) * 1000.0f
                    / static_cast<float>(elapsed);
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
// main
// =========================================================================

int main(int /*argc*/, char* /*argv*/[]) {
    swbf::log_set_level(swbf::LogLevel::DEBUG);
    LOG_INFO("OpenSWBF Phase 0 starting...");

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

    // -- Procedural terrain -----------------------------------------------
    if (!init_terrain()) {
        LOG_ERROR("Failed to initialise terrain — aborting.");
        return 1;
    }

    LOG_INFO("OpenSWBF Phase 0 ready.  WASD to move, mouse to look, "
             "click to capture pointer, Escape to release.");

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
    g_app.sky.destroy();
    g_app.audio.shutdown();

    LOG_INFO("OpenSWBF shut down cleanly.");
    return 0;
}
