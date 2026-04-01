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

/// Maximum bone influences per vertex for GPU skinning.
/// SWBF PC runtime only uses 1, but we support up to 4 for correctness.
static constexpr int MAX_BONE_INFLUENCES = 4;

/// Per-vertex data matching the SWBF .msh vertex layout.
struct Vertex {
    float position[3];   // Object-space position
    float normal[3];     // Object-space normal
    float uv[2];         // Texture coordinates
    uint8_t color[4];    // RGBA vertex color (0-255)
};

/// Per-vertex skinning data, stored separately from the base Vertex to
/// avoid changing the Vertex layout for non-skinned meshes.
struct SkinVertex {
    /// Bone indices (into the segment's bone_map or skeleton bone array).
    uint8_t bone_indices[MAX_BONE_INFLUENCES] = {0, 0, 0, 0};
    /// Bone weights (should sum to 1.0 for influenced vertices).
    float   bone_weights[MAX_BONE_INFLUENCES] = {0.0f, 0.0f, 0.0f, 0.0f};
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

    /// Skinning data -- one entry per vertex (same count as vertices).
    /// Empty if this segment is not skinned.
    std::vector<SkinVertex> skin_vertices;

    /// Bone map for this segment: maps local bone indices (used in
    /// skin_vertices) to skeleton-global bone indices.
    /// From ENVL or BMAP chunks.
    std::vector<int> bone_map;

    /// True if this segment has valid skinning data.
    bool is_skinned() const { return !skin_vertices.empty(); }
};

/// A complete model: one or more mesh segments plus associated textures.
struct Model {
    std::string name;
    std::vector<MeshSegment>  segments;
    std::vector<TextureData>  textures;

    /// Index of the skeleton associated with this model (-1 = none).
    /// References into a separate skeleton registry managed by the game.
    int skeleton_index = -1;

    /// True if any segment in this model is skinned.
    bool has_skinned_segments() const {
        for (const auto& seg : segments) {
            if (seg.is_skinned()) return true;
        }
        return false;
    }
};

} // namespace swbf
