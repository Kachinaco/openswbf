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
constexpr FourCC LTEX = make_fourcc('L', 'T', 'E', 'X');
constexpr FourCC DTLX = make_fourcc('D', 'T', 'L', 'X');
constexpr FourCC DTEX = make_fourcc('D', 'T', 'E', 'X');
constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');

// VBUF flags for the geometry buffer (position + normal + color).
constexpr uint32_t VBUF_FLAGS_GEOMETRY = 0x122;

// Cells per patch side (each patch is a 9x9 vertex grid = 8x8 cells).
constexpr uint32_t CELLS_PER_PATCH = 8;

} // anonymous namespace

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
        try {
            ChunkReader child = ltex.next_child();
            if (child.id() == NAME) {
                return child.read_string();
            }
        } catch (...) {
            // Not a valid sub-chunk — payload is a raw string.
        }
    }

    // Read the payload directly as a null-terminated string.
    const uint8_t* data = ltex.data();
    u32 size = ltex.size();

    std::size_t len = 0;
    while (len < size && data[len] != '\0') {
        ++len;
    }
    return std::string(reinterpret_cast<const char*>(data), len);
}

// ---------------------------------------------------------------------------
// load — main entry point: parse a tern chunk
//
// SWBF2 tern chunk hierarchy (observed from nab2.lvl):
//
//   tern
//     NAME (7 bytes) — terrain name
//     INFO (28 bytes) — header:
//       offset  0: float grid_scale    (e.g. 4.0 — meters per cell)
//       offset  4: float height_scale  (e.g. 0.06 — vertical multiplier)
//       offset  8: float unknown       (e.g. 1.5)
//       offset 12: float unknown       (e.g. 7.86 — default height?)
//       offset 16: uint32 grid_info    (e.g. 0x80080)
//       offset 20: uint32 patch_info   (e.g. 0x10010)
//       offset 24: uint32 patches_per_axis (e.g. 16)
//     LTEX — texture layer name
//     DTEX / DTLX — texture blend info
//     PCHS — patch set container:
//       COMN — common/default index buffer (shared by all patches)
//       PTCH (repeated, patches_per_axis^2 times) — terrain patches:
//         INFO (8 bytes) — patch metadata (LOD flags, not grid position)
//         VBUF (stride=16, flags=0x5122) — texture/blend vertex data
//         VBUF (stride=28, flags=0x122)  — geometry vertex data:
//           81 vertices (9x9 grid), each: vec3 pos + vec3 normal + u32 color
//       LOWR — lower LOD data (optional, skipped)
//
// Patches are stored sequentially in row-major order. Each patch covers
// an 8x8 cell region with 9x9 vertices. Vertex positions are LOCAL to
// the patch (x,z in [0..32] for grid_scale=4). The patch's grid position
// is derived from its sequential index:
//   patch_x = index % patches_per_axis
//   patch_z = index / patches_per_axis
//
// The full terrain grid is (patches_per_axis * 8 + 1) vertices per side.
// Adjacent patches share edge vertices.
// ---------------------------------------------------------------------------

TerrainData TerrainLoader::load(ChunkReader& chunk) {
    TerrainData terrain;

    if (chunk.id() != chunk_id::tern) {
        LOG_WARN("TerrainLoader: expected tern chunk, got 0x%08X", chunk.id());
        return terrain;
    }

    // Collect all top-level children first so we can process INFO before PCHS.
    std::vector<ChunkReader> children = chunk.get_children();

    // -----------------------------------------------------------------------
    // Pass 1: Find INFO and extract grid parameters.
    // -----------------------------------------------------------------------
    uint32_t patches_per_axis = 0;

    for (auto& child : children) {
        if (child.id() != INFO) continue;

        // SWBF2 tern INFO is 28 bytes:
        //   [0]  float grid_scale
        //   [4]  float height_scale
        //   [8]  float unknown
        //   [12] float unknown (default height)
        //   [16] uint32 grid_info
        //   [20] uint32 patch_info
        //   [24] uint32 patches_per_axis
        if (child.remaining() >= 28) {
            terrain.grid_scale   = child.read<float>();  // offset 0
            terrain.height_scale = child.read<float>();  // offset 4
            child.skip(8);                               // offsets 8, 12 (unknowns)
            child.skip(8);                               // offsets 16, 20 (grid/patch info)
            patches_per_axis = child.read<uint32_t>();   // offset 24
        } else if (child.remaining() >= 8) {
            // Fallback for shorter INFO: just read scale values.
            terrain.grid_scale   = child.read<float>();
            terrain.height_scale = child.read<float>();
        }
        break;
    }

    // Compute grid dimensions from patches.
    if (patches_per_axis == 0) {
        LOG_WARN("TerrainLoader: could not determine patches_per_axis from INFO");
        return terrain;
    }

    terrain.grid_size = patches_per_axis * CELLS_PER_PATCH + 1;
    std::size_t total_verts = static_cast<std::size_t>(terrain.grid_size) * terrain.grid_size;
    terrain.heights.resize(total_verts, 0.0f);
    terrain.colors.resize(total_verts, 0xFFFFFFFF);

    LOG_DEBUG("TerrainLoader: INFO — patches_per_axis=%u, grid_size=%u, "
              "grid_scale=%.2f, height_scale=%.2f",
              patches_per_axis, terrain.grid_size,
              static_cast<double>(terrain.grid_scale),
              static_cast<double>(terrain.height_scale));

    // -----------------------------------------------------------------------
    // Pass 2: Process PCHS to extract patch vertex data.
    // -----------------------------------------------------------------------
    for (auto& child : children) {
        if (child.id() == PCHS) {
            uint32_t ptch_index = 0;

            while (child.has_children()) {
                ChunkReader pchs_child = child.next_child();

                // Skip non-PTCH children (COMN, LOWR, etc.)
                if (pchs_child.id() != PTCH) continue;

                // Compute this patch's position in the grid.
                uint32_t patch_x = ptch_index % patches_per_axis;
                uint32_t patch_z = ptch_index / patches_per_axis;

                // Base vertex coordinates in the global grid for this patch.
                uint32_t base_gx = patch_x * CELLS_PER_PATCH;
                uint32_t base_gz = patch_z * CELLS_PER_PATCH;

                // Walk PTCH children looking for the geometry VBUF.
                while (pchs_child.has_children()) {
                    ChunkReader ptch_child = pchs_child.next_child();
                    if (ptch_child.id() != VBUF) continue;

                    if (ptch_child.remaining() < 12) continue;

                    uint32_t vertex_count = ptch_child.read<uint32_t>();
                    uint32_t stride       = ptch_child.read<uint32_t>();
                    uint32_t flags        = ptch_child.read<uint32_t>();

                    // We only want the geometry VBUF (stride=28, flags=0x122).
                    if (flags != VBUF_FLAGS_GEOMETRY || stride < 28) continue;
                    if (vertex_count == 0) continue;

                    std::size_t data_bytes = static_cast<std::size_t>(vertex_count) * stride;
                    if (ptch_child.remaining() < data_bytes) {
                        LOG_WARN("TerrainLoader: PTCH[%u] VBUF too small (%zu < %zu)",
                                 ptch_index, ptch_child.remaining(), data_bytes);
                        continue;
                    }

                    // Read each vertex: vec3 position (12) + vec3 normal (12) + u32 color (4).
                    // Positions are LOCAL to the patch: x,z in [0..cells*grid_scale].
                    // We convert local position to global grid indices.
                    for (uint32_t v = 0; v < vertex_count; ++v) {
                        float px = ptch_child.read<float>();
                        float py = ptch_child.read<float>();
                        float pz = ptch_child.read<float>();

                        // Skip normal.
                        ptch_child.skip(12);

                        uint32_t color = ptch_child.read<uint32_t>();

                        // Skip any extra stride bytes beyond the 28 we read.
                        if (stride > 28) {
                            ptch_child.skip(stride - 28);
                        }

                        // Convert local position to grid index.
                        // Local coords: x in [0..CELLS_PER_PATCH*grid_scale]
                        // Grid index: base + local / grid_scale
                        int local_gx = static_cast<int>(px / terrain.grid_scale + 0.5f);
                        int local_gz = static_cast<int>(pz / terrain.grid_scale + 0.5f);

                        int gx = static_cast<int>(base_gx) + local_gx;
                        int gz = static_cast<int>(base_gz) + local_gz;

                        if (gx >= 0 && gx < static_cast<int>(terrain.grid_size) &&
                            gz >= 0 && gz < static_cast<int>(terrain.grid_size)) {
                            std::size_t idx = static_cast<std::size_t>(gz) * terrain.grid_size +
                                              static_cast<std::size_t>(gx);
                            terrain.heights[idx] = py;
                            terrain.colors[idx]  = color;
                        }
                    }
                }

                ptch_index++;
            }

            LOG_DEBUG("TerrainLoader: processed %u patches", ptch_index);
        }
        else if (child.id() == LTEX) {
            std::string tex_name = parse_ltex(child);
            if (!tex_name.empty()) {
                terrain.texture_names.push_back(tex_name);
                LOG_TRACE("TerrainLoader: texture layer %zu = '%s'",
                          terrain.texture_names.size() - 1, tex_name.c_str());
            }
        }
        else if (child.id() == DTLX || child.id() == DTEX) {
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
                          (child.id() == DTLX) ? "DTLX" : "DTEX");
            }
        }
    }

    LOG_INFO("TerrainLoader: grid=%u, scale=%.2f, height_scale=%.2f, "
             "%zu texture layers, %zu weight layers",
             terrain.grid_size, static_cast<double>(terrain.grid_scale),
             static_cast<double>(terrain.height_scale),
             terrain.texture_names.size(), terrain.texture_weights.size());

    return terrain;
}

} // namespace swbf
