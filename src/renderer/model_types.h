#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

// ---------------------------------------------------------------------------
// CPU-side mesh / model data types.
//
// These structs are the single source of truth for model data flowing from
// the asset pipeline (MSH/LVL parser) to the renderer (MeshRenderer).
// ---------------------------------------------------------------------------

/// Maximum bone influences per vertex for GPU skinning.
/// SWBF PC runtime only uses 1, but we support up to 4 for correctness.
static constexpr int MAX_BONE_INFLUENCES = 4;

/// Per-vertex data matching the SWBF .msh vertex layout.
struct Vertex {
    float position[3]  = {0.0f, 0.0f, 0.0f};
    float normal[3]    = {0.0f, 0.0f, 0.0f};
    float uv[2]        = {0.0f, 0.0f};
    uint8_t color[4]   = {255, 255, 255, 255}; // RGBA vertex color (0-255)
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

/// A material definition parsed from MATD chunks inside the MATL list.
struct Material {
    std::string name;

    float diffuse[4]  = {1.0f, 1.0f, 1.0f, 1.0f};
    float specular[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float ambient[4]  = {1.0f, 1.0f, 1.0f, 1.0f};

    float gloss = 0.0f;

    /// ATRB flags byte:
    ///   bit 0 = emissive
    ///   bit 1 = glow
    ///   bit 2 = transparency (blended)
    ///   bit 3 = per-pixel lighting
    ///   bit 4 = additive transparency
    ///   bit 5 = specular
    uint8_t flags = 0;

    /// ATRB render type:
    ///   0 = normal
    ///   1 = glow
    ///   2 = lightmap (unused in SWBF1)
    ///   3 = scrolling
    ///   4 = specular
    ///   5 = glossmap
    ///   6 = chrome / env-map
    ///   7 = animated
    ///  22 = bumpmap
    ///  24 = tiled normalmap
    ///  25 = energy
    ///  26 = afterburner
    uint8_t render_type = 0;

    /// Up to 4 texture layer names (TX0D, TX1D, TX2D, TX3D).
    std::string textures[4];
};

/// A contiguous sub-mesh that shares a single material/texture.
/// Corresponds to one draw call on the GPU.
struct MeshSegment {
    std::vector<Vertex>   vertices;
    std::vector<uint16_t> indices;
    uint32_t material_index = 0; // Index into Model::materials or Model::textures

    /// Texture name from munged TNAM chunk (used when no MATL is present).
    std::string texture_name;

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

/// Bounding box for a model.
struct BoundingBox {
    float min[3] = {0.0f, 0.0f, 0.0f};
    float max[3] = {0.0f, 0.0f, 0.0f};
};

/// A complete model: mesh segments, materials (from asset loading), and
/// optionally resolved textures (for GPU upload).
struct Model {
    std::string name;

    std::vector<MeshSegment>  segments;
    std::vector<Material>     materials;  // Parsed material definitions
    std::vector<TextureData>  textures;   // Resolved texture data for GPU upload

    BoundingBox bounding_box;

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
