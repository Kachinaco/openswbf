#include "sound_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <cstring>

namespace swbf {

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

    LOG_DEBUG("SoundLoader: loaded '%s' (%u Hz, %u-ch, %u-bit, %.2fs, %zu bytes)",
              sound.name.c_str(), sound.sample_rate, sound.channels,
              sound.bits_per_sample, static_cast<double>(sound.duration()),
              sound.sample_data.size());

    return sound;
}

} // namespace swbf
