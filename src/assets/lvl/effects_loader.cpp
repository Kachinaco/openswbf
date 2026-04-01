#include "effects_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <cstring>

namespace swbf {

namespace {

constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');
constexpr FourCC INFO = make_fourcc('I', 'N', 'F', 'O');
constexpr FourCC DATA = make_fourcc('D', 'A', 'T', 'A');
constexpr FourCC TYPE = make_fourcc('T', 'Y', 'P', 'E');
constexpr FourCC TNAM = make_fourcc('T', 'N', 'A', 'M');

} // anonymous namespace

EffectData EffectsLoader::load(ChunkReader& chunk) {
    EffectData fx;

    // Handle possible double-wrapping.
    std::vector<ChunkReader> top_children = chunk.get_children();

    std::vector<ChunkReader> children;
    if (!top_children.empty() && top_children[0].id() == chunk_id::fx__) {
        children = top_children[0].get_children();
    } else {
        children = std::move(top_children);
    }

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == NAME) {
            fx.name = child.read_string();
        }
        else if (id == TYPE) {
            if (child.remaining() > 0) {
                try {
                    fx.type = child.read_string();
                } catch (...) {}
            }
        }
        else if (id == TNAM) {
            if (child.remaining() > 0) {
                try {
                    fx.texture = child.read_string();
                } catch (...) {}
            }
        }
        else if (id == DATA) {
            // Store raw data for future parsing.
            if (child.size() > 0) {
                fx.raw_data.assign(child.data(), child.data() + child.size());
            }
        }
        else if (id == INFO) {
            // Effect metadata — skip for now.
        }
    }

    LOG_DEBUG("EffectsLoader: loaded effect '%s' (type='%s', texture='%s', %zu bytes data)",
              fx.name.c_str(), fx.type.c_str(), fx.texture.c_str(),
              fx.raw_data.size());

    return fx;
}

} // namespace swbf
