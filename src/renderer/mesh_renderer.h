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
// Each GPUMesh corresponds to one MeshSegment — its own VAO/VBO/IBO so it
// can be drawn with a single glDrawElements call.
// ---------------------------------------------------------------------------

/// A single mesh segment uploaded to the GPU.
struct GPUMesh {
    VertexArray  vao;
    VertexBuffer vbo;
    IndexBuffer  ibo;
    uint32_t     index_count    = 0;
    uint32_t     material_index = 0;
};

/// A complete model on the GPU: mesh segments + shared texture array.
struct GPUModel {
    std::string             name;
    std::vector<GPUMesh>    meshes;
    std::vector<GLTexture>  textures;
};

// ---------------------------------------------------------------------------
// MeshRenderer — compiles the mesh shader, uploads models, draws them.
// ---------------------------------------------------------------------------

class MeshRenderer {
public:
    MeshRenderer() = default;
    ~MeshRenderer() = default;

    // Non-copyable.
    MeshRenderer(const MeshRenderer&) = delete;
    MeshRenderer& operator=(const MeshRenderer&) = delete;

    /// Compile the mesh shader program.  Must be called after an OpenGL
    /// context is current.  Returns false on shader compilation failure.
    bool init();

    /// Upload a CPU-side Model to the GPU and return the resulting GPUModel.
    /// Each MeshSegment becomes a separate GPUMesh with its own VAO/VBO/IBO.
    /// Each TextureData becomes a GLTexture.
    GPUModel upload(const Model& model);

    /// Render a GPUModel.
    ///
    /// @param model        The uploaded GPU model to draw.
    /// @param model_matrix 4x4 column-major model (object -> world) matrix.
    /// @param view_matrix  4x4 column-major view (world -> camera) matrix.
    /// @param proj_matrix  4x4 column-major projection matrix.
    void render(const GPUModel& model,
                const float* model_matrix,
                const float* view_matrix,
                const float* proj_matrix);

    /// Release the shader program.
    void destroy();

private:
    Shader m_shader;
};

} // namespace swbf
