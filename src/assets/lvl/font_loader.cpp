#include "font_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <cstring>

namespace swbf {

namespace {

constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');
constexpr FourCC INFO = make_fourcc('I', 'N', 'F', 'O');
constexpr FourCC DATA = make_fourcc('D', 'A', 'T', 'A');
constexpr FourCC TNAM = make_fourcc('T', 'N', 'A', 'M');

} // anonymous namespace

FontData FontLoader::load(ChunkReader& chunk) {
    FontData fnt;

    // Handle possible double-wrapping.
    std::vector<ChunkReader> top_children = chunk.get_children();

    std::vector<ChunkReader> children;
    if (!top_children.empty() && top_children[0].id() == chunk_id::font) {
        children = top_children[0].get_children();
    } else {
        children = std::move(top_children);
    }

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == NAME) {
            fnt.name = child.read_string();
        }
        else if (id == TNAM) {
            // Atlas texture name.
            if (child.remaining() > 0) {
                try {
                    fnt.atlas_texture_name = child.read_string();
                } catch (...) {}
            }
        }
        else if (id == INFO) {
            // Font metadata: line height, glyph count, etc.
            if (child.remaining() >= 8) {
                fnt.line_height = child.read<float>();
                fnt.baseline = child.read<float>();
            }
        }
        else if (id == DATA) {
            if (child.size() > 0) {
                fnt.raw_data.assign(child.data(), child.data() + child.size());
            }
        }
    }

    LOG_DEBUG("FontLoader: loaded font '%s' (atlas='%s', line_height=%.1f, %zu bytes data)",
              fnt.name.c_str(), fnt.atlas_texture_name.c_str(),
              static_cast<double>(fnt.line_height), fnt.raw_data.size());

    return fnt;
}

} // namespace swbf
