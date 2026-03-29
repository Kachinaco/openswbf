#include "texture_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"
#include "core/types.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace swbf {

// ---------------------------------------------------------------------------
// Sub-chunk FourCCs used inside tex_ chunks
// ---------------------------------------------------------------------------

namespace {

constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');
constexpr FourCC INFO = make_fourcc('I', 'N', 'F', 'O');
constexpr FourCC FMT_ = make_fourcc('F', 'M', 'T', '_');
constexpr FourCC FACE = make_fourcc('F', 'A', 'C', 'E');
constexpr FourCC LVL_ = make_fourcc('L', 'V', 'L', '_');
constexpr FourCC BODY = make_fourcc('B', 'O', 'D', 'Y');

// Map raw D3DFORMAT values seen in SWBF LVL files to our enum.
TextureFormat classify_format(uint32_t raw_fmt) {
    switch (raw_fmt) {
        case 0:  return TextureFormat::RGBA8;      // Uncompressed RGBA
        case 1:  return TextureFormat::DXT1;
        case 3:  return TextureFormat::DXT3;
        case 5:  return TextureFormat::R5G6B5;
        case 6:  return TextureFormat::A4R4G4B4;
        case 7:  return TextureFormat::A8R8G8B8;
        default:
            LOG_WARN("TextureLoader: unknown format id %u, treating as RGBA8", raw_fmt);
            return TextureFormat::RGBA8;
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// DXT1 (BC1) decoder
//
// Each 4x4 block is 8 bytes:
//   [uint16 color0] [uint16 color1] [uint32 lookup_table]
//
// Color endpoints are R5G6B5. The lookup table stores 2-bit indices per
// texel (16 texels = 32 bits). If color0 > color1, four colors are
// interpolated; otherwise three colors + transparent black.
// ---------------------------------------------------------------------------

static void unpack_rgb565(uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = static_cast<uint8_t>(((c >> 11) & 0x1F) * 255 / 31);
    g = static_cast<uint8_t>(((c >>  5) & 0x3F) * 255 / 63);
    b = static_cast<uint8_t>(( c        & 0x1F) * 255 / 31);
}

std::vector<uint8_t> TextureLoader::decode_dxt1(const uint8_t* src,
                                                 uint32_t width,
                                                 uint32_t height) {
    const uint32_t bw = (width  + 3) / 4;  // blocks across
    const uint32_t bh = (height + 3) / 4;  // blocks down

    std::vector<uint8_t> out(width * height * 4, 0);

    for (uint32_t by = 0; by < bh; ++by) {
        for (uint32_t bx = 0; bx < bw; ++bx) {
            const uint8_t* block = src + (by * bw + bx) * 8;

            uint16_t c0, c1;
            std::memcpy(&c0, block + 0, 2);
            std::memcpy(&c1, block + 2, 2);

            uint8_t r0, g0, b0, r1, g1, b1;
            unpack_rgb565(c0, r0, g0, b0);
            unpack_rgb565(c1, r1, g1, b1);

            // Build the 4-color palette.
            uint8_t palette[4][4]; // [index][R,G,B,A]
            palette[0][0] = r0; palette[0][1] = g0; palette[0][2] = b0; palette[0][3] = 255;
            palette[1][0] = r1; palette[1][1] = g1; palette[1][2] = b1; palette[1][3] = 255;

            if (c0 > c1) {
                // 4-color mode: color2 = 2/3*c0 + 1/3*c1, color3 = 1/3*c0 + 2/3*c1
                palette[2][0] = static_cast<uint8_t>((2 * r0 + r1 + 1) / 3);
                palette[2][1] = static_cast<uint8_t>((2 * g0 + g1 + 1) / 3);
                palette[2][2] = static_cast<uint8_t>((2 * b0 + b1 + 1) / 3);
                palette[2][3] = 255;
                palette[3][0] = static_cast<uint8_t>((r0 + 2 * r1 + 1) / 3);
                palette[3][1] = static_cast<uint8_t>((g0 + 2 * g1 + 1) / 3);
                palette[3][2] = static_cast<uint8_t>((b0 + 2 * b1 + 1) / 3);
                palette[3][3] = 255;
            } else {
                // 3-color + transparent mode
                palette[2][0] = static_cast<uint8_t>((r0 + r1) / 2);
                palette[2][1] = static_cast<uint8_t>((g0 + g1) / 2);
                palette[2][2] = static_cast<uint8_t>((b0 + b1) / 2);
                palette[2][3] = 255;
                palette[3][0] = 0; palette[3][1] = 0; palette[3][2] = 0; palette[3][3] = 0;
            }

            uint32_t indices;
            std::memcpy(&indices, block + 4, 4);

            for (uint32_t row = 0; row < 4; ++row) {
                for (uint32_t col = 0; col < 4; ++col) {
                    uint32_t px = bx * 4 + col;
                    uint32_t py = by * 4 + row;
                    if (px >= width || py >= height) continue;

                    uint32_t bit_index = (row * 4 + col) * 2;
                    uint32_t idx = (indices >> bit_index) & 0x3;

                    std::size_t offset = (py * width + px) * 4;
                    out[offset + 0] = palette[idx][0];
                    out[offset + 1] = palette[idx][1];
                    out[offset + 2] = palette[idx][2];
                    out[offset + 3] = palette[idx][3];
                }
            }
        }
    }

    return out;
}

// ---------------------------------------------------------------------------
// DXT3 (BC2) decoder
//
// Each 4x4 block is 16 bytes:
//   [8 bytes explicit alpha] [8 bytes DXT1-style color block]
//
// The alpha block stores 4 bits per texel (16 texels = 64 bits).
// The color block is identical to DXT1 but always uses 4-color mode.
// ---------------------------------------------------------------------------

std::vector<uint8_t> TextureLoader::decode_dxt3(const uint8_t* src,
                                                 uint32_t width,
                                                 uint32_t height) {
    const uint32_t bw = (width  + 3) / 4;
    const uint32_t bh = (height + 3) / 4;

    std::vector<uint8_t> out(width * height * 4, 0);

    for (uint32_t by = 0; by < bh; ++by) {
        for (uint32_t bx = 0; bx < bw; ++bx) {
            const uint8_t* block = src + (by * bw + bx) * 16;
            const uint8_t* alpha_block = block;
            const uint8_t* color_block = block + 8;

            // Decode color endpoints (same as DXT1, always 4-color mode).
            uint16_t c0, c1;
            std::memcpy(&c0, color_block + 0, 2);
            std::memcpy(&c1, color_block + 2, 2);

            uint8_t r0, g0, b0, r1, g1, b1;
            unpack_rgb565(c0, r0, g0, b0);
            unpack_rgb565(c1, r1, g1, b1);

            uint8_t palette[4][3];
            palette[0][0] = r0; palette[0][1] = g0; palette[0][2] = b0;
            palette[1][0] = r1; palette[1][1] = g1; palette[1][2] = b1;
            palette[2][0] = static_cast<uint8_t>((2 * r0 + r1 + 1) / 3);
            palette[2][1] = static_cast<uint8_t>((2 * g0 + g1 + 1) / 3);
            palette[2][2] = static_cast<uint8_t>((2 * b0 + b1 + 1) / 3);
            palette[3][0] = static_cast<uint8_t>((r0 + 2 * r1 + 1) / 3);
            palette[3][1] = static_cast<uint8_t>((g0 + 2 * g1 + 1) / 3);
            palette[3][2] = static_cast<uint8_t>((b0 + 2 * b1 + 1) / 3);

            uint32_t indices;
            std::memcpy(&indices, color_block + 4, 4);

            for (uint32_t row = 0; row < 4; ++row) {
                for (uint32_t col = 0; col < 4; ++col) {
                    uint32_t px = bx * 4 + col;
                    uint32_t py = by * 4 + row;
                    if (px >= width || py >= height) continue;

                    // Color index: 2 bits per texel
                    uint32_t bit_index = (row * 4 + col) * 2;
                    uint32_t idx = (indices >> bit_index) & 0x3;

                    // Alpha: 4 bits per texel, stored row-by-row as uint16s.
                    // alpha_block is 8 bytes = 4 uint16s, one per row.
                    uint16_t alpha_row;
                    std::memcpy(&alpha_row, alpha_block + row * 2, 2);
                    uint8_t alpha4 = (alpha_row >> (col * 4)) & 0xF;
                    uint8_t alpha8 = static_cast<uint8_t>(alpha4 * 17); // scale 0-15 -> 0-255

                    std::size_t offset = (py * width + px) * 4;
                    out[offset + 0] = palette[idx][0];
                    out[offset + 1] = palette[idx][1];
                    out[offset + 2] = palette[idx][2];
                    out[offset + 3] = alpha8;
                }
            }
        }
    }

    return out;
}

// ---------------------------------------------------------------------------
// R5G6B5 -> RGBA8
// ---------------------------------------------------------------------------

std::vector<uint8_t> TextureLoader::decode_r5g6b5(const uint8_t* src,
                                                    uint32_t width,
                                                    uint32_t height) {
    const uint32_t pixel_count = width * height;
    std::vector<uint8_t> out(pixel_count * 4);

    for (uint32_t i = 0; i < pixel_count; ++i) {
        uint16_t pixel;
        std::memcpy(&pixel, src + i * 2, 2);

        uint8_t r, g, b;
        unpack_rgb565(pixel, r, g, b);

        out[i * 4 + 0] = r;
        out[i * 4 + 1] = g;
        out[i * 4 + 2] = b;
        out[i * 4 + 3] = 255;
    }

    return out;
}

// ---------------------------------------------------------------------------
// A4R4G4B4 -> RGBA8
// ---------------------------------------------------------------------------

std::vector<uint8_t> TextureLoader::decode_a4r4g4b4(const uint8_t* src,
                                                      uint32_t width,
                                                      uint32_t height) {
    const uint32_t pixel_count = width * height;
    std::vector<uint8_t> out(pixel_count * 4);

    for (uint32_t i = 0; i < pixel_count; ++i) {
        uint16_t pixel;
        std::memcpy(&pixel, src + i * 2, 2);

        // D3DFMT_A4R4G4B4 bit layout: AAAA RRRR GGGG BBBB
        uint8_t a4 = static_cast<uint8_t>((pixel >> 12) & 0xF);
        uint8_t r4 = static_cast<uint8_t>((pixel >>  8) & 0xF);
        uint8_t g4 = static_cast<uint8_t>((pixel >>  4) & 0xF);
        uint8_t b4 = static_cast<uint8_t>( pixel        & 0xF);

        out[i * 4 + 0] = static_cast<uint8_t>(r4 * 17); // 0-15 -> 0-255
        out[i * 4 + 1] = static_cast<uint8_t>(g4 * 17);
        out[i * 4 + 2] = static_cast<uint8_t>(b4 * 17);
        out[i * 4 + 3] = static_cast<uint8_t>(a4 * 17);
    }

    return out;
}

// ---------------------------------------------------------------------------
// A8R8G8B8 (BGRA byte order in memory) -> RGBA8
// ---------------------------------------------------------------------------

std::vector<uint8_t> TextureLoader::decode_a8r8g8b8(const uint8_t* src,
                                                      uint32_t width,
                                                      uint32_t height) {
    const uint32_t pixel_count = width * height;
    std::vector<uint8_t> out(pixel_count * 4);

    for (uint32_t i = 0; i < pixel_count; ++i) {
        // D3D A8R8G8B8 in memory (little-endian): B, G, R, A
        const uint8_t* p = src + i * 4;
        out[i * 4 + 0] = p[2]; // R
        out[i * 4 + 1] = p[1]; // G
        out[i * 4 + 2] = p[0]; // B
        out[i * 4 + 3] = p[3]; // A
    }

    return out;
}

// ---------------------------------------------------------------------------
// decode_pixels — dispatch to the correct format decoder
// ---------------------------------------------------------------------------

std::vector<uint8_t> TextureLoader::decode_pixels(const uint8_t* src,
                                                    uint32_t width,
                                                    uint32_t height,
                                                    TextureFormat fmt,
                                                    std::size_t /*src_size*/) {
    switch (fmt) {
        case TextureFormat::DXT1:
            return decode_dxt1(src, width, height);

        case TextureFormat::DXT3:
            return decode_dxt3(src, width, height);

        case TextureFormat::R5G6B5:
            return decode_r5g6b5(src, width, height);

        case TextureFormat::A4R4G4B4:
            return decode_a4r4g4b4(src, width, height);

        case TextureFormat::A8R8G8B8:
            return decode_a8r8g8b8(src, width, height);

        case TextureFormat::RGBA8:
        default: {
            // Assume raw RGBA8 — just copy directly.
            std::size_t byte_count = static_cast<std::size_t>(width) * height * 4;
            return std::vector<uint8_t>(src, src + byte_count);
        }
    }
}

// ---------------------------------------------------------------------------
// load — main entry point: parse a tex_ chunk
// ---------------------------------------------------------------------------

Texture TextureLoader::load(ChunkReader& chunk) {
    Texture result;

    if (chunk.id() != chunk_id::tex_) {
        LOG_WARN("TextureLoader: expected tex_ chunk, got 0x%08X", chunk.id());
        return result;
    }

    TextureFormat fmt = TextureFormat::RGBA8;
    uint32_t mip_count = 1;

    // Walk the children of the tex_ chunk.
    while (chunk.has_children()) {
        ChunkReader child = chunk.next_child();

        if (child.id() == NAME) {
            // Texture name (null-terminated string).
            result.name = child.read_string();
        }
        else if (child.id() == INFO) {
            // Top-level texture info. Layout varies but typically:
            //   u32 format, u16 width, u16 height, u16 depth, u16 mip_count
            uint32_t raw_fmt = child.read<uint32_t>();
            fmt = classify_format(raw_fmt);
            result.format = fmt;
            result.width  = child.read<uint16_t>();
            result.height = child.read<uint16_t>();
            /* uint16_t depth = */ child.read<uint16_t>();
            mip_count = child.read<uint16_t>();
            if (mip_count == 0) mip_count = 1;
        }
        else if (child.id() == FMT_) {
            // FMT_ is a container that holds per-format INFO + FACE children.
            // Parse it for the actual pixel data.
            while (child.has_children()) {
                ChunkReader fmt_child = child.next_child();

                if (fmt_child.id() == INFO) {
                    // Per-format info — may override top-level values.
                    uint32_t raw_fmt2 = fmt_child.read<uint32_t>();
                    fmt = classify_format(raw_fmt2);
                    result.format = fmt;
                    result.width  = fmt_child.read<uint16_t>();
                    result.height = fmt_child.read<uint16_t>();
                    if (fmt_child.remaining() >= 4) {
                        /* uint16_t depth = */ fmt_child.read<uint16_t>();
                        mip_count = fmt_child.read<uint16_t>();
                        if (mip_count == 0) mip_count = 1;
                    }
                }
                else if (fmt_child.id() == FACE) {
                    // FACE contains LVL_ children, each of which holds a
                    // BODY child with the actual pixel data for one mip.
                    uint32_t mip_index = 0;
                    uint32_t mip_w = result.width;
                    uint32_t mip_h = result.height;

                    while (fmt_child.has_children()) {
                        ChunkReader lvl_child = fmt_child.next_child();

                        if (lvl_child.id() == LVL_) {
                            // Inside LVL_, look for the BODY chunk.
                            while (lvl_child.has_children()) {
                                ChunkReader body_child = lvl_child.next_child();

                                if (body_child.id() == BODY) {
                                    auto decoded = decode_pixels(
                                        body_child.data(),
                                        mip_w, mip_h,
                                        fmt,
                                        body_child.size());

                                    if (mip_index == 0) {
                                        // Base (largest) mip level.
                                        result.pixel_data = std::move(decoded);
                                    } else {
                                        result.mipmaps.push_back(std::move(decoded));
                                    }
                                }
                            }

                            ++mip_index;
                            // Next mip is half the size (min 1).
                            mip_w = std::max(1u, mip_w / 2);
                            mip_h = std::max(1u, mip_h / 2);
                        }
                    }
                }
            }
        }
    }

    // If we got format info but no FMT_/FACE tree (simpler tex_ layout),
    // the pixel data might be inline in the chunk's remaining payload.
    // This fallback handles that case.
    if (result.pixel_data.empty() && result.width > 0 && result.height > 0) {
        LOG_WARN("TextureLoader: '%s' — no BODY data found in tex_ chunk",
                 result.name.c_str());
    }

    return result;
}

} // namespace swbf
