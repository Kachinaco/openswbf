#include "terrain_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"
#include "core/types.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace swbf {

// ---------------------------------------------------------------------------
// Sub-chunk FourCCs used inside tern chunks
// ---------------------------------------------------------------------------

namespace {

constexpr FourCC INFO = make_fourcc('I', 'N', 'F', 'O');
constexpr FourCC PCHS = make_fourcc('P', 'C', 'H', 'S');
constexpr FourCC PTCH = make_fourcc('P', 'T', 'C', 'H');
constexpr FourCC VBUF = make_fourcc('V', 'B', 'U', 'F');
constexpr FourCC IBUF = make_fourcc('I', 'B', 'U', 'F');
constexpr FourCC LTEX = make_fourcc('L', 'T', 'E', 'X');
constexpr FourCC DTLX = make_fourcc('D', 'T', 'L', 'X');
constexpr FourCC DTEX = make_fourcc('D', 'T', 'E', 'X');
constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');

// VBUF element types that we recognize.
// Type 290 (0x122) = position(vec3) + normal(vec3) + color(u32) = 28 bytes/vertex
// Type 34  (0x022) = position(vec3) + normal(vec3) = 24 bytes/vertex
// Type 2   (0x002) = position(vec3) = 12 bytes/vertex
constexpr uint32_t VBUF_TYPE_POS_NRM_CLR = 290;  // 0x122
constexpr uint32_t VBUF_TYPE_POS_NRM     = 34;   // 0x022
constexpr uint32_t VBUF_TYPE_POS         = 2;     // 0x002

} // anonymous namespace

// ---------------------------------------------------------------------------
// parse_vbuf — extract height and color data from a terrain vertex buffer
//
// VBUF chunk layout:
//   u32 vertex_count
//   u32 stride (bytes per vertex)
//   u32 type_flags
//   [vertex data: vertex_count * stride bytes]
//
// For type 290 (the most common terrain VBUF), each vertex is:
//   float pos_x, pos_y, pos_z   (12 bytes)
//   float nrm_x, nrm_y, nrm_z  (12 bytes)
//   u32   color                  ( 4 bytes)
//   Total: 28 bytes
//
// We extract pos_y as the height value and the packed color. The x/z
// positions define the grid placement and are used to compute the vertex
// index into the flat height/color arrays.
// ---------------------------------------------------------------------------

void TerrainLoader::parse_vbuf(ChunkReader& vbuf, TerrainData& terrain) {
    if (vbuf.remaining() < 12) {
        LOG_WARN("TerrainLoader: VBUF chunk too small (%zu bytes)", vbuf.remaining());
        return;
    }

    uint32_t vertex_count = vbuf.read<uint32_t>();
    uint32_t stride       = vbuf.read<uint32_t>();
    uint32_t type_flags   = vbuf.read<uint32_t>();

    if (vertex_count == 0 || stride == 0) {
        return;
    }

    const std::size_t data_bytes = static_cast<std::size_t>(vertex_count) * stride;
    if (vbuf.remaining() < data_bytes) {
        LOG_WARN("TerrainLoader: VBUF declares %u vertices * %u stride = %zu bytes, "
                 "but only %zu remaining",
                 vertex_count, stride, data_bytes, vbuf.remaining());
        return;
    }

    // Determine what fields are present based on type_flags.
    // Position is always present (minimum 12 bytes per vertex).
    bool has_normal = (type_flags == VBUF_TYPE_POS_NRM || type_flags == VBUF_TYPE_POS_NRM_CLR);
    bool has_color  = (type_flags == VBUF_TYPE_POS_NRM_CLR);

    // Minimum expected stride based on detected fields.
    uint32_t expected_stride = 12;  // vec3 position
    if (has_normal) expected_stride += 12;
    if (has_color)  expected_stride += 4;

    if (stride < expected_stride) {
        // Unknown vertex format — try to handle gracefully.
        // If stride is at least 12, we can still extract positions.
        has_normal = (stride >= 24);
        has_color  = (stride >= 28);
    }

    // If the terrain grid isn't sized yet, estimate from vertex count.
    // Terrain patches contribute partial vertex data; we accumulate
    // into the arrays and use grid_size to address them.
    if (terrain.grid_size == 0) {
        // Try to infer grid size: common sizes are powers of 2 plus 1
        // (e.g. 129, 257, 513). If vertex_count is a perfect square
        // of such a number, use it.
        uint32_t side = static_cast<uint32_t>(std::sqrt(static_cast<double>(vertex_count)));
        if (side * side == vertex_count) {
            terrain.grid_size = side;
            terrain.heights.resize(static_cast<std::size_t>(side) * side, 0.0f);
            terrain.colors.resize(static_cast<std::size_t>(side) * side, 0xFFFFFFFF);
        }
    }

    // Read vertices and populate height/color arrays.
    for (uint32_t i = 0; i < vertex_count; ++i) {
        // Read position (always present).
        float px = vbuf.read<float>();
        float py = vbuf.read<float>();
        float pz = vbuf.read<float>();

        // Read normal (if present).
        if (has_normal) {
            vbuf.skip(12); // skip normal vec3
        }

        // Read color (if present).
        uint32_t color = 0xFFFFFFFF;
        if (has_color) {
            color = vbuf.read<uint32_t>();
        }

        // Skip any remaining bytes in this vertex's stride.
        uint32_t consumed = 12;
        if (has_normal) consumed += 12;
        if (has_color)  consumed += 4;
        if (stride > consumed) {
            vbuf.skip(stride - consumed);
        }

        // Map world-space position to grid index.
        // The terrain is centered at the origin, so grid coordinates are:
        //   gx = (px / grid_scale) + grid_size/2
        //   gz = (pz / grid_scale) + grid_size/2
        if (terrain.grid_size > 0 && terrain.grid_scale > 0.0f) {
            int gx = static_cast<int>(px / terrain.grid_scale + terrain.grid_size * 0.5f + 0.5f);
            int gz = static_cast<int>(pz / terrain.grid_scale + terrain.grid_size * 0.5f + 0.5f);

            if (gx >= 0 && gx < static_cast<int>(terrain.grid_size) &&
                gz >= 0 && gz < static_cast<int>(terrain.grid_size)) {
                std::size_t idx = static_cast<std::size_t>(gz) * terrain.grid_size + gx;
                terrain.heights[idx] = py;
                terrain.colors[idx]  = color;
            }
        } else if (i < terrain.heights.size()) {
            // Fallback: store sequentially if we cannot compute grid coords.
            terrain.heights[i] = py;
            terrain.colors[i]  = color;
        }
    }
}

// ---------------------------------------------------------------------------
// parse_ltex — extract a texture layer name from an LTEX chunk
//
// LTEX chunks contain a single null-terminated texture name string.
// They appear as direct children of the tern chunk, one per texture layer,
// in order (layer 0 first).
// ---------------------------------------------------------------------------

std::string TerrainLoader::parse_ltex(ChunkReader& ltex) {
    // The LTEX chunk may contain child chunks or a direct string payload.
    // First, check if there is a NAME sub-chunk.
    if (ltex.remaining() >= 8) {
        // Peek at the first 4 bytes to see if it looks like a FourCC.
        // If the first child is a NAME chunk, read from that; otherwise
        // treat the entire payload as a null-terminated string.
        try {
            ChunkReader child = ltex.next_child();
            if (child.id() == NAME) {
                return child.read_string();
            }
            // Not a NAME chunk — fall through to read as raw string.
        } catch (...) {
            // Not a valid sub-chunk — payload is a raw string.
        }
    }

    // Read the payload directly as a null-terminated string.
    // Re-construct a reader to start from the beginning of the payload.
    const uint8_t* data = ltex.data();
    u32 size = ltex.size();

    // Find null terminator.
    std::size_t len = 0;
    while (len < size && data[len] != '\0') {
        ++len;
    }
    return std::string(reinterpret_cast<const char*>(data), len);
}

// ---------------------------------------------------------------------------
// load — main entry point: parse a tern chunk
//
// The tern chunk hierarchy:
//
//   tern
//     INFO  — header with grid dimensions and scale factors
//     PCHS  — patch set (container)
//       PTCH  — individual terrain patch
//         INFO  — patch position, LOD info
//         VBUF  — vertex buffer for this patch
//         IBUF  — index buffer for this patch (triangle list/strip)
//     LTEX  — texture layer name (repeated, one per layer)
//     DTLX  — detail texture info (optional)
//     DTEX  — diffuse texture info (optional)
//
// We combine all patches' VBUF data into a single flat heightmap and color
// array. Texture layer names are collected from LTEX children.
// ---------------------------------------------------------------------------

TerrainData TerrainLoader::load(ChunkReader& chunk) {
    TerrainData terrain;

    if (chunk.id() != chunk_id::tern) {
        LOG_WARN("TerrainLoader: expected tern chunk, got 0x%08X", chunk.id());
        return terrain;
    }

    // First pass: walk all children of the tern chunk.
    while (chunk.has_children()) {
        ChunkReader child = chunk.next_child();
        FourCC id = child.id();

        if (id == INFO) {
            // tern INFO layout (common variant):
            //   u32 version           (typically 21 or 22)
            //   u16 extent_min_x      \
            //   u16 extent_min_z       |  grid extent in patch units
            //   u16 extent_max_x       |
            //   u16 extent_max_z      /
            //   u32 unused / reserved
            //   f32 grid_scale
            //   f32 height_scale
            //   u32 grid_size_plus_1
            //   ... (further optional fields)
            //
            // Some LVL files use a simpler layout:
            //   u32 grid_size
            //   f32 grid_scale
            //   f32 height_scale
            //   ... (followed by additional data)
            //
            // We handle both by checking available bytes.

            if (child.remaining() >= 12) {
                uint32_t first_u32 = child.read<uint32_t>();

                // Heuristic: if first_u32 is a small value (< 1000), it's
                // likely a version or grid_size. If it's very large, something
                // is wrong.
                if (first_u32 >= 21 && first_u32 <= 30 && child.remaining() >= 20) {
                    // Versioned format: skip extents (8 bytes) and reserved (4 bytes).
                    child.skip(12);  // 4*u16 extents + u32 reserved
                    terrain.grid_scale   = child.read<float>();
                    terrain.height_scale = child.read<float>();

                    if (child.remaining() >= 4) {
                        uint32_t gs_plus_1 = child.read<uint32_t>();
                        terrain.grid_size = gs_plus_1 > 0 ? gs_plus_1 - 1 : 0;

                        // Correct: grid_size might actually be the full value.
                        // If it's a power-of-2 + 1, use it directly.
                        if ((gs_plus_1 & (gs_plus_1 - 1)) == 0) {
                            // gs_plus_1 is a power of 2 — treat it as grid_size.
                            terrain.grid_size = gs_plus_1;
                        }
                    }
                } else {
                    // Simpler format: first u32 is grid_size directly.
                    terrain.grid_size    = first_u32;
                    terrain.grid_scale   = child.read<float>();
                    terrain.height_scale = child.read<float>();
                }
            }

            // Allocate height/color arrays now that we know the grid size.
            if (terrain.grid_size > 0) {
                std::size_t total = static_cast<std::size_t>(terrain.grid_size) * terrain.grid_size;
                terrain.heights.resize(total, 0.0f);
                terrain.colors.resize(total, 0xFFFFFFFF);
            }

            LOG_DEBUG("TerrainLoader: INFO — grid_size=%u, grid_scale=%.2f, height_scale=%.2f",
                      terrain.grid_size, terrain.grid_scale, terrain.height_scale);
        }
        else if (id == PCHS) {
            // PCHS is a container of PTCH (patch) children.
            while (child.has_children()) {
                ChunkReader ptch = child.next_child();

                if (ptch.id() != PTCH) continue;

                // Inside each PTCH, look for VBUF chunks.
                while (ptch.has_children()) {
                    ChunkReader ptch_child = ptch.next_child();

                    if (ptch_child.id() == VBUF) {
                        parse_vbuf(ptch_child, terrain);
                    }
                    // We skip IBUF and patch INFO for now — index data is
                    // needed for rendering but the heightmap can be
                    // reconstructed from VBUF positions alone.
                }
            }
        }
        else if (id == LTEX) {
            // Each LTEX child represents one texture layer name.
            std::string tex_name = parse_ltex(child);
            if (!tex_name.empty()) {
                terrain.texture_names.push_back(tex_name);
                LOG_TRACE("TerrainLoader: texture layer %zu = '%s'",
                          terrain.texture_names.size() - 1, tex_name.c_str());
            }
        }
        else if (id == DTLX || id == DTEX) {
            // Detail / diffuse texture info — these contain blend weight data
            // for the texture layers. Layout:
            //   For each layer: grid_size * grid_size bytes of weights (0-255).
            //
            // We store them in texture_weights.
            if (terrain.grid_size > 0) {
                std::size_t layer_size = static_cast<std::size_t>(terrain.grid_size) *
                                         terrain.grid_size;
                const uint8_t* payload = child.data();
                std::size_t payload_size = child.size();

                std::size_t num_layers = payload_size / layer_size;
                if (num_layers > 16) num_layers = 16;

                for (std::size_t layer = 0; layer < num_layers; ++layer) {
                    const uint8_t* layer_data = payload + layer * layer_size;
                    terrain.texture_weights.emplace_back(layer_data,
                                                         layer_data + layer_size);
                }

                LOG_DEBUG("TerrainLoader: loaded %zu texture weight layers from %s",
                          num_layers,
                          (id == DTLX) ? "DTLX" : "DTEX");
            }
        }
    }

    LOG_INFO("TerrainLoader: grid=%u, scale=%.2f, height_scale=%.2f, "
             "%zu texture layers, %zu weight layers",
             terrain.grid_size, terrain.grid_scale, terrain.height_scale,
             terrain.texture_names.size(), terrain.texture_weights.size());

    return terrain;
}

} // namespace swbf
