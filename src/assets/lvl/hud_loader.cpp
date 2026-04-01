#include "hud_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <cstring>

namespace swbf {

namespace {

constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');
constexpr FourCC INFO = make_fourcc('I', 'N', 'F', 'O');
constexpr FourCC DATA = make_fourcc('D', 'A', 'T', 'A');

} // anonymous namespace

HudLayout HudLoader::load(ChunkReader& chunk) {
    HudLayout hud;

    // Handle possible double-wrapping.
    std::vector<ChunkReader> top_children = chunk.get_children();

    std::vector<ChunkReader> children;
    if (!top_children.empty() && top_children[0].id() == chunk_id::hud_) {
        children = top_children[0].get_children();
    } else {
        children = std::move(top_children);
    }

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == NAME) {
            hud.name = child.read_string();
        }
        else if (id == DATA) {
            if (child.size() > 0) {
                hud.raw_data.assign(child.data(), child.data() + child.size());
            }
        }
        else if (id == INFO) {
            // HUD metadata — skip for now.
        }
    }

    LOG_DEBUG("HudLoader: loaded HUD layout '%s' (%zu bytes data)",
              hud.name.c_str(), hud.raw_data.size());

    return hud;
}

} // namespace swbf
