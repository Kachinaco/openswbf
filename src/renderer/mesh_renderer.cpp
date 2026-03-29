#include "renderer/mesh_renderer.h"
#include "core/log.h"
#include "core/types.h"

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <SDL_opengl.h>
#endif

namespace swbf {

// ===========================================================================
// GLSL ES 3.00 shader sources
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
        // No texture bound — use vertex color directly.
        base_color = v_color;
    }

    gl_FragColor = vec4(base_color.rgb * lit, base_color.a);
}
)glsl";

// ===========================================================================
// Vertex attribute layout
// ===========================================================================

// Must match the Vertex struct in model_types.h:
//   float position[3]  — 12 bytes, offset 0
//   float normal[3]    — 12 bytes, offset 12
//   float uv[2]        —  8 bytes, offset 24
//   uint8_t color[4]   —  4 bytes, offset 32
//   Total stride: 36 bytes

static constexpr GLsizei k_vertex_stride = 36;

static void setup_vertex_attribs() {
    // location 0: a_position — vec3, float
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, k_vertex_stride,
                          reinterpret_cast<const void*>(0));

    // location 1: a_normal — vec3, float
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, k_vertex_stride,
                          reinterpret_cast<const void*>(12));

    // location 2: a_uv — vec2, float
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, k_vertex_stride,
                          reinterpret_cast<const void*>(24));

    // location 3: a_color — vec4, unsigned byte, normalized to [0,1]
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, k_vertex_stride,
                          reinterpret_cast<const void*>(32));
}

// ===========================================================================
// MeshRenderer implementation
// ===========================================================================

bool MeshRenderer::init() {
    if (!m_shader.compile(k_mesh_vert_src, k_mesh_frag_src,
            {{0, "a_position"}, {1, "a_normal"}, {2, "a_uv"}, {3, "a_color"}})) {
        LOG_ERROR("MeshRenderer: failed to compile mesh shader");
        return false;
    }
    LOG_INFO("MeshRenderer: shader compiled successfully");
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

        if (seg.vertices.empty() || seg.indices.empty()) {
            LOG_WARN("MeshRenderer: skipping empty mesh segment in '%s'",
                     model.name.c_str());
            continue;
        }

        // Create and bind VAO first — it will capture the VBO/IBO bindings
        // and vertex attribute state.
        mesh.vao.create();
        mesh.vao.bind();

        // Upload vertex data.
        mesh.vbo.create(seg.vertices.data(),
                        seg.vertices.size() * sizeof(Vertex));

        // Upload index data.
        mesh.ibo.create(seg.indices.data(),
                        seg.indices.size() * sizeof(uint16_t));

        // Configure vertex attribute pointers (captured by the bound VAO).
        setup_vertex_attribs();

        // Unbind VAO (VBO/IBO state is now recorded inside the VAO).
        VertexArray::unbind();

        gpu.meshes.push_back(std::move(mesh));
    }

    LOG_INFO("MeshRenderer: uploaded model '%s' (%zu meshes, %zu textures)",
             model.name.c_str(), gpu.meshes.size(), gpu.textures.size());

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

void MeshRenderer::destroy() {
    // The Shader destructor will call glDeleteProgram, but we provide an
    // explicit destroy path so the caller can control teardown order.
    // Move-assign an empty Shader to trigger cleanup of the current one.
    m_shader = Shader{};
    LOG_INFO("MeshRenderer: destroyed");
}

} // namespace swbf
