#pragma once

#include "renderer/backend/gl_buffers.h"
#include "renderer/backend/gl_shader.h"
#include "renderer/particle_system.h"

#include <vector>

namespace swbf {

// ---------------------------------------------------------------------------
// ParticleVertex — per-vertex data for a billboard quad corner.
//
// Each particle becomes 4 vertices (a camera-facing quad).  The vertex
// shader offsets each corner using the camera's right/up vectors.
// ---------------------------------------------------------------------------

struct ParticleVertex {
    float px, py, pz;    // particle center position (world space)
    float offset_x;      // corner offset: -1 or +1 (multiplied by half-size in shader)
    float offset_y;      // corner offset: -1 or +1
    float size;           // particle size (world units)
    float r, g, b, a;    // interpolated color+alpha
};

// ---------------------------------------------------------------------------
// ParticleRenderer — batches all active particles into a dynamic VBO each
// frame and draws them as camera-facing billboards.
//
// Supports both alpha-blended and additive rendering modes.  Alpha-blended
// particles are sorted back-to-front before upload.
// ---------------------------------------------------------------------------

class ParticleRenderer {
public:
    ParticleRenderer() = default;
    ~ParticleRenderer() = default;

    // Non-copyable.
    ParticleRenderer(const ParticleRenderer&) = delete;
    ParticleRenderer& operator=(const ParticleRenderer&) = delete;

    /// Compile the particle shader and create GPU buffers.
    /// Must be called after an OpenGL context is current.
    /// Returns false on shader compilation failure.
    bool init();

    /// Render all particles in the given system.
    ///
    /// @param system       The particle system containing all emitters.
    /// @param view_matrix  4x4 column-major view matrix.
    /// @param proj_matrix  4x4 column-major projection matrix.
    void render(const ParticleSystem& system,
                const float* view_matrix,
                const float* proj_matrix);

    /// Release GPU resources.
    void destroy();

private:
    /// Build the vertex data for all particles in a single emitter,
    /// appending to the provided vertex vector.
    void build_vertices(const ParticleEmitter& emitter,
                        const float* view_matrix,
                        std::vector<ParticleVertex>& out_verts,
                        std::vector<u16>& out_indices) const;

    /// Flush (upload + draw) the accumulated vertex batch.
    void flush_batch(const std::vector<ParticleVertex>& verts,
                     const std::vector<u16>& indices,
                     ParticleBlendMode blend_mode);

    Shader       m_shader;
    VertexArray  m_vao;
    VertexBuffer m_vbo;
    IndexBuffer  m_ibo;

    // Pre-allocated capacity for the dynamic buffers.
    static constexpr u32 MAX_BATCH_PARTICLES = 4096;
    static constexpr u32 MAX_BATCH_VERTICES  = MAX_BATCH_PARTICLES * 4;
    static constexpr u32 MAX_BATCH_INDICES   = MAX_BATCH_PARTICLES * 6;

    bool m_initialized = false;
};

} // namespace swbf
