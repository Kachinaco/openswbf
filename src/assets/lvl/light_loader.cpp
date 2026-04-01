#include "light_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <cstring>

namespace swbf {

// ---------------------------------------------------------------------------
// Sub-chunk FourCCs used inside lght chunks
// ---------------------------------------------------------------------------

namespace {

constexpr FourCC INFO = make_fourcc('I', 'N', 'F', 'O');
constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');
constexpr FourCC DATA = make_fourcc('D', 'A', 'T', 'A');
constexpr FourCC TYPE = make_fourcc('T', 'Y', 'P', 'E');
constexpr FourCC COLR = make_fourcc('C', 'O', 'L', 'R');
constexpr FourCC POSN = make_fourcc('P', 'O', 'S', 'N');
constexpr FourCC ROTN = make_fourcc('R', 'O', 'T', 'N');

// Map raw type values to our LightType enum.
LightType classify_light_type(uint32_t raw_type) {
    switch (raw_type) {
        case 0: return LightType::Directional;
        case 1: return LightType::Point;
        case 2: return LightType::Spot;
        default:
            LOG_WARN("LightLoader: unknown light type %u", raw_type);
            return LightType::Point;
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// parse_light_data — parse a single light from a DATA sub-chunk
//
// Light DATA chunk layout (observed from munged .lvl files):
//
// Packed format (per light, ~76+ bytes):
//   offset  0: uint32 light_type (0=dir, 1=point, 2=spot)
//   offset  4: float color_r
//   offset  8: float color_g
//   offset 12: float color_b
//   offset 16: float intensity
//   offset 20: float position_x
//   offset 24: float position_y
//   offset 28: float position_z
//   offset 32: float rotation_x (quaternion)
//   offset 36: float rotation_y
//   offset 40: float rotation_z
//   offset 44: float rotation_w
//   offset 48: float range
//   offset 52: float inner_cone (spot only)
//   offset 56: float outer_cone (spot only)
//   offset 60: uint32 flags (static, bidirectional, etc.)
// ---------------------------------------------------------------------------

Light LightLoader::parse_light_data(ChunkReader& data_chunk) {
    Light light;

    // Read the packed light data.
    if (data_chunk.remaining() < 4) {
        return light;
    }

    // Type
    light.type = classify_light_type(data_chunk.read<uint32_t>());

    // Color RGB
    if (data_chunk.remaining() >= 12) {
        light.color[0] = data_chunk.read<float>();
        light.color[1] = data_chunk.read<float>();
        light.color[2] = data_chunk.read<float>();
    }

    // Intensity
    if (data_chunk.remaining() >= 4) {
        light.intensity = data_chunk.read<float>();
    }

    // Color alpha from intensity (keep color[3] as 1.0)
    light.color[3] = 1.0f;

    // Position
    if (data_chunk.remaining() >= 12) {
        light.position[0] = data_chunk.read<float>();
        light.position[1] = data_chunk.read<float>();
        light.position[2] = data_chunk.read<float>();
    }

    // Rotation quaternion
    if (data_chunk.remaining() >= 16) {
        light.rotation[0] = data_chunk.read<float>();
        light.rotation[1] = data_chunk.read<float>();
        light.rotation[2] = data_chunk.read<float>();
        light.rotation[3] = data_chunk.read<float>();
    }

    // Range
    if (data_chunk.remaining() >= 4) {
        light.range = data_chunk.read<float>();
    }

    // Spot cone angles (only meaningful for spot lights)
    if (data_chunk.remaining() >= 8) {
        light.inner_cone_angle = data_chunk.read<float>();
        light.outer_cone_angle = data_chunk.read<float>();
    }

    // Flags
    if (data_chunk.remaining() >= 4) {
        uint32_t flags = data_chunk.read<uint32_t>();
        light.bidirectional = (flags & 0x1) != 0;
        light.is_static     = (flags & 0x2) != 0;
    }

    return light;
}

// ---------------------------------------------------------------------------
// load — main entry point: parse a lght chunk
// ---------------------------------------------------------------------------

std::vector<Light> LightLoader::load(ChunkReader& chunk) {
    std::vector<Light> lights;

    if (chunk.id() != chunk_id::lght) {
        LOG_WARN("LightLoader: expected lght chunk, got 0x%08X", chunk.id());
        return lights;
    }

    uint32_t expected_count = 0;
    std::string current_name;

    std::vector<ChunkReader> children = chunk.get_children();

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == INFO) {
            // Light count.
            if (child.remaining() >= 4) {
                expected_count = child.read<uint32_t>();
                lights.reserve(expected_count);
            }
        }
        else if (id == NAME) {
            // Light name — applies to the next DATA chunk.
            current_name = child.read_string();
        }
        else if (id == DATA) {
            // Light data — can be a single packed light, or multiple lights
            // packed sequentially.
            if (child.remaining() >= 4) {
                // Try to determine if this is a single light or packed array.
                // Minimum packed light size is ~52 bytes (type + color + intensity
                // + position + rotation + range).
                static constexpr std::size_t MIN_LIGHT_SIZE = 52;

                if (child.remaining() >= MIN_LIGHT_SIZE * 2 &&
                    expected_count > 1 && lights.empty()) {
                    // Likely a packed array of all lights.
                    for (uint32_t i = 0; i < expected_count; ++i) {
                        if (child.remaining() < MIN_LIGHT_SIZE) break;
                        Light light = parse_light_data(child);
                        lights.push_back(std::move(light));
                    }
                } else {
                    // Single light in this DATA chunk.
                    Light light = parse_light_data(child);
                    if (!current_name.empty()) {
                        light.name = current_name;
                        current_name.clear();
                    }
                    lights.push_back(std::move(light));
                }
            }
        }
        else if (id == TYPE) {
            // Alternate format: TYPE + color/position as separate sub-chunks.
            // This handles the case where each light is defined by individual
            // sub-chunks rather than a packed DATA blob.
            Light light;
            if (!current_name.empty()) {
                light.name = current_name;
                current_name.clear();
            }
            if (child.remaining() >= 4) {
                light.type = classify_light_type(child.read<uint32_t>());
            }
            lights.push_back(std::move(light));
        }
        else if (id == COLR && !lights.empty()) {
            // Color data for the most recently added light.
            Light& light = lights.back();
            if (child.remaining() >= 12) {
                light.color[0] = child.read<float>();
                light.color[1] = child.read<float>();
                light.color[2] = child.read<float>();
                if (child.remaining() >= 4) {
                    light.color[3] = child.read<float>();
                }
            }
        }
        else if (id == POSN && !lights.empty()) {
            // Position for the most recently added light.
            Light& light = lights.back();
            if (child.remaining() >= 12) {
                light.position[0] = child.read<float>();
                light.position[1] = child.read<float>();
                light.position[2] = child.read<float>();
            }
        }
        else if (id == ROTN && !lights.empty()) {
            // Rotation for the most recently added light.
            Light& light = lights.back();
            if (child.remaining() >= 16) {
                light.rotation[0] = child.read<float>();
                light.rotation[1] = child.read<float>();
                light.rotation[2] = child.read<float>();
                light.rotation[3] = child.read<float>();
            }
        }
    }

    LOG_DEBUG("LightLoader: loaded %zu lights (expected %u)",
              lights.size(), expected_count);

    return lights;
}

} // namespace swbf
