#include "music_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <cstring>

namespace swbf {

// ---------------------------------------------------------------------------
// MusicTrack::duration
// ---------------------------------------------------------------------------

float MusicTrack::duration() const {
    if (stream_data.empty() || sample_rate == 0 || channels == 0) return 0.0f;
    uint32_t bytes_per_sample = bits_per_sample / 8;
    if (bytes_per_sample == 0) return 0.0f;
    uint32_t total_samples = static_cast<uint32_t>(stream_data.size()) /
                             (bytes_per_sample * channels);
    return static_cast<float>(total_samples) / static_cast<float>(sample_rate);
}

namespace {

constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');
constexpr FourCC INFO = make_fourcc('I', 'N', 'F', 'O');
constexpr FourCC DATA = make_fourcc('D', 'A', 'T', 'A');

} // anonymous namespace

MusicTrack MusicLoader::load(ChunkReader& chunk) {
    MusicTrack track;

    // Handle possible double-wrapping.
    std::vector<ChunkReader> top_children = chunk.get_children();

    std::vector<ChunkReader> children;
    if (!top_children.empty() && top_children[0].id() == chunk_id::mus_) {
        children = top_children[0].get_children();
    } else {
        children = std::move(top_children);
    }

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == NAME) {
            track.name = child.read_string();
        }
        else if (id == INFO) {
            // Parse format metadata if enough data.
            if (child.remaining() >= 8) {
                track.sample_rate = child.read<uint32_t>();
                uint32_t format_flags = child.read<uint32_t>();
                track.channels = static_cast<uint16_t>((format_flags & 0xFF));
                if (track.channels == 0) track.channels = 2;
                track.bits_per_sample = static_cast<uint16_t>((format_flags >> 8) & 0xFF);
                if (track.bits_per_sample == 0) track.bits_per_sample = 16;
            }
        }
        else if (id == DATA) {
            if (child.size() > 0) {
                track.stream_data.assign(child.data(), child.data() + child.size());
            }
        }
    }

    LOG_DEBUG("MusicLoader: loaded track '%s' (%u Hz, %u ch, %.1fs)",
              track.name.c_str(), track.sample_rate, track.channels,
              static_cast<double>(track.duration()));

    return track;
}

} // namespace swbf
