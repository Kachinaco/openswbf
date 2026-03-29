#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// TerrainData — parsed terrain from a tern UCFB chunk.
//
// SWBF terrain is a regular heightfield grid. The tern chunk contains:
//
//   - Grid dimensions and scale parameters
//   - A heightmap (one float per vertex)
//   - Per-vertex RGBA colors (baked lighting / vertex paint)
//   - Texture blend weights for up to 16 terrain layers
//   - Texture names for each layer
//
// The grid is (grid_size x grid_size) vertices. World-space position of
// vertex (x, z) is:
//
//   pos.x = (x - grid_size/2) * grid_scale
//   pos.y = heights[z * grid_size + x] * height_scale
//   pos.z = (z - grid_size/2) * grid_scale
// ---------------------------------------------------------------------------

struct TerrainData {
    uint32_t grid_size    = 0;     // Vertices per side (e.g. 128, 256, 512)
    float    grid_scale   = 1.0f;  // Horizontal spacing between vertices
    float    height_scale = 1.0f;  // Vertical multiplier for height values

    // Heightmap — grid_size * grid_size floats, row-major order.
    std::vector<float> heights;

    // Per-vertex RGBA color — grid_size * grid_size uint32s (packed RGBA).
    std::vector<uint32_t> colors;

    // Texture blend weights per layer. Up to 16 layers, each containing
    // grid_size * grid_size bytes (0-255 weight per vertex).
    std::vector<std::vector<uint8_t>> texture_weights;

    // Texture names for each layer (indices correspond to texture_weights).
    std::vector<std::string> texture_names;
};

// ---------------------------------------------------------------------------
// TerrainLoader — parses tern UCFB chunks into TerrainData.
//
// SWBF2 tern chunk structure (observed from nab2.lvl):
//
//   tern
//     NAME — terrain name
//     INFO (28 bytes) — grid_scale, height_scale, unknowns, patches_per_axis
//     PCHS — patch set container
//       COMN — common index buffer (shared)
//       PTCH (repeated) — individual terrain patch
//         INFO — patch metadata (LOD flags)
//         VBUF (stride=16, flags=0x5122) — texture/blend data
//         VBUF (stride=28, flags=0x122)  — geometry: pos + normal + color
//       LOWR — lower LOD (optional)
//     LTEX — texture layer name (one per layer, in order)
//     DTLX — detail texture info
//     DTEX — diffuse texture info
//
// Patches are 9x9 vertices (8x8 cells) with local coordinates.
// They are stored sequentially in row-major order across a
// patches_per_axis x patches_per_axis grid.
// ---------------------------------------------------------------------------

class TerrainLoader {
public:
    TerrainLoader() = default;

    // Parse a tern chunk and return the decoded terrain.
    TerrainData load(ChunkReader& chunk);

private:
    // Parse an LTEX chunk to extract a texture layer name.
    std::string parse_ltex(ChunkReader& ltex);
};

} // namespace swbf
