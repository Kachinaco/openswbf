#pragma once

#include "core/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// Visual effects data structures
//
// The fx__ chunk in munged .lvl files defines visual effects:
//   - Particle emitter configurations
//   - Texture references for particle sprites
//   - Effect parameters (lifetime, velocity, color curves)
//
// The fx__ chunk hierarchy:
//
//   fx__
//     NAME  — effect name
//     INFO  — effect metadata
//     DATA  — effect parameters (emitter configs, particle properties)
// ---------------------------------------------------------------------------

/// A parsed visual effect definition.
struct EffectData {
    std::string name;

    /// Effect type name (e.g. "explosion", "smoke", "spark").
    std::string type;

    /// Texture name used by the effect's particle sprites.
    std::string texture;

    /// Raw parameter data — stored as bytes until we fully decode the format.
    std::vector<uint8_t> raw_data;
};

// ---------------------------------------------------------------------------
// EffectsLoader — parses fx__ UCFB chunks from munged .lvl files.
//
// Usage:
//   EffectsLoader loader;
//   EffectData fx = loader.load(fx_chunk);
// ---------------------------------------------------------------------------

class EffectsLoader {
public:
    EffectsLoader() = default;

    /// Parse an fx__ chunk and return decoded effect definition.
    EffectData load(ChunkReader& chunk);
};

} // namespace swbf
