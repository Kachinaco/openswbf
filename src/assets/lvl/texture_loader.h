#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// Texture pixel format identifiers.
//
// These correspond to the D3DFORMAT values stored in SWBF's tex_ chunks.
// The game uses DirectX 8/9-era texture formats; we translate them to raw
// RGBA8 during loading.
// ---------------------------------------------------------------------------

enum class TextureFormat : uint32_t {
    RGBA8  = 0,   // 32-bit uncompressed (A8R8G8B8 in D3D notation)
    DXT1   = 1,   // BC1 — 4:1 compression, 1-bit alpha
    DXT3   = 3,   // BC2 — 4:1 compression, explicit 4-bit alpha
    DXT5   = 4,   // BC3 — 4:1 compression, interpolated 8-bit alpha
    R5G6B5 = 5,   // 16-bit uncompressed, no alpha
    A4R4G4B4 = 6, // 16-bit uncompressed, 4-bit alpha
    A8R8G8B8 = 7, // Same as RGBA8 but in D3D-native channel order
    Unknown = 0xFFFF
};

// ---------------------------------------------------------------------------
// Texture — a fully-decoded texture asset loaded from a tex_ chunk.
//
// The pixel_data vector always contains RGBA8 data (4 bytes per pixel),
// regardless of the source format. The original format is preserved in
// `format` for informational purposes.
//
// Mipmaps are stored smallest-to-largest, each as a separate byte vector
// of RGBA8 data. mip_levels[0] is the smallest mip.
// ---------------------------------------------------------------------------

struct Texture {
    std::string name;

    uint32_t width  = 0;
    uint32_t height = 0;

    TextureFormat format = TextureFormat::Unknown;

    // RGBA8 pixel data for the base (largest) mip level.
    // Size: width * height * 4 bytes.
    std::vector<uint8_t> pixel_data;

    // Optional mip levels (excluding the base level which is in pixel_data).
    // Index 0 = next-smallest mip after the base, etc.
    std::vector<std::vector<uint8_t>> mipmaps;
};

// ---------------------------------------------------------------------------
// TextureLoader — parses tex_ UCFB chunks into Texture structs.
//
// tex_ chunk layout (observed structure):
//
//   tex_
//     NAME   — null-terminated texture name
//     INFO   — format info: uint32 format_id, uint16 width, uint16 height,
//              uint16 depth, uint16 mip_count, uint32 type_hint
//     FMT_   — format container
//       INFO — uint32 format, uint16 w, uint16 h, uint16 depth, uint16 mips
//       FACE — face container (one per cube-map face; usually just one)
//         LVL_ — mip-level container
//           BODY — raw pixel data for this mip level
//
// The loader handles DXT1 and DXT3 decompression inline (software S3TC
// decoder), as well as raw RGBA and 16-bit formats.
// ---------------------------------------------------------------------------

class TextureLoader {
public:
    TextureLoader() = default;

    // Parse a tex_ chunk and return a decoded Texture.
    // The chunk reader should be positioned at (or represent) a tex_ chunk.
    Texture load(ChunkReader& chunk);

private:
    // Decode DXT1 (BC1) compressed data into RGBA8.
    static std::vector<uint8_t> decode_dxt1(const uint8_t* src,
                                            uint32_t width, uint32_t height);

    // Decode DXT3 (BC2) compressed data into RGBA8.
    static std::vector<uint8_t> decode_dxt3(const uint8_t* src,
                                            uint32_t width, uint32_t height);

    // Decode DXT5 (BC3) compressed data into RGBA8.
    static std::vector<uint8_t> decode_dxt5(const uint8_t* src,
                                            uint32_t width, uint32_t height);

    // Convert R5G6B5 (16-bit) data to RGBA8.
    static std::vector<uint8_t> decode_r5g6b5(const uint8_t* src,
                                              uint32_t width, uint32_t height);

    // Convert A4R4G4B4 (16-bit) data to RGBA8.
    static std::vector<uint8_t> decode_a4r4g4b4(const uint8_t* src,
                                                uint32_t width, uint32_t height);

    // Convert A8R8G8B8 (BGRA byte order) to RGBA8.
    static std::vector<uint8_t> decode_a8r8g8b8(const uint8_t* src,
                                                uint32_t width, uint32_t height);

    // Decode a single mip-level body based on format.
    std::vector<uint8_t> decode_pixels(const uint8_t* src,
                                       uint32_t width, uint32_t height,
                                       TextureFormat fmt,
                                       std::size_t src_size);
};

} // namespace swbf
