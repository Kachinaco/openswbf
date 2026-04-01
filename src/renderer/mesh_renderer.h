#pragma once

#include "renderer/backend/gl_buffers.h"
#include "renderer/backend/gl_shader.h"
#include "renderer/backend/gl_texture.h"
#include "renderer/model_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

// ---------------------------------------------------------------------------
// GPU-side mesh and model data.
//
// Each GPUMesh corresponds to one MeshSegment -- its own VAO/VBO/IBO so it
// can be drawn with a single glDrawElements call.
// ---------------------------------------------------------------------------

/// Maximum bones the skinned shader supports.  Must match the array size
/// in the GLSL shader source.  Capped at a value that works with GLSL ES 1.00
/// uniform limits (typically 128 vec4s = 32 mat4s on WebGL 1).
static constexpr int GPU_MAX_BONES = 32;

/// A single mesh segment uploaded to the GPU.
struct GPUMesh {
    VertexArray  vao;
    VertexBuffer vbo;
    IndexBuffer  ibo;
    uint32_t     index_count    = 0;
    uint32_t     material_index = 0;

    /// True if this mesh was uploaded with skinning vertex attributes
    /// (bone indices + bone weights).
    bool skinned = false;

    /// Maps local bone indices (in skin vertex data) to skeleton-global
    /// bone indices.  Populated from MeshSegment::bone_map.
    std::vector<int> bone_map;
};

/// A complete model on the GPU: mesh segments + shared texture array.
struct GPUModel {
    std::string             name;
    std::vector<GPUMesh>    meshes;
    std::vector<GLTexture>  textures;

    /// True if any mesh in this model is skinned.
    bool has_skinned_meshes() const {
        for (const auto& m : meshes) {
            if (m.skinned) return true;
        }
        return false;
    }
};

// ---------------------------------------------------------------------------
// MeshRenderer -- compiles the mesh shader, uploads models, draws them.
//
// Supports two rendering modes:
//   1. Static -- standard model/view/projection transform (no skinning).
//   2. Skinned -- applies bone matrix palette for skeletal deformation.
// ---------------------------------------------------------------------------

class MeshRenderer {
public:
    MeshRenderer() = default;
    ~MeshRenderer() = default;

    // Non-copyable.
    MeshRenderer(const MeshRenderer&) = delete;
    MeshRenderer& operator=(const MeshRenderer&) = delete;

    /// Compile both static and skinned shader programs.  Must be called
    /// after an OpenGL context is current.  Returns false on failure.
    bool init();

    /// Upload a CPU-side Model to the GPU and return the resulting GPUModel.
    /// Each MeshSegment becomes a separate GPUMesh with its own VAO/VBO/IBO.
    /// Skinned segments get additional bone index/weight vertex attributes.
    GPUModel upload(const Model& model);

    /// Render a GPUModel without skinning (static meshes).
    ///
    /// @param model        The uploaded GPU model to draw.
    /// @param model_matrix 4x4 column-major model (object -> world) matrix.
    /// @param view_matrix  4x4 column-major view (world -> camera) matrix.
    /// @param proj_matrix  4x4 column-major projection matrix.
    void render(const GPUModel& model,
                const float* model_matrix,
                const float* view_matrix,
                const float* proj_matrix);

    /// Render a GPUModel with skeletal animation (skinned meshes).
    ///
    /// @param model          The uploaded GPU model to draw.
    /// @param model_matrix   4x4 column-major model (object -> world) matrix.
    /// @param view_matrix    4x4 column-major view (world -> camera) matrix.
    /// @param proj_matrix    4x4 column-major projection matrix.
    /// @param bone_matrices  Array of bone_count * 16 floats: column-major 4x4
    ///                       skinning matrices (one per bone).
    /// @param bone_count     Number of bones in the bone_matrices array.
    void render_skinned(const GPUModel& model,
                        const float* model_matrix,
                        const float* view_matrix,
                        const float* proj_matrix,
                        const float* bone_matrices,
                        int bone_count);

    /// Release the shader programs.
    void destroy();

private:
    Shader m_shader;          // Static (non-skinned) mesh shader
    Shader m_skinned_shader;  // Skinned mesh shader
};

} // namespace swbf
