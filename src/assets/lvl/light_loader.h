#pragma once

#include "core/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// Light data structures
//
// SWBF supports three types of lights in its world files:
//   - Directional (sun/moon) — infinite parallel rays
//   - Point — omnidirectional, with range falloff
//   - Spot — cone-shaped, with inner/outer angles and range
//
// Lights in munged .lvl files appear as lght chunks containing:
//
//   lght
//     INFO  — light count
//     DATA (repeated) — per-light data
//       or a single DATA block with all lights packed sequentially
//
// Each light record stores:
//   - Name (for editor/script reference)
//   - Type (directional, point, spot)
//   - Color (RGB float)
//   - Position and rotation (for point/spot lights)
//   - Range (for point/spot lights)
//   - Cone angles (for spot lights)
// ---------------------------------------------------------------------------

/// Light type enumeration matching SWBF's internal type codes.
enum class LightType : uint32_t {
    Directional = 0,   // Infinite parallel light (sun/moon)
    Point       = 1,   // Omnidirectional point light
    Spot        = 2,   // Cone-shaped spotlight
    Unknown     = 0xFF
};

/// A fully parsed light definition.
struct Light {
    std::string name;
    LightType   type = LightType::Point;

    float color[4]     = {1.0f, 1.0f, 1.0f, 1.0f};  // RGBA color
    float position[3]  = {0.0f, 0.0f, 0.0f};          // World position
    float rotation[4]  = {0.0f, 0.0f, 0.0f, 1.0f};    // Quaternion (x,y,z,w)
    float range        = 10.0f;                         // Attenuation range

    // Spotlight-specific parameters
    float inner_cone_angle = 30.0f;   // Inner cone half-angle (degrees)
    float outer_cone_angle = 45.0f;   // Outer cone half-angle (degrees)

    // Bidirectional flag — some lights emit in both directions
    bool  bidirectional = false;

    // Intensity/brightness multiplier
    float intensity     = 1.0f;

    // Is this a static (baked) light or a dynamic runtime light?
    bool  is_static     = true;
};

// ---------------------------------------------------------------------------
// LightLoader — parses lght UCFB chunks from munged .lvl files.
//
// Munged light chunk structure:
//
//   lght
//     INFO (4 bytes) — uint32 light count
//     NAME           — light name (null-terminated)
//     DATA           — light parameters (type, color, pos, rot, range, etc.)
//
// Some .lvl files pack all lights inside a single lght container with
// repeated sub-chunks for each light.
//
// Usage:
//   LightLoader loader;
//   auto lights = loader.load(lght_chunk);
// ---------------------------------------------------------------------------

class LightLoader {
public:
    LightLoader() = default;

    /// Parse a lght chunk and return all decoded lights.
    std::vector<Light> load(ChunkReader& chunk);

private:
    /// Parse a single light from a DATA sub-chunk.
    Light parse_light_data(ChunkReader& data_chunk);
};

} // namespace swbf
