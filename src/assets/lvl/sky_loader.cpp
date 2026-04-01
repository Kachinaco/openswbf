#include "sky_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <cstring>

namespace swbf {

namespace {

constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');
constexpr FourCC INFO = make_fourcc('I', 'N', 'F', 'O');
constexpr FourCC DATA = make_fourcc('D', 'A', 'T', 'A');
constexpr FourCC DOME = make_fourcc('D', 'O', 'M', 'E');
constexpr FourCC TNAM = make_fourcc('T', 'N', 'A', 'M');
constexpr FourCC PROP = make_fourcc('P', 'R', 'O', 'P');

} // anonymous namespace

SkyData SkyLoader::load(ChunkReader& chunk) {
    SkyData sky;

    // Handle possible double-wrapping.
    std::vector<ChunkReader> top_children = chunk.get_children();

    std::vector<ChunkReader> children;
    if (!top_children.empty() && top_children[0].id() == chunk_id::sky_) {
        children = top_children[0].get_children();
    } else {
        children = std::move(top_children);
    }

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == NAME) {
            sky.name = child.read_string();
        }
        else if (id == TNAM || id == DOME) {
            // Texture name for the dome.
            if (child.remaining() > 0) {
                try {
                    sky.dome_texture = child.read_string();
                } catch (...) {
                    // Not a string — skip.
                }
            }
        }
        else if (id == DATA) {
            // DATA may contain packed sky parameters.
            // Try to read colors if enough data is available.
            if (child.remaining() >= 36) {
                // Ambient color
                sky.ambient_color[0] = child.read<float>();
                sky.ambient_color[1] = child.read<float>();
                sky.ambient_color[2] = child.read<float>();
                // Top color
                sky.top_color[0] = child.read<float>();
                sky.top_color[1] = child.read<float>();
                sky.top_color[2] = child.read<float>();
                // Horizon color
                sky.horizon_color[0] = child.read<float>();
                sky.horizon_color[1] = child.read<float>();
                sky.horizon_color[2] = child.read<float>();
            }
            if (child.remaining() >= 12) {
                // Fog color
                sky.fog_color[0] = child.read<float>();
                sky.fog_color[1] = child.read<float>();
                sky.fog_color[2] = child.read<float>();
            }
            if (child.remaining() >= 8) {
                sky.fog_near = child.read<float>();
                sky.fog_far  = child.read<float>();
            }
        }
        else if (id == INFO) {
            // Sky configuration flags — skip for now.
        }
    }

    LOG_INFO("SkyLoader: loaded sky '%s' (dome='%s')",
             sky.name.c_str(), sky.dome_texture.c_str());

    return sky;
}

} // namespace swbf
