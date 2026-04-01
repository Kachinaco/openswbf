#pragma once

#include "core/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// HUD layout data structures
//
// The hud_ chunk in munged .lvl files defines HUD elements:
//   - Element positions and sizes (screen-space coordinates)
//   - Element types (health bar, ammo counter, minimap, reticle)
//   - Texture references for HUD sprites
//
// The hud_ chunk hierarchy:
//
//   hud_
//     NAME  — HUD layout name
//     INFO  — layout metadata
//     DATA  — element definitions
// ---------------------------------------------------------------------------

/// A single HUD element definition.
struct HudElement {
    std::string name;
    std::string type;      // Element type string
    std::string texture;   // Texture name for this element

    float position[2] = {0.0f, 0.0f};  // Screen-space position
    float size[2]     = {1.0f, 1.0f};   // Element size
};

/// A parsed HUD layout.
struct HudLayout {
    std::string name;
    std::vector<HudElement> elements;

    /// Raw data blob — stored until full format is decoded.
    std::vector<uint8_t> raw_data;
};

// ---------------------------------------------------------------------------
// HudLoader — parses hud_ UCFB chunks from munged .lvl files.
//
// Usage:
//   HudLoader loader;
//   HudLayout hud = loader.load(hud_chunk);
// ---------------------------------------------------------------------------

class HudLoader {
public:
    HudLoader() = default;

    /// Parse a hud_ chunk and return decoded HUD layout.
    HudLayout load(ChunkReader& chunk);
};

} // namespace swbf
