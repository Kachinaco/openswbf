#pragma once

#include "core/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// Font / glyph data structures
//
// The font chunk in munged .lvl files defines bitmap fonts:
//   - Glyph atlas texture
//   - Per-glyph metrics (UV rect, advance, bearing)
//   - Font metadata (line height, baseline)
//
// The font chunk hierarchy:
//
//   font
//     NAME  — font name
//     INFO  — font metadata (line height, glyph count)
//     DATA  — glyph metrics table
//     tex_  — glyph atlas texture
// ---------------------------------------------------------------------------

/// Metrics for a single glyph.
struct GlyphMetrics {
    uint32_t codepoint = 0;       // Unicode codepoint

    float uv_min[2] = {0.0f, 0.0f}; // Top-left UV in atlas
    float uv_max[2] = {0.0f, 0.0f}; // Bottom-right UV in atlas

    float advance   = 0.0f;       // Horizontal advance
    float bearing[2] = {0.0f, 0.0f}; // X/Y bearing offset
    float size[2]   = {0.0f, 0.0f};  // Glyph pixel size
};

/// A parsed bitmap font.
struct FontData {
    std::string name;

    float line_height = 16.0f;    // Height of a line of text
    float baseline    = 12.0f;    // Baseline offset from top

    std::vector<GlyphMetrics> glyphs;

    /// Raw glyph atlas texture data (may reference a tex_ sub-chunk).
    std::string atlas_texture_name;

    /// Raw data blob for formats not yet decoded.
    std::vector<uint8_t> raw_data;
};

// ---------------------------------------------------------------------------
// FontLoader — parses font UCFB chunks from munged .lvl files.
//
// Usage:
//   FontLoader loader;
//   FontData font = loader.load(font_chunk);
// ---------------------------------------------------------------------------

class FontLoader {
public:
    FontLoader() = default;

    /// Parse a font chunk and return decoded font data.
    FontData load(ChunkReader& chunk);
};

} // namespace swbf
