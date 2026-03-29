#include "model_loader.h"

#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace swbf {

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

Model ModelLoader::load(ChunkReader chunk) {
    Model model;

    // In munged .lvl files the top-level modl chunk wraps inner data.
    // The hierarchy can be:
    //   modl -> (ucfb or direct) -> HEDR -> MSH2
    // or in some munged formats the MSH2 is directly inside.
    //
    // Strategy: walk all children looking for known chunk IDs, and recurse
    // into containers until we find MSH2, MATL, MODL (sub), SINF, etc.

    // First, try to find the actual MSH2 chunk by walking down the hierarchy.
    // The modl chunk in a .lvl typically contains child chunks that eventually
    // lead to mesh data.

    auto children = chunk.get_children();

    // Look for a direct MSH2 or navigate through wrapper layers.
    std::vector<ChunkReader> msh2_search;

    for (auto& child : children) {
        if (child.id() == chunk_id::MSH2) {
            msh2_search.push_back(child);
        } else if (child.id() == chunk_id::HEDR ||
                   child.id() == chunk_id::ucfb) {
            // Navigate into HEDR or ucfb wrapper to find MSH2
            auto grandchildren = child.get_children();
            for (auto& gc : grandchildren) {
                if (gc.id() == chunk_id::MSH2) {
                    msh2_search.push_back(gc);
                }
            }
        }
    }

    // If we still haven't found MSH2, the chunk itself might BE the MSH2
    // (caller passed the MSH2 directly).
    if (msh2_search.empty() && chunk.id() == chunk_id::MSH2) {
        // Re-read children from the original chunk. We need to reconstruct
        // the reader since get_children() consumed it.
        ChunkReader msh2_chunk(chunk.data() - 8, chunk.size() + 8);
        msh2_search.push_back(msh2_chunk);
    }

    if (msh2_search.empty()) {
        // Last resort: try treating the entire modl payload as MSH2 content.
        // Some munged formats pack data directly.
        LOG_WARN("ModelLoader: no MSH2 chunk found in modl hierarchy, "
                 "attempting direct parse of children");

        for (auto& child : children) {
            if (child.id() == chunk_id::SINF) {
                parse_scene_info(child, model);
            } else if (child.id() == chunk_id::MATL) {
                parse_materials(child, model);
            } else if (child.id() == chunk_id::MODL_sub) {
                parse_geometry(child, model);
            }
        }
        return model;
    }

    // Parse the MSH2 chunk.
    for (auto& msh2 : msh2_search) {
        auto msh2_children = msh2.get_children();

        for (auto& child : msh2_children) {
            FourCC id = child.id();

            if (id == chunk_id::SINF) {
                parse_scene_info(child, model);
            } else if (id == chunk_id::MATL) {
                parse_materials(child, model);
            } else if (id == chunk_id::MODL_sub) {
                parse_geometry(child, model);
            }
        }
    }

    return model;
}

// ---------------------------------------------------------------------------
// Scene info (SINF)
// ---------------------------------------------------------------------------

void ModelLoader::parse_scene_info(ChunkReader sinf, Model& model) {
    auto children = sinf.get_children();

    for (auto& child : children) {
        if (child.id() == chunk_id::NAME) {
            model.name = child.read_string();
        } else if (child.id() == chunk_id::BBOX) {
            // BBOX layout (in many MSH variants):
            //   float rotation[4]     (quaternion, 16 bytes)
            //   float center[3]       (12 bytes)
            //   float extents[3]      (12 bytes)
            //   float sphere_radius   (4 bytes) — optional
            //
            // We compute min/max from center and extents.
            if (child.size() >= 44) {
                // Skip rotation quaternion (16 bytes)
                child.skip(16);

                float cx = child.read<float>();
                float cy = child.read<float>();
                float cz = child.read<float>();

                float ex = child.read<float>();
                float ey = child.read<float>();
                float ez = child.read<float>();

                model.bounding_box_min[0] = cx - ex;
                model.bounding_box_min[1] = cy - ey;
                model.bounding_box_min[2] = cz - ez;

                model.bounding_box_max[0] = cx + ex;
                model.bounding_box_max[1] = cy + ey;
                model.bounding_box_max[2] = cz + ez;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Material parsing (MATL -> MATD*)
// ---------------------------------------------------------------------------

void ModelLoader::parse_materials(ChunkReader matl, Model& model) {
    // The first 4 bytes of MATL payload is typically the material count (u32).
    // Followed by MATD child chunks.
    // In some formats the count is implicit (just iterate children).

    auto children = matl.get_children();

    for (auto& child : children) {
        if (child.id() == chunk_id::MATD) {
            model.materials.push_back(parse_material(child));
        }
    }
}

Material ModelLoader::parse_material(ChunkReader matd) {
    Material mat;

    auto children = matd.get_children();

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == chunk_id::NAME) {
            mat.name = child.read_string();

        } else if (id == chunk_id::DATA) {
            // DATA chunk layout for materials:
            //   float diffuse[4]   (16 bytes)
            //   float specular[4]  (16 bytes)
            //   float ambient[4]   (16 bytes)
            //   float gloss        (4 bytes)
            // Total: 52 bytes
            if (child.size() >= 52) {
                for (int i = 0; i < 4; ++i) mat.diffuse[i]  = child.read<float>();
                for (int i = 0; i < 4; ++i) mat.specular[i] = child.read<float>();
                for (int i = 0; i < 4; ++i) mat.ambient[i]  = child.read<float>();
                mat.gloss = child.read<float>();
            }

        } else if (id == chunk_id::ATRB) {
            // ATRB layout:
            //   u8 flags
            //   u8 render_type
            //   u8 data0       (varies by render_type)
            //   u8 data1       (varies by render_type)
            if (child.size() >= 2) {
                mat.flags       = child.read<u8>();
                mat.render_type = child.read<u8>();
            }

        } else if (id == chunk_id::TX0D) {
            mat.textures[0] = child.read_string();
        } else if (id == chunk_id::TX1D) {
            mat.textures[1] = child.read_string();
        } else if (id == chunk_id::TX2D) {
            mat.textures[2] = child.read_string();
        } else if (id == chunk_id::TX3D) {
            mat.textures[3] = child.read_string();
        }
    }

    return mat;
}

// ---------------------------------------------------------------------------
// Geometry parsing (MODL sub-chunk -> GEOM -> SEGM*)
// ---------------------------------------------------------------------------

void ModelLoader::parse_geometry(ChunkReader modl_sub, Model& model) {
    auto children = modl_sub.get_children();

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == chunk_id::NAME && model.name.empty()) {
            // Use the sub-model name if we don't have one from SINF.
            model.name = child.read_string();

        } else if (id == chunk_id::GEOM) {
            // GEOM contains SEGM children (and optionally an envelope/ENVL).
            auto geom_children = child.get_children();

            for (auto& segm_chunk : geom_children) {
                if (segm_chunk.id() == chunk_id::SEGM) {
                    model.segments.push_back(parse_segment(segm_chunk));
                }
            }
        }
    }
}

MeshSegment ModelLoader::parse_segment(ChunkReader segm) {
    MeshSegment segment;

    // Temporary storage for raw vertex attributes — built up as we encounter
    // POSL, NRML, UV0L, CLRL chunks, then combined into Vertex structs.
    std::vector<float>    positions;   // flat float[3] per vertex
    std::vector<float>    normals;     // flat float[3] per vertex
    std::vector<float>    uvs;         // flat float[2] per vertex
    std::vector<uint32_t> colors;      // one u32 per vertex
    std::vector<uint16_t> raw_strips;  // raw triangle strip indices

    auto children = segm.get_children();

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == chunk_id::MTRL) {
            // Material index for this segment — single u32.
            segment.material_index = child.read<u32>();

        } else if (id == chunk_id::POSL) {
            // Positions: u32 count followed by float[3] * count.
            u32 count = child.read<u32>();
            positions.resize(static_cast<std::size_t>(count) * 3);
            for (u32 i = 0; i < count * 3; ++i) {
                positions[i] = child.read<float>();
            }

        } else if (id == chunk_id::NRML) {
            // Normals: u32 count followed by float[3] * count.
            u32 count = child.read<u32>();
            normals.resize(static_cast<std::size_t>(count) * 3);
            for (u32 i = 0; i < count * 3; ++i) {
                normals[i] = child.read<float>();
            }

        } else if (id == chunk_id::UV0L) {
            // UVs: u32 count followed by float[2] * count.
            u32 count = child.read<u32>();
            uvs.resize(static_cast<std::size_t>(count) * 2);
            for (u32 i = 0; i < count * 2; ++i) {
                uvs[i] = child.read<float>();
            }

        } else if (id == chunk_id::CLRL || id == chunk_id::CLRB) {
            // Vertex colors: u32 count followed by u32 * count (RGBA packed).
            u32 count = child.read<u32>();
            colors.resize(count);
            for (u32 i = 0; i < count; ++i) {
                colors[i] = child.read<u32>();
            }

        } else if (id == chunk_id::STRP) {
            // Triangle strips: u32 count followed by u16 * count.
            // Indices may have bit 15 set as a strip restart marker.
            u32 count = child.read<u32>();
            raw_strips.resize(count);
            for (u32 i = 0; i < count; ++i) {
                raw_strips[i] = child.read<u16>();
            }

        } else if (id == chunk_id::NDXT || id == chunk_id::NDXL) {
            // Pre-triangulated index list (some models use this instead of STRP).
            u32 count = child.read<u32>();
            segment.indices.resize(count);
            for (u32 i = 0; i < count; ++i) {
                segment.indices[i] = child.read<u16>();
            }
        }
    }

    // -----------------------------------------------------------------------
    // Assemble vertices from the separate attribute arrays.
    // -----------------------------------------------------------------------

    std::size_t vertex_count = positions.size() / 3;
    if (vertex_count == 0 && !normals.empty()) {
        vertex_count = normals.size() / 3;
    }

    segment.vertices.resize(vertex_count);

    for (std::size_t i = 0; i < vertex_count; ++i) {
        Vertex& v = segment.vertices[i];

        if (i * 3 + 2 < positions.size()) {
            v.position[0] = positions[i * 3 + 0];
            v.position[1] = positions[i * 3 + 1];
            v.position[2] = positions[i * 3 + 2];
        }

        if (i * 3 + 2 < normals.size()) {
            v.normal[0] = normals[i * 3 + 0];
            v.normal[1] = normals[i * 3 + 1];
            v.normal[2] = normals[i * 3 + 2];
        }

        if (i * 2 + 1 < uvs.size()) {
            v.uv[0] = uvs[i * 2 + 0];
            v.uv[1] = uvs[i * 2 + 1];
        }

        if (i < colors.size()) {
            v.color = colors[i];
        }
    }

    // -----------------------------------------------------------------------
    // Convert triangle strips to a triangle list (if we got STRP data and
    // don't already have a pre-built index list from NDXT/NDXL).
    // -----------------------------------------------------------------------

    if (!raw_strips.empty() && segment.indices.empty()) {
        segment.indices = convert_strips_to_triangles(raw_strips);
    }

    return segment;
}

// ---------------------------------------------------------------------------
// Triangle strip -> triangle list conversion
//
// SWBF uses the standard Direct3D / OpenGL triangle strip convention with
// a restart marker encoded in bit 15 of each index:
//
//   - If (index & 0x8000) != 0, this index starts a NEW strip. Clear the
//     high bit to get the real vertex index.
//   - Within a strip, each successive triple of indices forms a triangle.
//     Winding order alternates: even triangles use (i0, i1, i2), odd
//     triangles use (i1, i0, i2) to maintain consistent face orientation.
//   - Degenerate triangles (any two indices equal) are discarded.
// ---------------------------------------------------------------------------

std::vector<uint16_t> ModelLoader::convert_strips_to_triangles(
    const std::vector<uint16_t>& strips)
{
    std::vector<uint16_t> triangles;
    // Reserve a rough estimate (strips typically expand ~1.5-2x).
    triangles.reserve(strips.size() * 2);

    // State for the current strip.
    uint16_t buf[2] = {0, 0}; // sliding window of the two previous indices
    int strip_pos = 0;        // position within the current strip (0, 1, 2, ...)

    for (std::size_t i = 0; i < strips.size(); ++i) {
        uint16_t raw = strips[i];

        // Check for strip restart marker (bit 15 set).
        bool restart = (raw & 0x8000) != 0;
        uint16_t idx = raw & 0x7FFF; // actual vertex index

        if (restart || strip_pos == 0) {
            // Begin a new strip. The first index is just stored; we need at
            // least 3 indices before we can emit a triangle.
            buf[0] = idx;
            strip_pos = 1;
            continue;
        }

        if (strip_pos == 1) {
            buf[1] = idx;
            strip_pos = 2;
            continue;
        }

        // strip_pos >= 2: we have enough indices to form a triangle.
        uint16_t i0 = buf[0];
        uint16_t i1 = buf[1];
        uint16_t i2 = idx;

        // Skip degenerate triangles (two or more vertices the same).
        if (i0 != i1 && i1 != i2 && i0 != i2) {
            // Alternate winding for even/odd triangles within the strip.
            if ((strip_pos & 1) == 0) {
                // Even: (i0, i1, i2)
                triangles.push_back(i0);
                triangles.push_back(i1);
                triangles.push_back(i2);
            } else {
                // Odd: swap first two to maintain consistent winding
                triangles.push_back(i1);
                triangles.push_back(i0);
                triangles.push_back(i2);
            }
        }

        // Slide the window forward.
        buf[0] = i1;
        buf[1] = i2;
        ++strip_pos;
    }

    return triangles;
}

} // namespace swbf
