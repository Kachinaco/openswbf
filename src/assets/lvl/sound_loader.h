#pragma once

#include "core/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// Sound / audio asset data structures
//
// SWBF stores munged audio data in snd_ chunks within .lvl files.
// Sound assets include:
//   - PCM sample data (raw audio waveforms)
//   - Metadata (sample rate, channels, bits per sample)
//   - Playback properties (volume, pitch, looping, 3D spatialization)
//
// The snd_ chunk hierarchy in munged .lvl files:
//
//   snd_
//     NAME  — sound asset name
//     INFO  — format metadata (sample rate, channels, bps, flags)
//     DATA  — raw PCM audio data
//     FACE  — optional: additional audio faces (for multi-variant sounds)
//
// Audio data is typically 16-bit signed PCM, mono or stereo, at
// 22050 Hz or 44100 Hz sample rates.
// ---------------------------------------------------------------------------

/// Audio sample format.
enum class SoundFormat : uint32_t {
    PCM8     = 0,   // 8-bit unsigned PCM
    PCM16    = 1,   // 16-bit signed PCM (most common)
    ADPCM    = 2,   // IMA ADPCM compressed (Xbox)
    Unknown  = 0xFF
};

/// A fully parsed sound asset.
struct Sound {
    std::string  name;              // Sound name/identifier

    uint32_t     sample_rate = 22050;   // Samples per second (Hz)
    uint16_t     channels    = 1;       // 1 = mono, 2 = stereo
    uint16_t     bits_per_sample = 16;  // 8 or 16

    SoundFormat  format = SoundFormat::PCM16;

    /// Raw PCM sample data.
    /// For PCM16: interleaved int16_t samples.
    /// For PCM8: unsigned uint8_t samples.
    std::vector<uint8_t> sample_data;

    // Playback properties parsed from the chunk
    float        volume    = 1.0f;   // 0.0 to 1.0
    float        pitch     = 1.0f;   // Pitch multiplier (1.0 = normal)
    float        min_range = 1.0f;   // 3D: minimum audible distance
    float        max_range = 100.0f; // 3D: maximum audible distance
    bool         looping   = false;  // Whether the sound loops
    bool         is_3d     = false;  // Whether the sound is spatialized

    /// Duration in seconds (computed from sample data).
    float duration() const;

    /// Total number of samples (per channel).
    uint32_t sample_count() const;
};

// ---------------------------------------------------------------------------
// SoundLoader — parses snd_ UCFB chunks from munged .lvl files.
//
// Usage:
//   SoundLoader loader;
//   Sound snd = loader.load(snd_chunk);
// ---------------------------------------------------------------------------

class SoundLoader {
public:
    SoundLoader() = default;

    /// Parse a snd_ chunk and return the decoded sound asset.
    Sound load(ChunkReader& chunk);
};

} // namespace swbf
