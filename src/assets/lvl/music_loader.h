#pragma once

#include "core/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// Music / streaming audio data structures
//
// The mus_ chunk in munged .lvl files defines music tracks:
//   - Track name and metadata
//   - Audio stream data (typically lower quality than sound effects)
//   - Playback configuration (looping, crossfade, volume)
//
// The mus_ chunk hierarchy:
//
//   mus_
//     NAME  — music track name
//     INFO  — format metadata (sample rate, channels, etc.)
//     DATA  — audio stream data
// ---------------------------------------------------------------------------

/// A parsed music track asset.
struct MusicTrack {
    std::string name;

    uint32_t sample_rate     = 22050;
    uint16_t channels        = 2;       // Stereo typical for music
    uint16_t bits_per_sample = 16;

    float    volume          = 1.0f;
    bool     looping         = true;

    /// Raw audio stream data.
    std::vector<uint8_t> stream_data;

    /// Duration in seconds (computed from stream data).
    float duration() const;
};

// ---------------------------------------------------------------------------
// MusicLoader — parses mus_ UCFB chunks from munged .lvl files.
//
// Usage:
//   MusicLoader loader;
//   MusicTrack track = loader.load(mus_chunk);
// ---------------------------------------------------------------------------

class MusicLoader {
public:
    MusicLoader() = default;

    /// Parse a mus_ chunk and return decoded music track.
    MusicTrack load(ChunkReader& chunk);
};

} // namespace swbf
