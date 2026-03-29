#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

// ---------------------------------------------------------------------------
// CPU-side mesh / model data types.
//
// These structs represent parsed model data ready for GPU upload.
// The asset pipeline (MSH parser) fills these structures; the MeshRenderer
// consumes them to create GPUModel instances.
// ---------------------------------------------------------------------------

/// Per-vertex data matching the SWBF .msh vertex layout.
struct Vertex {
    float position[3];   // Object-space position
    float normal[3];     // Object-space normal
    float uv[2];         // Texture coordinates
    uint8_t color[4];    // RGBA vertex color (0-255)
};

/// A single texture image in CPU memory (RGBA8).
struct TextureData {
    std::string name;
    int width  = 0;
    int height = 0;
    std::vector<uint8_t> pixels; // RGBA8, row-major, width * height * 4 bytes
};

/// A contiguous sub-mesh that shares a single material/texture.
/// Corresponds to one draw call on the GPU.
struct MeshSegment {
    std::vector<Vertex>   vertices;
    std::vector<uint16_t> indices;
    uint32_t material_index = 0; // Index into Model::textures
};

/// A complete model: one or more mesh segments plus associated textures.
struct Model {
    std::string name;
    std::vector<MeshSegment>  segments;
    std::vector<TextureData>  textures;
};

} // namespace swbf
