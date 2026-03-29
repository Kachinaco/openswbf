#pragma once

#include "core/types.h"
#include "assets/ucfb/chunk_reader.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

// ---------------------------------------------------------------------------
// Model data structures
//
// These represent the parsed, GPU-ready output of the MSH model loader.
// Triangle strips with restart markers are converted to flat triangle lists.
// ---------------------------------------------------------------------------

/// A single vertex with position, normal, UV, and optional vertex color.
struct Vertex {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float normal[3]   = {0.0f, 0.0f, 0.0f};
    float uv[2]       = {0.0f, 0.0f};
    uint32_t color     = 0xFFFFFFFF; // RGBA packed, default opaque white
};

/// A mesh segment — a contiguous set of triangles sharing one material.
struct MeshSegment {
    uint32_t material_index = 0;
    std::vector<Vertex>   vertices;
    std::vector<uint16_t> indices; // triangle list (3 indices per triangle)
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

/// A fully parsed model extracted from a modl (or MSH2) chunk hierarchy.
struct Model {
    std::string name;

    std::vector<Material>    materials;
    std::vector<MeshSegment> segments;

    float bounding_box_min[3] = {0.0f, 0.0f, 0.0f};
    float bounding_box_max[3] = {0.0f, 0.0f, 0.0f};
};

// ---------------------------------------------------------------------------
// ModelLoader — parses munged MSH data from .lvl chunks
//
// Usage:
//   // `modl_chunk` is a ChunkReader positioned at a top-level modl chunk
//   // inside the .lvl file (tag == chunk_id::modl).
//   ModelLoader loader;
//   Model m = loader.load(modl_chunk);
//
// The loader handles the full chunk hierarchy:
//   modl -> (INFO) -> MSH2 -> SINF + MATL + MODL* -> GEOM -> SEGM*
//
// Munged .lvl models embed the MSH data inside a ucfb wrapper; the loader
// peels off the outer layers and digs into the MSH2 payload.
// ---------------------------------------------------------------------------

class ModelLoader {
public:
    /// Parse a complete model from a modl chunk.
    /// The chunk should be at the top-level modl wrapper as found in a .lvl.
    Model load(ChunkReader chunk);

private:
    /// Parse the MATL (material list) container. Fills model.materials.
    void parse_materials(ChunkReader matl, Model& model);

    /// Parse a single MATD (material definition) chunk.
    Material parse_material(ChunkReader matd);

    /// Parse geometry from one or more MODL sub-chunks inside MSH2.
    /// Each MODL sub-chunk may contain a GEOM with SEGM children.
    void parse_geometry(ChunkReader modl_sub, Model& model);

    /// Parse a single SEGM (segment) chunk into a MeshSegment.
    MeshSegment parse_segment(ChunkReader segm);

    /// Parse SINF (scene info) to extract bounding box and model name.
    void parse_scene_info(ChunkReader sinf, Model& model);

    /// Convert triangle strips (with 0x8000 restart markers) to a flat
    /// triangle list. The high bit of each index is a restart flag; the
    /// actual vertex index is (raw_index & 0x7FFF). Winding order alternates
    /// for each successive triangle within a strip.
    static std::vector<uint16_t> convert_strips_to_triangles(
        const std::vector<uint16_t>& strips);
};

} // namespace swbf
