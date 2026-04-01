#pragma once

#include "core/types.h"
#include "assets/ucfb/chunk_reader.h"
#include "renderer/model_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

// ---------------------------------------------------------------------------
// ModelLoader -- parses munged MSH data from .lvl chunks
//
// Outputs the unified Model/MeshSegment/Vertex types from model_types.h,
// which are directly consumable by MeshRenderer.
//
// Usage:
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

    /// Parse a single SEGM (segment) chunk into a MeshSegment (classic format).
    MeshSegment parse_segment(ChunkReader segm);

    /// Parse a munged-format segm (lowercase) chunk into a MeshSegment.
    /// These use VBUF/IBUF/TNAM/SKIN/BMAP instead of POSL/NRML/UV0L/STRP.
    MeshSegment parse_munged_segment(ChunkReader segm, const Model& model);

    /// Decode a VBUF (vertex buffer) chunk, handling all flag combinations
    /// including compressed positions, normals, and texture coordinates.
    void decode_vbuf(ChunkReader vbuf, MeshSegment& segment,
                     const Model& model);

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
