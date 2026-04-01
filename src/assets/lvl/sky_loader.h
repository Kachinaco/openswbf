#pragma once

#include "core/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// Sky / sky dome data structures
//
// The sky_ chunk in munged .lvl files describes the skybox or sky dome:
//   - Dome geometry or texture names for cubemap faces
//   - Ambient / fog colors
//   - Sun/moon direction and colors
//
// The sky_ chunk hierarchy:
//
//   sky_
//     INFO  — sky configuration flags
//     DOME  — dome geometry or texture references
//     DATA  — sky parameters (colors, directions, fog settings)
// ---------------------------------------------------------------------------

/// Parsed sky/dome configuration.
struct SkyData {
    std::string name;

    /// Sky dome texture name (if using a textured dome).
    std::string dome_texture;

    /// Ambient sky color (RGB).
    float ambient_color[3] = {0.3f, 0.35f, 0.5f};

    /// Top/zenith sky color (RGB).
    float top_color[3] = {0.2f, 0.35f, 0.75f};

    /// Horizon sky color (RGB).
    float horizon_color[3] = {0.6f, 0.7f, 0.9f};

    /// Fog color (RGB) and range.
    float fog_color[3] = {0.6f, 0.7f, 0.9f};
    float fog_near     = 100.0f;
    float fog_far      = 1000.0f;

    /// Sun direction (normalized).
    float sun_direction[3] = {0.4f, 0.8f, 0.3f};

    /// Sun color (RGB).
    float sun_color[3] = {1.0f, 0.95f, 0.8f};
};

// ---------------------------------------------------------------------------
// SkyLoader — parses sky_ UCFB chunks from munged .lvl files.
//
// Usage:
//   SkyLoader loader;
//   SkyData sky = loader.load(sky_chunk);
// ---------------------------------------------------------------------------

class SkyLoader {
public:
    SkyLoader() = default;

    /// Parse a sky_ chunk and return decoded sky configuration.
    SkyData load(ChunkReader& chunk);
};

} // namespace swbf
