#include "renderer/mesh_renderer.h"
#include "core/log.h"
#include "core/types.h"

#include <algorithm>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <SDL_opengl.h>
#endif

namespace swbf {

// ===========================================================================
// GLSL ES 1.00 shader sources -- static (non-skinned)
// ===========================================================================

static const char* k_mesh_vert_src = R"glsl(#version 100
precision highp float;

// Vertex attributes
attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec2 a_uv;
attribute vec4 a_color;

// Uniforms
uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

// Varyings passed to fragment shader
varying vec3 v_world_normal;
varying vec2 v_uv;
varying vec4 v_color;

void main() {
    mat4 mvp = u_proj * u_view * u_model;
    gl_Position = mvp * vec4(a_position, 1.0);

    // Transform normal to world space.  For correct lighting with
    // non-uniform scale we would need the inverse-transpose, but for
    // the common case (uniform scale or no scale) using the upper-left
    // 3x3 of the model matrix is sufficient and avoids a matrix inverse.
    v_world_normal = mat3(u_model) * a_normal;

    v_uv    = a_uv;
    v_color = a_color;
}
)glsl";

static const char* k_mesh_frag_src = R"glsl(#version 100
precision mediump float;

// Varyings from vertex shader
varying vec3 v_world_normal;
varying vec2 v_uv;
varying vec4 v_color;

// Uniforms
uniform sampler2D u_diffuse;
uniform int       u_has_texture;

void main() {
    // Normalise the interpolated normal.
    vec3 N = normalize(v_world_normal);

    // Simple directional light from upper-right-forward.
    vec3 light_dir = normalize(vec3(0.5, 1.0, 0.3));
    float NdotL = max(dot(N, light_dir), 0.0);

    // Half-Lambert-ish: 0.3 ambient + 0.7 diffuse contribution.
    float lit = NdotL * 0.7 + 0.3;

    vec4 base_color;
    if (u_has_texture != 0) {
        // Sample diffuse texture and modulate by vertex color.
        base_color = texture2D(u_diffuse, v_uv) * v_color;
    } else {
        // No texture bound -- use vertex color directly.
        base_color = v_color;
    }

    gl_FragColor = vec4(base_color.rgb * lit, base_color.a);
}
)glsl";

// ===========================================================================
// GLSL ES 1.00 shader sources -- skinned
//
// The skinned vertex shader adds bone index + weight attributes and applies
// up to 4 bone influences per vertex using a uniform bone matrix palette.
//
// Bone matrices are passed as an array of mat4 uniforms.  The bone index
// attribute selects which matrix to use, and the weight controls how much
// influence each bone has.  The final position/normal is the weighted sum
// of all influences.
// ===========================================================================

// Maximum bones the shader supports.  Must match GPU_MAX_BONES in the header.
// GLSL ES 1.00 has a limit of ~128 vec4 uniforms; 32 mat4 = 128 vec4 exactly.
static const char* k_skinned_vert_src = R"glsl(#version 100
precision highp float;

// Vertex attributes -- base geometry (same layout as static shader)
attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec2 a_uv;
attribute vec4 a_color;

// Skinning attributes -- from a separate VBO
attribute vec4 a_bone_indices;   // up to 4 bone indices (float-encoded)
attribute vec4 a_bone_weights;   // corresponding weights

// Uniforms
uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;
uniform mat4 u_bones[32];       // bone matrix palette
uniform int  u_skinned;         // 1 = apply skinning, 0 = static fallback

// Varyings passed to fragment shader
varying vec3 v_world_normal;
varying vec2 v_uv;
varying vec4 v_color;

void main() {
    vec4 skinned_pos;
    vec3 skinned_normal;

    if (u_skinned != 0) {
        // Apply bone influences.
        skinned_pos    = vec4(0.0);
        skinned_normal = vec3(0.0);

        // Influence 0
        float w0 = a_bone_weights.x;
        if (w0 > 0.0) {
            int idx0 = int(a_bone_indices.x);
            skinned_pos    += u_bones[idx0] * vec4(a_position, 1.0) * w0;
            skinned_normal += mat3(u_bones[idx0]) * a_normal * w0;
        }

        // Influence 1
        float w1 = a_bone_weights.y;
        if (w1 > 0.0) {
            int idx1 = int(a_bone_indices.y);
            skinned_pos    += u_bones[idx1] * vec4(a_position, 1.0) * w1;
            skinned_normal += mat3(u_bones[idx1]) * a_normal * w1;
        }

        // Influence 2
        float w2 = a_bone_weights.z;
        if (w2 > 0.0) {
            int idx2 = int(a_bone_indices.z);
            skinned_pos    += u_bones[idx2] * vec4(a_position, 1.0) * w2;
            skinned_normal += mat3(u_bones[idx2]) * a_normal * w2;
        }

        // Influence 3
        float w3 = a_bone_weights.w;
        if (w3 > 0.0) {
            int idx3 = int(a_bone_indices.w);
            skinned_pos    += u_bones[idx3] * vec4(a_position, 1.0) * w3;
            skinned_normal += mat3(u_bones[idx3]) * a_normal * w3;
        }

        skinned_pos.w = 1.0;
    } else {
        skinned_pos    = vec4(a_position, 1.0);
        skinned_normal = a_normal;
    }

    mat4 mvp = u_proj * u_view * u_model;
    gl_Position = mvp * skinned_pos;

    v_world_normal = mat3(u_model) * skinned_normal;
    v_uv    = a_uv;
    v_color = a_color;
}
)glsl";

// The skinned fragment shader is identical to the static one.
static const char* k_skinned_frag_src = k_mesh_frag_src;

// ===========================================================================
// Vertex attribute layout
// ===========================================================================

// Must match the Vertex struct in model_types.h:
//   float position[3]  -- 12 bytes, offset 0
//   float normal[3]    -- 12 bytes, offset 12
//   float uv[2]        --  8 bytes, offset 24
//   uint8_t color[4]   --  4 bytes, offset 32
//   Total stride: 36 bytes

static constexpr GLsizei k_vertex_stride = 36;

static void setup_vertex_attribs() {
    // location 0: a_position -- vec3, float
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, k_vertex_stride,
                          reinterpret_cast<const void*>(0));

    // location 1: a_normal -- vec3, float
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, k_vertex_stride,
                          reinterpret_cast<const void*>(12));

    // location 2: a_uv -- vec2, float
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, k_vertex_stride,
                          reinterpret_cast<const void*>(24));

    // location 3: a_color -- vec4, unsigned byte, normalized to [0,1]
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, k_vertex_stride,
                          reinterpret_cast<const void*>(32));
}

// Skinning vertex data layout (separate VBO):
//   float bone_indices[4]  -- 16 bytes, offset 0  (stored as float for GLSL ES 1.00)
//   float bone_weights[4]  -- 16 bytes, offset 16
//   Total stride: 32 bytes

static constexpr GLsizei k_skin_vertex_stride = 32;

static void setup_skin_vertex_attribs() {
    // location 4: a_bone_indices -- vec4, float (indices encoded as floats)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, k_skin_vertex_stride,
                          reinterpret_cast<const void*>(0));

    // location 5: a_bone_weights -- vec4, float
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, k_skin_vertex_stride,
                          reinterpret_cast<const void*>(16));
}

/// Interleaved skin vertex data for GPU upload.
/// GLSL ES 1.00 does not support integer attributes, so we store bone
/// indices as floats.
struct GPUSkinVertex {
    float bone_indices[4]; // float-encoded bone indices
    float bone_weights[4]; // bone weights
};

// ===========================================================================
// MeshRenderer implementation
// ===========================================================================

bool MeshRenderer::init() {
    // Compile the static (non-skinned) mesh shader.
    if (!m_shader.compile(k_mesh_vert_src, k_mesh_frag_src,
            {{0, "a_position"}, {1, "a_normal"}, {2, "a_uv"}, {3, "a_color"}})) {
        LOG_ERROR("MeshRenderer: failed to compile static mesh shader");
        return false;
    }
    LOG_INFO("MeshRenderer: static shader compiled successfully");

    // Compile the skinned mesh shader.
    if (!m_skinned_shader.compile(k_skinned_vert_src, k_skinned_frag_src,
            {{0, "a_position"}, {1, "a_normal"}, {2, "a_uv"}, {3, "a_color"},
             {4, "a_bone_indices"}, {5, "a_bone_weights"}})) {
        LOG_ERROR("MeshRenderer: failed to compile skinned mesh shader");
        return false;
    }
    LOG_INFO("MeshRenderer: skinned shader compiled successfully");

    return true;
}

GPUModel MeshRenderer::upload(const Model& model) {
    GPUModel gpu;
    gpu.name = model.name;

    // ----- Upload textures ------------------------------------------------
    gpu.textures.reserve(model.textures.size());
    for (const auto& tex_data : model.textures) {
        GLTexture tex;
        if (!tex_data.pixels.empty() && tex_data.width > 0 && tex_data.height > 0) {
            tex.create(static_cast<u32>(tex_data.width),
                       static_cast<u32>(tex_data.height),
                       tex_data.pixels.data());
            LOG_DEBUG("MeshRenderer: uploaded texture '%s' (%dx%d)",
                      tex_data.name.c_str(), tex_data.width, tex_data.height);
        } else {
            LOG_WARN("MeshRenderer: texture '%s' has no pixel data, skipping",
                     tex_data.name.c_str());
        }
        gpu.textures.push_back(std::move(tex));
    }

    // ----- Upload mesh segments -------------------------------------------
    gpu.meshes.reserve(model.segments.size());
    for (const auto& seg : model.segments) {
        GPUMesh mesh;
        mesh.index_count    = static_cast<uint32_t>(seg.indices.size());
        mesh.material_index = seg.material_index;
        mesh.skinned        = seg.is_skinned();
        mesh.bone_map       = seg.bone_map;

        if (seg.vertices.empty() || seg.indices.empty()) {
            LOG_WARN("MeshRenderer: skipping empty mesh segment in '%s'",
                     model.name.c_str());
            continue;
        }

        // Create and bind VAO first.
        mesh.vao.create();
        mesh.vao.bind();

        // Upload vertex geometry data.
        mesh.vbo.create(seg.vertices.data(),
                        seg.vertices.size() * sizeof(Vertex));

        // Upload index data.
        mesh.ibo.create(seg.indices.data(),
                        seg.indices.size() * sizeof(uint16_t));

        // Configure base vertex attribute pointers.
        setup_vertex_attribs();

        // If skinned, upload bone indices/weights as a separate VBO and
        // configure skinning attribute pointers.
        if (mesh.skinned) {
            // Convert SkinVertex data to GPU-friendly float format.
            std::vector<GPUSkinVertex> gpu_skin(seg.skin_vertices.size());
            for (std::size_t i = 0; i < seg.skin_vertices.size(); ++i) {
                const auto& sv = seg.skin_vertices[i];
                for (int j = 0; j < MAX_BONE_INFLUENCES; ++j) {
                    gpu_skin[i].bone_indices[j] =
                        static_cast<float>(sv.bone_indices[j]);
                    gpu_skin[i].bone_weights[j] = sv.bone_weights[j];
                }
            }

            // Create a second VBO for skinning data.  We need to bind it
            // while setting up attributes so the VAO captures it.
            VertexBuffer skin_vbo;
            skin_vbo.create(gpu_skin.data(),
                            gpu_skin.size() * sizeof(GPUSkinVertex));

            setup_skin_vertex_attribs();

            // The skin VBO will be destroyed when it goes out of scope, but
            // the VAO has already captured the binding. The VBO data persists
            // because OpenGL keeps a reference as long as the VAO exists.
            // (This relies on the VAO holding a reference to the buffer.)

            LOG_DEBUG("MeshRenderer: uploaded skinned segment with %zu verts, "
                      "%zu bone_map entries",
                      seg.skin_vertices.size(), seg.bone_map.size());
        } else {
            // For non-skinned meshes used with the skinned shader, provide
            // default disabled attributes (weight = 0).
            glDisableVertexAttribArray(4);
            glDisableVertexAttribArray(5);
        }

        // Unbind VAO.
        VertexArray::unbind();

        gpu.meshes.push_back(std::move(mesh));
    }

    LOG_INFO("MeshRenderer: uploaded model '%s' (%zu meshes, %zu textures, skinned=%d)",
             model.name.c_str(), gpu.meshes.size(), gpu.textures.size(),
             gpu.has_skinned_meshes() ? 1 : 0);

    return gpu;
}

void MeshRenderer::render(const GPUModel& model,
                           const float* model_matrix,
                           const float* view_matrix,
                           const float* proj_matrix) {
    m_shader.use();

    // Set transform uniforms.
    m_shader.set_mat4("u_model", model_matrix);
    m_shader.set_mat4("u_view",  view_matrix);
    m_shader.set_mat4("u_proj",  proj_matrix);

    // Diffuse texture is always on unit 0.
    m_shader.set_int("u_diffuse", 0);

    for (const auto& mesh : model.meshes) {
        // Bind texture if the material index is valid and the texture exists.
        bool has_texture = false;
        if (mesh.material_index < static_cast<uint32_t>(model.textures.size())) {
            const auto& tex = model.textures[mesh.material_index];
            if (tex.valid()) {
                tex.bind(0);
                has_texture = true;
            }
        }
        m_shader.set_int("u_has_texture", has_texture ? 1 : 0);

        // Bind VAO (which has the VBO, IBO, and attrib pointers recorded).
        mesh.vao.bind();

        // Draw.
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(mesh.index_count),
                       GL_UNSIGNED_SHORT,
                       nullptr);
    }

    // Clean up state.
    VertexArray::unbind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void MeshRenderer::render_skinned(const GPUModel& model,
                                   const float* model_matrix,
                                   const float* view_matrix,
                                   const float* proj_matrix,
                                   const float* bone_matrices,
                                   int bone_count) {
    m_skinned_shader.use();

    // Set transform uniforms.
    m_skinned_shader.set_mat4("u_model", model_matrix);
    m_skinned_shader.set_mat4("u_view",  view_matrix);
    m_skinned_shader.set_mat4("u_proj",  proj_matrix);

    // Upload bone matrix palette.
    // Clamp to the shader's maximum bone count.
    int clamped_bones = std::min(bone_count, GPU_MAX_BONES);

    if (bone_matrices && clamped_bones > 0) {
        GLint loc = m_skinned_shader.uniform_location("u_bones[0]");
        if (loc >= 0) {
            glUniformMatrix4fv(loc, clamped_bones, GL_FALSE, bone_matrices);
        }
    }

    // Diffuse texture is always on unit 0.
    m_skinned_shader.set_int("u_diffuse", 0);

    for (const auto& mesh : model.meshes) {
        // Enable/disable skinning per mesh segment.
        m_skinned_shader.set_int("u_skinned", mesh.skinned ? 1 : 0);

        // If this mesh has a bone map, we need to remap the bone matrices
        // to match the local bone indices used in the vertex data.
        // For simplicity, we upload a remapped palette for each mesh.
        if (mesh.skinned && !mesh.bone_map.empty() &&
            bone_matrices && clamped_bones > 0) {
            // Build a remapped bone matrix array.
            float remapped[GPU_MAX_BONES * 16];
            std::memset(remapped, 0, sizeof(remapped));

            int local_count = std::min(static_cast<int>(mesh.bone_map.size()),
                                       GPU_MAX_BONES);
            for (int i = 0; i < local_count; ++i) {
                int global_idx = mesh.bone_map[static_cast<std::size_t>(i)];
                if (global_idx >= 0 && global_idx < clamped_bones) {
                    std::memcpy(&remapped[i * 16],
                                &bone_matrices[global_idx * 16],
                                sizeof(float) * 16);
                } else {
                    // Identity matrix for unmapped bones.
                    remapped[i * 16 + 0]  = 1.0f;
                    remapped[i * 16 + 5]  = 1.0f;
                    remapped[i * 16 + 10] = 1.0f;
                    remapped[i * 16 + 15] = 1.0f;
                }
            }

            GLint loc = m_skinned_shader.uniform_location("u_bones[0]");
            if (loc >= 0) {
                glUniformMatrix4fv(loc, std::min(local_count, GPU_MAX_BONES),
                                   GL_FALSE, remapped);
            }
        }

        // Bind texture.
        bool has_texture = false;
        if (mesh.material_index < static_cast<uint32_t>(model.textures.size())) {
            const auto& tex = model.textures[mesh.material_index];
            if (tex.valid()) {
                tex.bind(0);
                has_texture = true;
            }
        }
        m_skinned_shader.set_int("u_has_texture", has_texture ? 1 : 0);

        // Bind VAO and draw.
        mesh.vao.bind();
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(mesh.index_count),
                       GL_UNSIGNED_SHORT,
                       nullptr);
    }

    // Clean up state.
    VertexArray::unbind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void MeshRenderer::destroy() {
    m_shader = Shader{};
    m_skinned_shader = Shader{};
    LOG_INFO("MeshRenderer: destroyed");
}

} // namespace swbf
