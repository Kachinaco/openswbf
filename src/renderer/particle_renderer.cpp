#include "renderer/particle_renderer.h"
#include "core/log.h"
#include "core/types.h"

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <SDL_opengl.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>

namespace swbf {

// ===========================================================================
// GLSL ES 1.00 shader sources for particle billboards
// ===========================================================================

static const char* k_particle_vert_src = R"glsl(#version 100
precision highp float;

// Per-vertex attributes
attribute vec3 a_center;       // particle center in world space
attribute vec2 a_offset;       // corner offset (-1 or +1 for x, y)
attribute float a_size;        // particle half-size in world units
attribute vec4 a_color;        // interpolated RGBA color

// Uniforms
uniform mat4 u_view;
uniform mat4 u_proj;
uniform vec3 u_camera_right;   // camera right vector (world space)
uniform vec3 u_camera_up;      // camera up vector (world space)

// Varyings
varying vec4 v_color;
varying vec2 v_uv;

void main() {
    // Billboard: offset the center by camera-aligned axes.
    vec3 world_pos = a_center
                   + u_camera_right * (a_offset.x * a_size)
                   + u_camera_up    * (a_offset.y * a_size);

    gl_Position = u_proj * u_view * vec4(world_pos, 1.0);

    v_color = a_color;

    // UV from offset: map (-1,-1)..(+1,+1) to (0,0)..(1,1).
    v_uv = a_offset * 0.5 + 0.5;
}
)glsl";

static const char* k_particle_frag_src = R"glsl(#version 100
precision mediump float;

varying vec4 v_color;
varying vec2 v_uv;

void main() {
    // Soft circular falloff: distance from center of quad.
    vec2 d = v_uv - vec2(0.5, 0.5);
    float dist = dot(d, d) * 4.0;  // 0 at center, 1 at edge

    // Smooth circular mask — particles appear as soft circles.
    float mask = 1.0 - smoothstep(0.6, 1.0, dist);

    gl_FragColor = vec4(v_color.rgb, v_color.a * mask);
}
)glsl";

// ===========================================================================
// ParticleRenderer implementation
// ===========================================================================

bool ParticleRenderer::init() {
    if (!m_shader.compile(k_particle_vert_src, k_particle_frag_src,
            {{0, "a_center"}, {1, "a_offset"}, {2, "a_size"}, {3, "a_color"}})) {
        LOG_ERROR("ParticleRenderer: failed to compile particle shader");
        return false;
    }

    // Create VAO.
    m_vao.create();
    m_vao.bind();

    // Create dynamic VBO with pre-allocated capacity.
    m_vbo.create(nullptr,
                 MAX_BATCH_VERTICES * sizeof(ParticleVertex),
                 GL_DYNAMIC_DRAW);

    // Create dynamic IBO with pre-allocated capacity.
    m_ibo.create(nullptr,
                 MAX_BATCH_INDICES * sizeof(u16),
                 GL_DYNAMIC_DRAW);

    // Configure vertex attributes.
    // ParticleVertex layout:
    //   float px, py, pz          — 12 bytes, offset 0   (a_center, location 0)
    //   float offset_x, offset_y  —  8 bytes, offset 12  (a_offset, location 1, vec2)
    //                               but we have offset_x at 12, offset_y at 16
    //   float size                —  4 bytes, offset 20  (a_size,   location 2)
    //   float r, g, b, a          — 16 bytes, offset 24  (a_color,  location 3)
    //   Total stride: 40 bytes

    static constexpr GLsizei stride = 40;

    // location 0: a_center — vec3
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(0));

    // location 1: a_offset — vec2
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(12));

    // location 2: a_size — float
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(20));

    // location 3: a_color — vec4
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(24));

    VertexArray::unbind();

    m_initialized = true;
    LOG_INFO("ParticleRenderer: initialized (max %u particles per batch)",
             MAX_BATCH_PARTICLES);
    return true;
}

void ParticleRenderer::render(const ParticleSystem& system,
                               const float* view_matrix,
                               const float* proj_matrix) {
    if (!m_initialized) return;

    const auto& emitters = system.emitters();
    if (emitters.empty()) return;

    // Extract camera right and up vectors from the view matrix.
    // The view matrix rows (stored as columns in column-major) contain:
    //   Column 0: right.x, up.x, -forward.x, 0
    //   Column 1: right.y, up.y, -forward.y, 0
    //   Column 2: right.z, up.z, -forward.z, 0
    // So: right  = (view[0], view[4], view[8])
    //     up     = (view[1], view[5], view[9])
    float cam_right[3] = { view_matrix[0], view_matrix[4], view_matrix[8]  };
    float cam_up[3]    = { view_matrix[1], view_matrix[5], view_matrix[9]  };

    // Extract camera position from view matrix for sorting.
    // For a view matrix V, the camera position is -V^T * translation column.
    // Faster: use the fact that cam_pos = -(R^T * t) where R is the rotation
    // part and t is column 3.
    float cam_pos[3];
    cam_pos[0] = -(view_matrix[0] * view_matrix[12] +
                   view_matrix[1] * view_matrix[13] +
                   view_matrix[2] * view_matrix[14]);
    cam_pos[1] = -(view_matrix[4] * view_matrix[12] +
                   view_matrix[5] * view_matrix[13] +
                   view_matrix[6] * view_matrix[14]);
    cam_pos[2] = -(view_matrix[8] * view_matrix[12] +
                   view_matrix[9] * view_matrix[13] +
                   view_matrix[10]* view_matrix[14]);

    // Bind shader and set uniforms that don't change between batches.
    m_shader.use();
    m_shader.set_mat4("u_view", view_matrix);
    m_shader.set_mat4("u_proj", proj_matrix);
    m_shader.set_vec3("u_camera_right", cam_right);
    m_shader.set_vec3("u_camera_up", cam_up);

    // Batch particles by blend mode.
    // Pass 1: alpha-blended particles (sorted back-to-front).
    // Pass 2: additive particles (order-independent, no sorting needed).
    for (int pass = 0; pass < 2; ++pass) {
        ParticleBlendMode target_mode = (pass == 0)
            ? ParticleBlendMode::Alpha
            : ParticleBlendMode::Additive;

        std::vector<ParticleVertex> verts;
        std::vector<u16> indices;
        verts.reserve(1024);
        indices.reserve(1536);

        for (const auto* emitter : emitters) {
            if (emitter->blend_mode() != target_mode) continue;
            if (emitter->active_count() == 0) continue;

            build_vertices(*emitter, view_matrix, verts, indices);
        }

        if (verts.empty()) continue;

        // Sort alpha-blended particles back-to-front.
        if (target_mode == ParticleBlendMode::Alpha) {
            // We need to sort by quad (groups of 4 vertices, 6 indices).
            u32 quad_count = static_cast<u32>(verts.size()) / 4;

            // Build a sort key array: (distance_sq, quad_index).
            struct SortKey {
                float dist_sq;
                u32   quad_idx;
            };
            std::vector<SortKey> keys(quad_count);
            for (u32 q = 0; q < quad_count; ++q) {
                const auto& v = verts[q * 4];  // center position
                float dx = v.px - cam_pos[0];
                float dy = v.py - cam_pos[1];
                float dz = v.pz - cam_pos[2];
                keys[q] = { dx*dx + dy*dy + dz*dz, q };
            }

            // Sort back-to-front (farthest first).
            std::sort(keys.begin(), keys.end(),
                [](const SortKey& a, const SortKey& b) {
                    return a.dist_sq > b.dist_sq;
                });

            // Rebuild sorted vertex and index arrays.
            std::vector<ParticleVertex> sorted_verts(verts.size());
            std::vector<u16> sorted_indices(indices.size());

            for (u32 i = 0; i < quad_count; ++i) {
                u32 src = keys[i].quad_idx;
                u32 dst = i;

                // Copy 4 vertices.
                std::memcpy(&sorted_verts[dst * 4], &verts[src * 4],
                            4 * sizeof(ParticleVertex));

                // Write 6 indices pointing to the new vertex positions.
                u16 base = static_cast<u16>(dst * 4);
                sorted_indices[dst * 6 + 0] = base + 0;
                sorted_indices[dst * 6 + 1] = base + 1;
                sorted_indices[dst * 6 + 2] = base + 2;
                sorted_indices[dst * 6 + 3] = base + 2;
                sorted_indices[dst * 6 + 4] = base + 1;
                sorted_indices[dst * 6 + 5] = base + 3;
            }

            verts   = std::move(sorted_verts);
            indices = std::move(sorted_indices);
        }

        flush_batch(verts, indices, target_mode);
    }
}

void ParticleRenderer::build_vertices(const ParticleEmitter& emitter,
                                       const float* /*view_matrix*/,
                                       std::vector<ParticleVertex>& out_verts,
                                       std::vector<u16>& out_indices) const {
    const Particle* pool = emitter.particles();
    u32 max_p = emitter.max_particles();

    for (u32 i = 0; i < max_p; ++i) {
        const Particle& p = pool[i];
        if (!p.alive) continue;

        // Interpolation factor: 0 at birth, 1 at death.
        float t = p.age / p.lifetime;
        if (t > 1.0f) t = 1.0f;

        // Interpolate size.
        float size = p.size_start + (p.size_end - p.size_start) * t;
        float half_size = size * 0.5f;

        // Interpolate color.
        float r = p.r0 + (p.r1 - p.r0) * t;
        float g = p.g0 + (p.g1 - p.g0) * t;
        float b = p.b0 + (p.b1 - p.b0) * t;
        float a = p.a0 + (p.a1 - p.a0) * t;

        // Check batch capacity.
        u32 current_quads = static_cast<u32>(out_verts.size()) / 4;
        if (current_quads >= MAX_BATCH_PARTICLES) break;

        // Base index for this quad.
        u16 base = static_cast<u16>(out_verts.size());

        // 4 corners of the billboard quad.
        // Offsets: (-1,-1), (+1,-1), (-1,+1), (+1,+1)
        out_verts.push_back({p.px, p.py, p.pz, -1.0f, -1.0f, half_size, r, g, b, a});
        out_verts.push_back({p.px, p.py, p.pz, +1.0f, -1.0f, half_size, r, g, b, a});
        out_verts.push_back({p.px, p.py, p.pz, -1.0f, +1.0f, half_size, r, g, b, a});
        out_verts.push_back({p.px, p.py, p.pz, +1.0f, +1.0f, half_size, r, g, b, a});

        // Two triangles: (0,1,2) and (2,1,3).
        out_indices.push_back(base + 0);
        out_indices.push_back(base + 1);
        out_indices.push_back(base + 2);
        out_indices.push_back(base + 2);
        out_indices.push_back(base + 1);
        out_indices.push_back(base + 3);
    }
}

void ParticleRenderer::flush_batch(const std::vector<ParticleVertex>& verts,
                                    const std::vector<u16>& indices,
                                    ParticleBlendMode blend_mode) {
    if (verts.empty()) return;

    // Set GL blending state.
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);  // Don't write to depth buffer for particles.

    if (blend_mode == ParticleBlendMode::Additive) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    } else {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // Upload vertex data to the dynamic VBO.
    m_vao.bind();

    std::size_t vert_bytes = verts.size() * sizeof(ParticleVertex);
    std::size_t idx_bytes  = indices.size() * sizeof(u16);

    // If the data fits in the pre-allocated buffer, use sub-data.
    // Otherwise, orphan and re-allocate.
    if (verts.size() <= MAX_BATCH_VERTICES) {
        m_vbo.update(verts.data(), vert_bytes);
    } else {
        m_vbo.bind();
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vert_bytes),
                     verts.data(), GL_DYNAMIC_DRAW);
    }

    if (indices.size() <= MAX_BATCH_INDICES) {
        m_ibo.bind();
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                        static_cast<GLsizeiptr>(idx_bytes), indices.data());
    } else {
        m_ibo.bind();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(idx_bytes),
                     indices.data(), GL_DYNAMIC_DRAW);
    }

    // Draw.
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(indices.size()),
                   GL_UNSIGNED_SHORT,
                   nullptr);

    VertexArray::unbind();

    // Restore state.
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void ParticleRenderer::destroy() {
    m_shader = Shader{};
    m_vao.destroy();
    m_vbo.destroy();
    m_ibo.destroy();
    m_initialized = false;
    LOG_INFO("ParticleRenderer: destroyed");
}

} // namespace swbf
