#pragma once

#include "renderer/backend/gl_shader.h"
#include "renderer/backend/gl_buffers.h"
#include "renderer/backend/gl_texture.h"
#include "assets/lvl/terrain_loader.h"
#include "core/types.h"

namespace swbf {

// ---------------------------------------------------------------------------
// TerrainRenderer — renders heightmap terrain for SWBF (2004) maps
//
// SWBF terrain is a regular grid (typically 128x128) with per-vertex heights,
// vertex colors (baked lighting), and up to 16 texture layers blended via
// per-vertex weights.
//
// The renderer works in two modes:
//   1. Vertex-color-only: if no textures are loaded, the terrain is drawn
//      using just vertex colors. This lets you see terrain even without
//      extracting game assets.
//   2. Textured: up to 4 splatmap texture layers are blended per-fragment
//      using per-vertex weights, then multiplied by vertex color.
//
// Vertex format (per vertex, 52 bytes):
//   position   : vec3  (12 bytes, offset  0) — world-space x/y/z
//   normal     : vec3  (12 bytes, offset 12) — face normal for lighting
//   color      : u32   ( 4 bytes, offset 24) — RGBA8 packed vertex color
//   tex_weights: vec4  (16 bytes, offset 28) — blend weights for layers 0-3
//   texcoord   : vec2  ( 8 bytes, offset 44) — UV for texture tiling
// ---------------------------------------------------------------------------

class TerrainRenderer {
public:
    TerrainRenderer() = default;
    ~TerrainRenderer() = default;

    // Non-copyable, non-movable (owns GL resources).
    TerrainRenderer(const TerrainRenderer&) = delete;
    TerrainRenderer& operator=(const TerrainRenderer&) = delete;

    /// Compile terrain shaders. Must be called after GL context is created.
    /// Returns true on success.
    bool init();

    /// Upload terrain geometry to the GPU from parsed TerrainData.
    /// Builds vertex and index buffers. Can be called again to reload terrain.
    void upload(const TerrainData& terrain);

    /// Render the terrain.
    /// @param view_matrix  pointer to 16 floats (column-major 4x4 view matrix)
    /// @param proj_matrix  pointer to 16 floats (column-major 4x4 projection matrix)
    void render(const float* view_matrix, const float* proj_matrix);

    /// Release all GPU resources.
    void destroy();

    /// Load a texture into one of the 16 splatmap slots.
    /// @param layer  texture layer index [0..15]
    /// @param width  texture width in pixels
    /// @param height texture height in pixels
    /// @param pixels RGBA8 pixel data
    void set_texture(u32 layer, u32 width, u32 height, const void* pixels);

    /// Returns true if upload() has been called and geometry is ready.
    bool has_terrain() const { return m_index_count > 0; }

private:
    /// Packed vertex data uploaded to the GPU.
    struct TerrainVertex {
        float px, py, pz;       // position
        float nx, ny, nz;       // normal
        u32   color;            // RGBA8 packed
        float tw0, tw1, tw2, tw3; // texture blend weights (layers 0-3)
        float u, v;             // texture coordinates
    };

    static_assert(sizeof(TerrainVertex) == 52,
                  "TerrainVertex must be 52 bytes for correct GPU layout");

    /// Compute a face normal for the triangle formed by three positions.
    static void compute_normal(float ax, float ay, float az,
                               float bx, float by, float bz,
                               float cx, float cy, float cz,
                               float& out_nx, float& out_ny, float& out_nz);

    /// Pack RGBA8 color into a single u32 (for glVertexAttribPointer with
    /// GL_UNSIGNED_BYTE + normalized).
    static u32 pack_color(u8 r, u8 g, u8 b, u8 a);

    Shader       m_shader;
    VertexArray  m_vao;
    VertexBuffer m_vbo;
    IndexBuffer  m_ibo;
    u32          m_index_count = 0;
    GLTexture    m_textures[16];    // splatmap textures
    u32          m_texture_count = 0; // number of valid textures loaded
};

} // namespace swbf
