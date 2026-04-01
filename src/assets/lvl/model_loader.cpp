#include "model_loader.h"

#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <algorithm>
#include <cmath>
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
        // No MSH2 found. Check if this is the munged model format used by
        // soldier/vehicle models -- identified by lowercase 'segm' children
        // containing VBUF/IBUF/TNAM/SKIN/BMAP chunks.
        bool has_munged_segments = false;
        for (const auto& child : children) {
            if (child.id() == chunk_id::segm) {
                has_munged_segments = true;
                break;
            }
        }

        if (has_munged_segments) {
            LOG_DEBUG("ModelLoader: detected munged model format (segm children)");

            // First pass: extract bounding box and name (needed for VBUF
            // position decompression before we parse segments).
            for (auto& child : children) {
                FourCC id = child.id();
                if (id == chunk_id::SINF) {
                    parse_scene_info(child, model);
                } else if (id == chunk_id::NAME) {
                    if (model.name.empty()) {
                        model.name = child.read_string();
                    }
                } else if (id == chunk_id::BBOX) {
                    if (child.size() >= 44) {
                        child.skip(16); // skip quaternion
                        float cx = child.read<float>();
                        float cy = child.read<float>();
                        float cz = child.read<float>();
                        float ex = child.read<float>();
                        float ey = child.read<float>();
                        float ez = child.read<float>();
                        model.bounding_box.min[0] = cx - ex;
                        model.bounding_box.min[1] = cy - ey;
                        model.bounding_box.min[2] = cz - ez;
                        model.bounding_box.max[0] = cx + ex;
                        model.bounding_box.max[1] = cy + ey;
                        model.bounding_box.max[2] = cz + ez;
                    }
                } else if (id == chunk_id::MATL) {
                    parse_materials(child, model);
                }
            }

            // Second pass: parse munged segments (needs bbox for decompression).
            for (auto& child : children) {
                if (child.id() == chunk_id::segm) {
                    model.segments.push_back(parse_munged_segment(child, model));
                }
            }
            return model;
        }

        // Classic fallback: try treating the entire modl payload as MSH2 content.
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

                model.bounding_box.min[0] = cx - ex;
                model.bounding_box.min[1] = cy - ey;
                model.bounding_box.min[2] = cz - ez;

                model.bounding_box.max[0] = cx + ex;
                model.bounding_box.max[1] = cy + ey;
                model.bounding_box.max[2] = cz + ez;
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
            uint32_t c = colors[i];
            v.color[0] = static_cast<uint8_t>((c >>  0) & 0xFF);
            v.color[1] = static_cast<uint8_t>((c >>  8) & 0xFF);
            v.color[2] = static_cast<uint8_t>((c >> 16) & 0xFF);
            v.color[3] = static_cast<uint8_t>((c >> 24) & 0xFF);
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
// Munged segment parsing (segm -> VBUF, IBUF, TNAM, SKIN, BMAP)
// ---------------------------------------------------------------------------

MeshSegment ModelLoader::parse_munged_segment(ChunkReader segm,
                                              const Model& model) {
    MeshSegment segment;

    auto children = segm.get_children();

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == chunk_id::VBUF) {
            decode_vbuf(child, segment, model);

        } else if (id == chunk_id::IBUF) {
            // Index buffer: u32 count, then u16[count] triangle indices.
            u32 count = child.read<u32>();
            segment.indices.resize(count);
            for (u32 i = 0; i < count; ++i) {
                segment.indices[i] = child.read<u16>();
            }

        } else if (id == chunk_id::TNAM) {
            segment.texture_name = child.read_string();

        } else if (id == chunk_id::MTRL) {
            segment.material_index = child.read<u32>();

        } else if (id == chunk_id::SKIN) {
            // Skin data: raw bone index bytes. The size relative to
            // vertex count tells us how many bones per vertex.
            std::size_t remaining = child.remaining();
            if (remaining > 0 && !segment.vertices.empty()) {
                std::size_t vert_count = segment.vertices.size();

                if (remaining == vert_count) {
                    // 1 bone per vertex
                    segment.skin_vertices.resize(vert_count);
                    for (std::size_t i = 0; i < vert_count; ++i) {
                        segment.skin_vertices[i].bone_indices[0] = child.read<u8>();
                        segment.skin_vertices[i].bone_weights[0] = 1.0f;
                    }
                } else if (remaining == vert_count * 4) {
                    // 4 bone indices per vertex
                    segment.skin_vertices.resize(vert_count);
                    for (std::size_t i = 0; i < vert_count; ++i) {
                        for (int j = 0; j < MAX_BONE_INFLUENCES; ++j) {
                            segment.skin_vertices[i].bone_indices[j] =
                                child.read<u8>();
                        }
                        int active = 0;
                        for (int j = 0; j < MAX_BONE_INFLUENCES; ++j) {
                            if (segment.skin_vertices[i].bone_indices[j] != 0
                                || j == 0) {
                                ++active;
                            }
                        }
                        float w = 1.0f / static_cast<float>(active);
                        for (int j = 0; j < active; ++j) {
                            segment.skin_vertices[i].bone_weights[j] = w;
                        }
                    }
                }
            }

        } else if (id == chunk_id::BMAP) {
            // Bone mapping: local -> skeleton-global bone index array.
            std::size_t count = child.remaining() / sizeof(u32);
            segment.bone_map.resize(count);
            for (std::size_t i = 0; i < count; ++i) {
                segment.bone_map[i] = static_cast<int>(child.read<u32>());
            }
        }
    }

    return segment;
}

// ---------------------------------------------------------------------------
// VBUF decoding -- vertex buffer with flag-driven attribute layout
//
// Header: u32 vertex_count, u32 stride, u32 flags
// ---------------------------------------------------------------------------

namespace {

constexpr u32 VBUF_POSITION        = 0x0002;
constexpr u32 VBUF_BONE_INDICES    = 0x0004;
constexpr u32 VBUF_BONE_WEIGHTS    = 0x0008;
constexpr u32 VBUF_NORMAL          = 0x0020;
constexpr u32 VBUF_TANGENTS        = 0x0040;
constexpr u32 VBUF_COLOR           = 0x0080;
constexpr u32 VBUF_STATIC_LIGHT    = 0x0100;
constexpr u32 VBUF_TEXCOORD        = 0x0200;
constexpr u32 VBUF_POS_COMPRESSED  = 0x1000;
constexpr u32 VBUF_BONE_COMPRESSED = 0x2000;
constexpr u32 VBUF_NRM_COMPRESSED  = 0x4000;
constexpr u32 VBUF_UV_COMPRESSED   = 0x8000;

} // anonymous namespace

void ModelLoader::decode_vbuf(ChunkReader vbuf, MeshSegment& segment,
                              const Model& model) {
    if (vbuf.remaining() < 12) {
        LOG_WARN("ModelLoader: VBUF chunk too small (%zu bytes)",
                 vbuf.remaining());
        return;
    }

    u32 vertex_count = vbuf.read<u32>();
    u32 stride       = vbuf.read<u32>();
    u32 flags        = vbuf.read<u32>();

    if (vertex_count == 0 || stride == 0) {
        return;
    }

    LOG_DEBUG("ModelLoader: VBUF vertex_count=%u stride=%u flags=0x%04X",
              vertex_count, stride, flags);

    // Bounding box range for compressed position decompression.
    float bbox_range[3] = {
        model.bounding_box.max[0] - model.bounding_box.min[0],
        model.bounding_box.max[1] - model.bounding_box.min[1],
        model.bounding_box.max[2] - model.bounding_box.min[2],
    };
    constexpr float INT16_RANGE =
        static_cast<float>(INT16_MAX) - static_cast<float>(INT16_MIN);

    bool has_position     = (flags & VBUF_POSITION)        != 0;
    bool has_bone_indices = (flags & VBUF_BONE_INDICES)    != 0;
    bool has_bone_weights = (flags & VBUF_BONE_WEIGHTS)    != 0;
    bool has_normal       = (flags & VBUF_NORMAL)          != 0;
    bool has_tangents     = (flags & VBUF_TANGENTS)        != 0;
    bool has_color        = (flags & VBUF_COLOR)           != 0;
    bool has_static_light = (flags & VBUF_STATIC_LIGHT)    != 0;
    bool has_texcoord     = (flags & VBUF_TEXCOORD)        != 0;
    bool pos_compressed   = (flags & VBUF_POS_COMPRESSED)  != 0;
    bool bone_compressed  = (flags & VBUF_BONE_COMPRESSED) != 0;
    bool nrm_compressed   = (flags & VBUF_NRM_COMPRESSED)  != 0;
    bool uv_compressed    = (flags & VBUF_UV_COMPRESSED)   != 0;

    bool has_skin = has_bone_indices || has_bone_weights;

    segment.vertices.resize(vertex_count);
    if (has_skin) {
        segment.skin_vertices.resize(vertex_count);
    }

    // Raw vertex data starts after the 12-byte header we already consumed.
    const u8* raw_data = vbuf.data() + 12;
    std::size_t data_available = vbuf.remaining();

    for (u32 vi = 0; vi < vertex_count; ++vi) {
        Vertex& v = segment.vertices[vi];
        std::size_t off = 0;

        if (static_cast<std::size_t>(vi) * stride + stride > data_available) {
            LOG_WARN("ModelLoader: VBUF data truncated at vertex %u/%u",
                     vi, vertex_count);
            segment.vertices.resize(vi);
            if (has_skin) segment.skin_vertices.resize(vi);
            break;
        }

        const u8* vp = raw_data + static_cast<std::size_t>(vi) * stride;

        // --- Position ---
        if (has_position) {
            if (pos_compressed) {
                i16 cx, cy, cz;
                std::memcpy(&cx, vp + off, 2); off += 2;
                std::memcpy(&cy, vp + off, 2); off += 2;
                std::memcpy(&cz, vp + off, 2); off += 2;
                off += 2; // skip w

                v.position[0] = model.bounding_box.min[0] +
                    (static_cast<float>(cx) - static_cast<float>(INT16_MIN)) *
                    bbox_range[0] / INT16_RANGE;
                v.position[1] = model.bounding_box.min[1] +
                    (static_cast<float>(cy) - static_cast<float>(INT16_MIN)) *
                    bbox_range[1] / INT16_RANGE;
                v.position[2] = model.bounding_box.min[2] +
                    (static_cast<float>(cz) - static_cast<float>(INT16_MIN)) *
                    bbox_range[2] / INT16_RANGE;
            } else {
                std::memcpy(&v.position[0], vp + off, 4); off += 4;
                std::memcpy(&v.position[1], vp + off, 4); off += 4;
                std::memcpy(&v.position[2], vp + off, 4); off += 4;
            }
        }

        // --- Bone indices ---
        if (has_bone_indices) {
            SkinVertex& sv = segment.skin_vertices[vi];
            if (bone_compressed) {
                sv.bone_indices[0] = *(vp + off); off += 1;
                sv.bone_indices[1] = *(vp + off); off += 1;
                sv.bone_weights[0] = 1.0f;
            } else {
                sv.bone_indices[0] = *(vp + off); off += 1;
                sv.bone_indices[1] = *(vp + off); off += 1;
                sv.bone_indices[2] = *(vp + off); off += 1;
                sv.bone_indices[3] = *(vp + off); off += 1;
            }
        }

        // --- Bone weights ---
        if (has_bone_weights) {
            SkinVertex& sv = segment.skin_vertices[vi];
            if (bone_compressed) {
                u8 w0 = *(vp + off); off += 1;
                u8 w1 = *(vp + off); off += 1;
                sv.bone_weights[0] = static_cast<float>(w0) / 255.0f;
                sv.bone_weights[1] = static_cast<float>(w1) / 255.0f;
                float wsum = sv.bone_weights[0] + sv.bone_weights[1];
                if (wsum > 0.0f) {
                    sv.bone_weights[0] /= wsum;
                    sv.bone_weights[1] /= wsum;
                } else {
                    sv.bone_weights[0] = 1.0f;
                }
            } else {
                for (int bi = 0; bi < MAX_BONE_INFLUENCES; ++bi) {
                    std::memcpy(&sv.bone_weights[bi], vp + off, 4);
                    off += 4;
                }
            }
        }

        // --- Normal ---
        if (has_normal) {
            if (nrm_compressed) {
                u32 packed;
                std::memcpy(&packed, vp + off, 4); off += 4;

                u8 bz = static_cast<u8>((packed >>  0) & 0xFF);
                u8 by = static_cast<u8>((packed >>  8) & 0xFF);
                u8 bx = static_cast<u8>((packed >> 16) & 0xFF);

                v.normal[0] = (static_cast<float>(bx) / 255.0f) * 2.0f - 1.0f;
                v.normal[1] = (static_cast<float>(by) / 255.0f) * 2.0f - 1.0f;
                v.normal[2] = (static_cast<float>(bz) / 255.0f) * 2.0f - 1.0f;
            } else {
                std::memcpy(&v.normal[0], vp + off, 4); off += 4;
                std::memcpy(&v.normal[1], vp + off, 4); off += 4;
                std::memcpy(&v.normal[2], vp + off, 4); off += 4;
            }
        }

        // --- Tangent / Bitangent (skip, not stored in Vertex) ---
        if (has_tangents) {
            if (nrm_compressed) {
                off += 4; // compressed tangent
                off += 4; // compressed bitangent
            } else {
                off += 12; // float[3] tangent
                off += 12; // float[3] bitangent
            }
        }

        // --- Vertex color ---
        if (has_color) {
            std::memcpy(&v.color, vp + off, 4); off += 4;
        }

        // --- Static lighting color ---
        if (has_static_light) {
            if (!has_color) {
                std::memcpy(&v.color, vp + off, 4);
            }
            off += 4;
        }

        // --- Texture coordinates ---
        if (has_texcoord) {
            if (uv_compressed) {
                i16 cu, cv;
                std::memcpy(&cu, vp + off, 2); off += 2;
                std::memcpy(&cv, vp + off, 2); off += 2;
                v.uv[0] = static_cast<float>(cu) / 2048.0f;
                v.uv[1] = static_cast<float>(cv) / 2048.0f;
            } else {
                std::memcpy(&v.uv[0], vp + off, 4); off += 4;
                std::memcpy(&v.uv[1], vp + off, 4); off += 4;
            }
        }

        (void)off; // stride handles total vertex advancement
    }
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
