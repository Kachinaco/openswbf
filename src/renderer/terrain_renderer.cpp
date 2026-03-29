#include "renderer/terrain_renderer.h"
#include "core/log.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace swbf {

// ===========================================================================
// GLSL ES 3.0 shaders (embedded as string literals)
// ===========================================================================

static const char* k_terrain_vert_src = R"glsl(#version 300 es
precision mediump float;

// Per-vertex attributes
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color;       // vertex color (normalized u8 -> float)
layout(location = 3) in vec4 a_tex_weights; // blend weights for texture layers 0-3
layout(location = 4) in vec2 a_texcoord;

// Uniforms
uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

// Varyings passed to fragment shader
out vec3 v_normal;
out vec4 v_color;
out vec4 v_tex_weights;
out vec2 v_texcoord;

void main() {
    mat4 mvp = u_proj * u_view * u_model;
    gl_Position = mvp * vec4(a_position, 1.0);

    // Transform normal by model matrix (assuming uniform scale / no shear).
    v_normal = mat3(u_model) * a_normal;
    v_color = a_color;
    v_tex_weights = a_tex_weights;
    v_texcoord = a_texcoord;
}
)glsl";

static const char* k_terrain_frag_src = R"glsl(#version 300 es
precision mediump float;

in vec3 v_normal;
in vec4 v_color;
in vec4 v_tex_weights;
in vec2 v_texcoord;

// Splatmap texture layers (up to 4 active)
uniform sampler2D u_tex0;
uniform sampler2D u_tex1;
uniform sampler2D u_tex2;
uniform sampler2D u_tex3;
uniform int u_texture_count; // 0 = vertex-color-only mode

// Simple directional light for basic shading
uniform vec3 u_light_dir; // normalized, world-space

out vec4 frag_color;

void main() {
    vec3 normal = normalize(v_normal);

    // Basic half-Lambert lighting to avoid fully black backfaces.
    float ndotl = dot(normal, u_light_dir);
    float light = ndotl * 0.5 + 0.5; // remap [-1,1] -> [0,1]

    vec4 base_color;

    if (u_texture_count > 0) {
        // Blend up to 4 texture layers using per-vertex weights.
        vec4 tex_color = vec4(0.0);
        float total_weight = v_tex_weights.x + v_tex_weights.y
                           + v_tex_weights.z + v_tex_weights.w;

        if (total_weight > 0.001) {
            tex_color += v_tex_weights.x * texture(u_tex0, v_texcoord);
            if (u_texture_count > 1)
                tex_color += v_tex_weights.y * texture(u_tex1, v_texcoord);
            if (u_texture_count > 2)
                tex_color += v_tex_weights.z * texture(u_tex2, v_texcoord);
            if (u_texture_count > 3)
                tex_color += v_tex_weights.w * texture(u_tex3, v_texcoord);
            tex_color /= total_weight;
        } else {
            // No weights — fall back to first texture.
            tex_color = texture(u_tex0, v_texcoord);
        }

        // Multiply blended texture by vertex color (baked lighting).
        base_color = tex_color * v_color;
    } else {
        // No textures loaded — use vertex color directly.
        base_color = v_color;
    }

    // Apply lighting.
    vec3 lit = base_color.rgb * light;
    frag_color = vec4(lit, base_color.a);
}
)glsl";

// ===========================================================================
// Helper functions
// ===========================================================================

void TerrainRenderer::compute_normal(float ax, float ay, float az,
                                     float bx, float by, float bz,
                                     float cx, float cy, float cz,
                                     float& out_nx, float& out_ny, float& out_nz) {
    // Edge vectors: AB and AC.
    float abx = bx - ax, aby = by - ay, abz = bz - az;
    float acx = cx - ax, acy = cy - ay, acz = cz - az;

    // Cross product AB x AC.
    out_nx = aby * acz - abz * acy;
    out_ny = abz * acx - abx * acz;
    out_nz = abx * acy - aby * acx;

    // Normalize.
    float len = std::sqrt(out_nx * out_nx + out_ny * out_ny + out_nz * out_nz);
    if (len > 1e-8f) {
        float inv = 1.0f / len;
        out_nx *= inv;
        out_ny *= inv;
        out_nz *= inv;
    } else {
        out_nx = 0.0f;
        out_ny = 1.0f;
        out_nz = 0.0f;
    }
}

u32 TerrainRenderer::pack_color(u8 r, u8 g, u8 b, u8 a) {
    // Pack as RGBA in little-endian byte order for GL_UNSIGNED_BYTE.
    return static_cast<u32>(r)
         | (static_cast<u32>(g) << 8)
         | (static_cast<u32>(b) << 16)
         | (static_cast<u32>(a) << 24);
}

// ===========================================================================
// Public interface
// ===========================================================================

bool TerrainRenderer::init() {
    LOG_INFO("TerrainRenderer: compiling shaders...");

    if (!m_shader.compile(k_terrain_vert_src, k_terrain_frag_src)) {
        LOG_ERROR("TerrainRenderer: failed to compile terrain shaders");
        return false;
    }

    // Set default light direction (sun from upper-right).
    m_shader.bind();
    float light_dir[3] = { 0.4f, 0.8f, 0.3f };
    // Normalize.
    float len = std::sqrt(light_dir[0] * light_dir[0]
                        + light_dir[1] * light_dir[1]
                        + light_dir[2] * light_dir[2]);
    if (len > 0.0f) {
        light_dir[0] /= len;
        light_dir[1] /= len;
        light_dir[2] /= len;
    }
    m_shader.set_vec3("u_light_dir", light_dir);

    // Set default model matrix to identity.
    float identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    m_shader.set_mat4("u_model", identity);

    // Bind texture sampler uniforms to texture units.
    m_shader.set_int("u_tex0", 0);
    m_shader.set_int("u_tex1", 1);
    m_shader.set_int("u_tex2", 2);
    m_shader.set_int("u_tex3", 3);
    m_shader.set_int("u_texture_count", 0);

    Shader::unbind();

    LOG_INFO("TerrainRenderer: initialization complete");
    return true;
}

void TerrainRenderer::upload(const TerrainData& terrain) {
    LOG_INFO("TerrainRenderer: uploading terrain (%u x %u, scale %.2f)",
             terrain.grid_size, terrain.grid_size, terrain.grid_scale);

    const u32 grid = terrain.grid_size;
    const float scale = terrain.grid_scale;
    const u32 vert_count = grid * grid;

    if (terrain.heights.size() < vert_count) {
        LOG_ERROR("TerrainRenderer: height data too small (%zu < %u)",
                  terrain.heights.size(), vert_count);
        return;
    }

    // ------------------------------------------------------------------
    // Build vertex data
    // ------------------------------------------------------------------
    std::vector<TerrainVertex> vertices(vert_count);

    // First pass: set positions, colors, texture weights, and UVs.
    for (u32 row = 0; row < grid; ++row) {
        for (u32 col = 0; col < grid; ++col) {
            u32 idx = row * grid + col;
            TerrainVertex& v = vertices[idx];

            // Position: x = col * scale, y = height, z = row * scale.
            v.px = static_cast<float>(col) * scale;
            v.py = terrain.heights[idx];
            v.pz = static_cast<float>(row) * scale;

            // Vertex color — stored as packed RGBA u32 in TerrainData.
            if (idx < terrain.colors.size()) {
                // TerrainData stores colors as packed u32 (RGBA byte order).
                // Re-pack through our helper to ensure consistent GPU layout.
                u32 c = terrain.colors[idx];
                u8 cr = static_cast<u8>((c >>  0) & 0xFF);
                u8 cg = static_cast<u8>((c >>  8) & 0xFF);
                u8 cb = static_cast<u8>((c >> 16) & 0xFF);
                u8 ca = static_cast<u8>((c >> 24) & 0xFF);
                v.color = pack_color(cr, cg, cb, ca);
            } else {
                // Default: white (fully lit).
                v.color = pack_color(255, 255, 255, 255);
            }

            // Texture blend weights (first 4 layers).
            // TerrainData stores weights as vector<vector<uint8_t>>,
            // each layer containing grid_size^2 bytes (0-255).
            // Convert to float [0..1] for the shader.
            v.tw0 = 0.0f;
            v.tw1 = 0.0f;
            v.tw2 = 0.0f;
            v.tw3 = 0.0f;

            if (terrain.texture_weights.size() > 0 && idx < terrain.texture_weights[0].size())
                v.tw0 = terrain.texture_weights[0][idx] / 255.0f;
            if (terrain.texture_weights.size() > 1 && idx < terrain.texture_weights[1].size())
                v.tw1 = terrain.texture_weights[1][idx] / 255.0f;
            if (terrain.texture_weights.size() > 2 && idx < terrain.texture_weights[2].size())
                v.tw2 = terrain.texture_weights[2][idx] / 255.0f;
            if (terrain.texture_weights.size() > 3 && idx < terrain.texture_weights[3].size())
                v.tw3 = terrain.texture_weights[3][idx] / 255.0f;

            // Texture coordinates: tile based on grid position.
            // Use a reasonable tiling factor so textures repeat across the terrain.
            const float tex_tile = 8.0f; // texture repeats per terrain side
            v.u = static_cast<float>(col) / static_cast<float>(grid - 1) * tex_tile;
            v.v = static_cast<float>(row) / static_cast<float>(grid - 1) * tex_tile;

            // Normal will be computed in the second pass (accumulated from
            // adjacent face normals then normalized). Start at zero.
            v.nx = 0.0f;
            v.ny = 0.0f;
            v.nz = 0.0f;
        }
    }

    // Second pass: compute smooth normals by averaging face normals of
    // adjacent triangles. For each cell, two triangles contribute normals
    // to their four corner vertices.
    for (u32 row = 0; row < grid - 1; ++row) {
        for (u32 col = 0; col < grid - 1; ++col) {
            // Cell corners (row-major indices).
            u32 tl = row * grid + col;           // top-left
            u32 tr = row * grid + col + 1;       // top-right
            u32 bl = (row + 1) * grid + col;     // bottom-left
            u32 br = (row + 1) * grid + col + 1; // bottom-right

            // Triangle 1: TL -> BL -> TR
            float n1x, n1y, n1z;
            compute_normal(vertices[tl].px, vertices[tl].py, vertices[tl].pz,
                           vertices[bl].px, vertices[bl].py, vertices[bl].pz,
                           vertices[tr].px, vertices[tr].py, vertices[tr].pz,
                           n1x, n1y, n1z);

            // Triangle 2: TR -> BL -> BR
            float n2x, n2y, n2z;
            compute_normal(vertices[tr].px, vertices[tr].py, vertices[tr].pz,
                           vertices[bl].px, vertices[bl].py, vertices[bl].pz,
                           vertices[br].px, vertices[br].py, vertices[br].pz,
                           n2x, n2y, n2z);

            // Accumulate normals into the four corner vertices.
            // Triangle 1 contributes to TL, BL, TR.
            vertices[tl].nx += n1x; vertices[tl].ny += n1y; vertices[tl].nz += n1z;
            vertices[bl].nx += n1x; vertices[bl].ny += n1y; vertices[bl].nz += n1z;
            vertices[tr].nx += n1x; vertices[tr].ny += n1y; vertices[tr].nz += n1z;

            // Triangle 2 contributes to TR, BL, BR.
            vertices[tr].nx += n2x; vertices[tr].ny += n2y; vertices[tr].nz += n2z;
            vertices[bl].nx += n2x; vertices[bl].ny += n2y; vertices[bl].nz += n2z;
            vertices[br].nx += n2x; vertices[br].ny += n2y; vertices[br].nz += n2z;
        }
    }

    // Normalize accumulated normals.
    for (u32 i = 0; i < vert_count; ++i) {
        TerrainVertex& v = vertices[i];
        float len = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
        if (len > 1e-8f) {
            float inv = 1.0f / len;
            v.nx *= inv;
            v.ny *= inv;
            v.nz *= inv;
        } else {
            v.nx = 0.0f;
            v.ny = 1.0f;
            v.nz = 0.0f;
        }
    }

    // ------------------------------------------------------------------
    // Build index buffer
    // ------------------------------------------------------------------
    // For each cell (row, col) in the (grid-1) x (grid-1) cell grid,
    // generate 2 triangles (6 indices).
    u32 cell_count = (grid - 1) * (grid - 1);
    m_index_count = cell_count * 6;

    // Use 32-bit indices for grids larger than 255x255 (>65535 vertices).
    std::vector<u32> indices;
    indices.reserve(m_index_count);

    for (u32 row = 0; row < grid - 1; ++row) {
        for (u32 col = 0; col < grid - 1; ++col) {
            u32 tl = row * grid + col;
            u32 tr = row * grid + col + 1;
            u32 bl = (row + 1) * grid + col;
            u32 br = (row + 1) * grid + col + 1;

            // Triangle 1: TL -> BL -> TR (CCW winding, Y-up).
            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);

            // Triangle 2: TR -> BL -> BR.
            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    // ------------------------------------------------------------------
    // Upload to GPU
    // ------------------------------------------------------------------

    // Destroy previous buffers if re-uploading.
    m_vao.destroy();
    m_vbo.destroy();
    m_ibo.destroy();

    m_vao.create();
    m_vao.bind();

    m_vbo.create(vertices.data(), vertices.size() * sizeof(TerrainVertex));
    m_ibo.create(indices.data(), indices.size() * sizeof(u32));

    const GLsizei stride = static_cast<GLsizei>(sizeof(TerrainVertex));

    // Attribute 0: position (vec3) at offset 0
    m_vao.attrib(0, 3, GL_FLOAT, GL_FALSE, stride,
                 offsetof(TerrainVertex, px));

    // Attribute 1: normal (vec3) at offset 12
    m_vao.attrib(1, 3, GL_FLOAT, GL_FALSE, stride,
                 offsetof(TerrainVertex, nx));

    // Attribute 2: color (vec4 normalized u8) at offset 24
    m_vao.attrib(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                 offsetof(TerrainVertex, color));

    // Attribute 3: texture weights (vec4) at offset 28
    m_vao.attrib(3, 4, GL_FLOAT, GL_FALSE, stride,
                 offsetof(TerrainVertex, tw0));

    // Attribute 4: texcoord (vec2) at offset 44
    m_vao.attrib(4, 2, GL_FLOAT, GL_FALSE, stride,
                 offsetof(TerrainVertex, u));

    VertexArray::unbind();
    VertexBuffer::unbind();
    // Note: do NOT unbind the IBO while the VAO is unbound — the VAO
    // remembers the element buffer binding.

    LOG_INFO("TerrainRenderer: uploaded %u vertices, %u indices (%u triangles)",
             vert_count, m_index_count, m_index_count / 3);
}

void TerrainRenderer::render(const float* view_matrix, const float* proj_matrix) {
    if (m_index_count == 0) return;

    m_shader.bind();
    m_shader.set_mat4("u_view", view_matrix);
    m_shader.set_mat4("u_proj", proj_matrix);

    // Set texture count so the fragment shader knows which mode to use.
    m_shader.set_int("u_texture_count", static_cast<int>(m_texture_count));

    // Bind active textures.
    for (u32 i = 0; i < m_texture_count && i < 4; ++i) {
        if (m_textures[i].valid()) {
            m_textures[i].bind(i);
        }
    }

    m_vao.bind();
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_index_count),
                   GL_UNSIGNED_INT, nullptr);
    VertexArray::unbind();
    Shader::unbind();
}

void TerrainRenderer::destroy() {
    m_shader.destroy();
    m_vao.destroy();
    m_vbo.destroy();
    m_ibo.destroy();
    m_index_count = 0;

    for (u32 i = 0; i < 16; ++i) {
        m_textures[i].destroy();
    }
    m_texture_count = 0;

    LOG_INFO("TerrainRenderer: destroyed");
}

void TerrainRenderer::set_texture(u32 layer, u32 width, u32 height, const void* pixels) {
    if (layer >= 16) {
        LOG_WARN("TerrainRenderer: texture layer %u out of range (max 15)", layer);
        return;
    }

    m_textures[layer].create(width, height, pixels);

    // Track highest contiguous texture count for the shader.
    if (layer >= m_texture_count) {
        m_texture_count = layer + 1;
    }

    LOG_INFO("TerrainRenderer: loaded texture layer %u (%ux%u), %u layers active",
             layer, width, height, m_texture_count);
}

} // namespace swbf
