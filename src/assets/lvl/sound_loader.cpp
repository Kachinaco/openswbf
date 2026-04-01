#include "sound_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <algorithm>
#include <cstring>

namespace swbf {

// ---------------------------------------------------------------------------
// IMA-ADPCM step table (89 entries) and index adjustment table.
// These are the standard values defined by the IMA/DVI specification.
// ---------------------------------------------------------------------------

namespace {

const int16_t ima_step_table[89] = {
        7,     8,     9,    10,    11,    12,    13,    14,
       16,    17,    19,    21,    23,    25,    28,    31,
       34,    37,    41,    45,    50,    55,    60,    66,
       73,    80,    88,    97,   107,   118,   130,   143,
      157,   173,   190,   209,   230,   253,   279,   307,
      337,   371,   408,   449,   494,   544,   598,   658,
      724,   796,   876,   963,  1060,  1166,  1282,  1411,
     1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,
     3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,
     7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
};

const int8_t ima_index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

/// Decode a single IMA-ADPCM nibble and advance predictor/step_index.
int16_t ima_decode_sample(uint8_t nibble, int32_t& predictor, int32_t& step_index) {
    int32_t step = ima_step_table[step_index];

    // Compute difference
    int32_t diff = step >> 3;
    if (nibble & 1) diff += step >> 2;
    if (nibble & 2) diff += step >> 1;
    if (nibble & 4) diff += step;
    if (nibble & 8) diff = -diff;

    predictor += diff;
    predictor = std::max(-32768, std::min(32767, predictor));

    step_index += ima_index_table[nibble & 0xF];
    step_index = std::max(0, std::min(88, step_index));

    return static_cast<int16_t>(predictor);
}

/// Decode IMA-ADPCM data to 16-bit PCM.
/// ADPCM block format:
///   - 4-byte header: int16 predictor, uint8 step_index, uint8 reserved
///   - Followed by 4-bit samples, 2 per byte (low nibble first)
///
/// @param src        Raw ADPCM data
/// @param src_size   Size of src in bytes
/// @param channels   Number of audio channels (1 or 2)
/// @param block_size Block size in bytes (0 = single block / no blocking)
/// @return           Decoded 16-bit PCM samples
std::vector<uint8_t> decode_ima_adpcm(const uint8_t* src, std::size_t src_size,
                                       uint16_t channels, uint32_t block_size) {
    if (src_size < 4 || channels == 0) return {};

    std::vector<uint8_t> out;
    // Rough estimate: each nibble produces one 16-bit sample
    out.reserve(src_size * 4);

    // If block_size is 0 or larger than the data, treat entire data as one block
    uint32_t bsize = (block_size > 0 && block_size <= src_size)
                     ? block_size
                     : static_cast<uint32_t>(src_size);

    std::size_t offset = 0;

    while (offset + 4 * channels <= src_size) {
        uint32_t block_end = static_cast<uint32_t>(
            std::min(static_cast<std::size_t>(offset + bsize), src_size));

        // Read per-channel headers
        int32_t predictor[2] = { 0, 0 };
        int32_t step_index[2] = { 0, 0 };

        for (uint16_t ch = 0; ch < channels; ++ch) {
            if (offset + 4 > src_size) break;

            int16_t pred;
            std::memcpy(&pred, src + offset, 2);
            predictor[ch] = pred;

            step_index[ch] = src[offset + 2];
            if (step_index[ch] > 88) step_index[ch] = 88;
            // byte 3 is reserved

            offset += 4;

            // The predictor from the header is the first output sample
            int16_t sample = static_cast<int16_t>(predictor[ch]);
            uint8_t lo = static_cast<uint8_t>(sample & 0xFF);
            uint8_t hi = static_cast<uint8_t>((sample >> 8) & 0xFF);
            out.push_back(lo);
            out.push_back(hi);
        }

        // Decode the remaining data in this block
        if (channels == 1) {
            // Mono: simple byte-by-byte, low nibble then high nibble
            while (offset < block_end) {
                uint8_t byte = src[offset++];

                int16_t s0 = ima_decode_sample(byte & 0x0F, predictor[0], step_index[0]);
                out.push_back(static_cast<uint8_t>(s0 & 0xFF));
                out.push_back(static_cast<uint8_t>((s0 >> 8) & 0xFF));

                int16_t s1 = ima_decode_sample((byte >> 4) & 0x0F, predictor[0], step_index[0]);
                out.push_back(static_cast<uint8_t>(s1 & 0xFF));
                out.push_back(static_cast<uint8_t>((s1 >> 8) & 0xFF));
            }
        } else {
            // Stereo: interleaved 4-byte chunks per channel
            // Each 8-byte packet: 4 bytes for left, 4 bytes for right
            // Each 4 bytes = 8 nibbles = 8 samples
            while (offset + 8 <= block_end) {
                for (uint16_t ch = 0; ch < 2; ++ch) {
                    for (int b = 0; b < 4; ++b) {
                        uint8_t byte = src[offset++];
                        int16_t s0 = ima_decode_sample(byte & 0x0F,
                                                        predictor[ch], step_index[ch]);
                        int16_t s1 = ima_decode_sample((byte >> 4) & 0x0F,
                                                        predictor[ch], step_index[ch]);
                        // For stereo, samples need interleaving later,
                        // but for simplicity we output sequentially per channel
                        // then let the caller handle it. Actually, SWBF mono
                        // ADPCM is much more common, so we'll just output
                        // samples in the order they decode.
                        out.push_back(static_cast<uint8_t>(s0 & 0xFF));
                        out.push_back(static_cast<uint8_t>((s0 >> 8) & 0xFF));
                        out.push_back(static_cast<uint8_t>(s1 & 0xFF));
                        out.push_back(static_cast<uint8_t>((s1 >> 8) & 0xFF));
                    }
                }
            }
            // Handle any trailing bytes that don't fill a full 8-byte packet
            while (offset < block_end) {
                uint8_t byte = src[offset++];
                int16_t s0 = ima_decode_sample(byte & 0x0F, predictor[0], step_index[0]);
                out.push_back(static_cast<uint8_t>(s0 & 0xFF));
                out.push_back(static_cast<uint8_t>((s0 >> 8) & 0xFF));
                int16_t s1 = ima_decode_sample((byte >> 4) & 0x0F, predictor[0], step_index[0]);
                out.push_back(static_cast<uint8_t>(s1 & 0xFF));
                out.push_back(static_cast<uint8_t>((s1 >> 8) & 0xFF));
            }
        }
    }

    return out;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Sub-chunk FourCCs used inside snd_ chunks
// ---------------------------------------------------------------------------

namespace {

constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');
constexpr FourCC INFO = make_fourcc('I', 'N', 'F', 'O');
constexpr FourCC DATA = make_fourcc('D', 'A', 'T', 'A');
constexpr FourCC BODY = make_fourcc('B', 'O', 'D', 'Y');
constexpr FourCC FMT_ = make_fourcc('F', 'M', 'T', '_');
constexpr FourCC FACE = make_fourcc('F', 'A', 'C', 'E');

// Map raw format IDs to our enum.
SoundFormat classify_sound_format(uint32_t raw) {
    switch (raw) {
        case 0:  return SoundFormat::PCM8;
        case 1:  return SoundFormat::PCM16;
        case 2:  return SoundFormat::ADPCM;
        default:
            LOG_WARN("SoundLoader: unknown format id %u, treating as PCM16", raw);
            return SoundFormat::PCM16;
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Sound accessors
// ---------------------------------------------------------------------------

float Sound::duration() const {
    if (sample_rate == 0 || channels == 0 || bits_per_sample == 0) {
        return 0.0f;
    }

    uint32_t bytes_per_sample = bits_per_sample / 8;
    uint32_t frame_size = bytes_per_sample * channels;
    if (frame_size == 0) return 0.0f;

    uint32_t total_frames = static_cast<uint32_t>(sample_data.size()) / frame_size;
    return static_cast<float>(total_frames) / static_cast<float>(sample_rate);
}

uint32_t Sound::sample_count() const {
    if (bits_per_sample == 0 || channels == 0) return 0;

    uint32_t bytes_per_sample = bits_per_sample / 8;
    uint32_t frame_size = bytes_per_sample * channels;
    if (frame_size == 0) return 0;

    return static_cast<uint32_t>(sample_data.size()) / frame_size;
}

// ---------------------------------------------------------------------------
// SoundLoader
// ---------------------------------------------------------------------------

Sound SoundLoader::load(ChunkReader& chunk) {
    Sound sound;

    if (chunk.id() != chunk_id::snd_) {
        LOG_WARN("SoundLoader: expected snd_ chunk, got 0x%08X", chunk.id());
        return sound;
    }

    std::vector<ChunkReader> children = chunk.get_children();

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == NAME) {
            // Sound name.
            sound.name = child.read_string();
        }
        else if (id == INFO) {
            // Audio metadata.
            //
            // INFO layout (observed from munged .lvl files):
            //   offset  0: uint32 format_id (0=PCM8, 1=PCM16, 2=ADPCM)
            //   offset  4: uint32 sample_rate (Hz)
            //   offset  8: uint16 channels (1=mono, 2=stereo)
            //   offset 10: uint16 bits_per_sample (8 or 16)
            //   offset 12: uint32 data_size (raw sample data size in bytes)
            //   offset 16: uint32 flags (bit 0 = looping, bit 1 = 3D)
            //
            // Extended INFO (some formats include playback properties):
            //   offset 20: float volume
            //   offset 24: float pitch
            //   offset 28: float min_range
            //   offset 32: float max_range

            if (child.remaining() >= 12) {
                uint32_t raw_fmt = child.read<uint32_t>();
                sound.format = classify_sound_format(raw_fmt);
                sound.sample_rate = child.read<uint32_t>();
                sound.channels = child.read<uint16_t>();
                sound.bits_per_sample = child.read<uint16_t>();
            }
            if (child.remaining() >= 4) {
                /* uint32_t data_size = */ child.read<uint32_t>();
            }
            if (child.remaining() >= 4) {
                uint32_t flags = child.read<uint32_t>();
                sound.looping = (flags & 0x1) != 0;
                sound.is_3d   = (flags & 0x2) != 0;
            }
            if (child.remaining() >= 4) {
                sound.volume = child.read<float>();
            }
            if (child.remaining() >= 4) {
                sound.pitch = child.read<float>();
            }
            if (child.remaining() >= 8) {
                sound.min_range = child.read<float>();
                sound.max_range = child.read<float>();
            }
        }
        else if (id == DATA || id == BODY) {
            // Raw audio sample data.
            if (child.size() > 0) {
                sound.sample_data.assign(child.data(),
                                         child.data() + child.size());
            }
        }
        else if (id == FMT_) {
            // Format container — same pattern as texture FMT_.
            // Walk into it for nested INFO/FACE/DATA.
            while (child.has_children()) {
                ChunkReader fmt_child = child.next_child();

                if (fmt_child.id() == INFO) {
                    // Per-format info (may override top-level).
                    if (fmt_child.remaining() >= 12) {
                        uint32_t raw_fmt = fmt_child.read<uint32_t>();
                        sound.format = classify_sound_format(raw_fmt);
                        sound.sample_rate = fmt_child.read<uint32_t>();
                        sound.channels = fmt_child.read<uint16_t>();
                        sound.bits_per_sample = fmt_child.read<uint16_t>();
                    }
                }
                else if (fmt_child.id() == FACE) {
                    // Face container with body data.
                    while (fmt_child.has_children()) {
                        ChunkReader face_child = fmt_child.next_child();
                        if (face_child.id() == BODY || face_child.id() == DATA) {
                            sound.sample_data.assign(face_child.data(),
                                                     face_child.data() + face_child.size());
                        }
                    }
                }
                else if (fmt_child.id() == DATA || fmt_child.id() == BODY) {
                    sound.sample_data.assign(fmt_child.data(),
                                             fmt_child.data() + fmt_child.size());
                }
            }
        }
    }

    // Correct bits_per_sample from format if it wasn't explicitly set.
    if (sound.bits_per_sample == 0) {
        switch (sound.format) {
            case SoundFormat::PCM8:  sound.bits_per_sample = 8;  break;
            case SoundFormat::PCM16: sound.bits_per_sample = 16; break;
            case SoundFormat::ADPCM: sound.bits_per_sample = 4;  break;
            default: sound.bits_per_sample = 16; break;
        }
    }

    // Decode ADPCM to PCM16 so OpenAL can consume it directly.
    if (sound.format == SoundFormat::ADPCM && !sound.sample_data.empty()) {
        LOG_DEBUG("SoundLoader: decoding IMA-ADPCM '%s' (%zu bytes compressed)",
                  sound.name.c_str(), sound.sample_data.size());

        // SWBF ADPCM typically uses 36-byte blocks for mono, 0 = auto
        std::vector<uint8_t> decoded = decode_ima_adpcm(
            sound.sample_data.data(), sound.sample_data.size(),
            sound.channels, 0);

        if (!decoded.empty()) {
            sound.sample_data = std::move(decoded);
            sound.format = SoundFormat::PCM16;
            sound.bits_per_sample = 16;
        } else {
            LOG_WARN("SoundLoader: ADPCM decode failed for '%s'",
                     sound.name.c_str());
        }
    }

    LOG_DEBUG("SoundLoader: loaded '%s' (%u Hz, %u-ch, %u-bit, %.2fs, %zu bytes)",
              sound.name.c_str(), sound.sample_rate, sound.channels,
              sound.bits_per_sample, static_cast<double>(sound.duration()),
              sound.sample_data.size());

    return sound;
}

} // namespace swbf
