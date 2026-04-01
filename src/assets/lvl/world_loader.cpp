#include "world_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <cstring>

namespace swbf {

// ---------------------------------------------------------------------------
// WorldInstance::get
// ---------------------------------------------------------------------------

std::string WorldInstance::get(const std::string& key,
                               const std::string& default_val) const {
    for (const auto& prop : properties) {
        if (prop.key == key) {
            return prop.value;
        }
    }
    return default_val;
}

// ---------------------------------------------------------------------------
// Sub-chunk FourCCs used inside wrld/inst chunks
// ---------------------------------------------------------------------------

namespace {

constexpr FourCC TYPE = make_fourcc('T', 'Y', 'P', 'E');
constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');
constexpr FourCC XFRM = make_fourcc('X', 'F', 'R', 'M');
constexpr FourCC PROP = make_fourcc('P', 'R', 'O', 'P');
constexpr FourCC VALU = make_fourcc('V', 'A', 'L', 'U');
constexpr FourCC INFO = make_fourcc('I', 'N', 'F', 'O');

} // anonymous namespace

// ---------------------------------------------------------------------------
// WorldLoader::parse_property
// ---------------------------------------------------------------------------

EntityProperty WorldLoader::parse_property(ChunkReader& prop) {
    EntityProperty result;

    while (prop.has_children()) {
        ChunkReader child = prop.next_child();

        if (child.id() == NAME) {
            result.key = child.read_string();
        } else if (child.id() == VALU) {
            result.value = child.read_string();
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// WorldLoader::parse_instance
// ---------------------------------------------------------------------------

WorldInstance WorldLoader::parse_instance(ChunkReader& inst) {
    WorldInstance instance;

    while (inst.has_children()) {
        ChunkReader child = inst.next_child();
        FourCC id = child.id();

        if (id == TYPE) {
            instance.class_name = child.read_string();
        }
        else if (id == NAME) {
            instance.instance_name = child.read_string();
        }
        else if (id == XFRM) {
            // XFRM stores a 3x3 rotation matrix (column-major) followed by
            // a 3-float position vector = 12 floats = 48 bytes.
            if (child.remaining() >= 48) {
                // Read rotation matrix (3 columns of 3 floats each)
                for (int i = 0; i < 9; ++i) {
                    instance.transform.rotation[i] = child.read<float>();
                }
                // Read position
                instance.transform.position[0] = child.read<float>();
                instance.transform.position[1] = child.read<float>();
                instance.transform.position[2] = child.read<float>();
            }
            else if (child.remaining() >= 28) {
                // Alternate format: quaternion (4 floats) + position (3 floats)
                float qx = child.read<float>();
                float qy = child.read<float>();
                float qz = child.read<float>();
                float qw = child.read<float>();

                // Convert quaternion to 3x3 rotation matrix
                float xx = qx * qx, yy = qy * qy, zz = qz * qz;
                float xy = qx * qy, xz = qx * qz, yz = qy * qz;
                float wx = qw * qx, wy = qw * qy, wz = qw * qz;

                instance.transform.rotation[0] = 1.0f - 2.0f * (yy + zz);
                instance.transform.rotation[1] = 2.0f * (xy + wz);
                instance.transform.rotation[2] = 2.0f * (xz - wy);

                instance.transform.rotation[3] = 2.0f * (xy - wz);
                instance.transform.rotation[4] = 1.0f - 2.0f * (xx + zz);
                instance.transform.rotation[5] = 2.0f * (yz + wx);

                instance.transform.rotation[6] = 2.0f * (xz + wy);
                instance.transform.rotation[7] = 2.0f * (yz - wx);
                instance.transform.rotation[8] = 1.0f - 2.0f * (xx + yy);

                instance.transform.position[0] = child.read<float>();
                instance.transform.position[1] = child.read<float>();
                instance.transform.position[2] = child.read<float>();
            }
            else {
                LOG_WARN("WorldLoader: XFRM chunk too small (%zu bytes), "
                         "expected 48 or 28", child.remaining());
            }
        }
        else if (id == PROP) {
            EntityProperty prop = parse_property(child);
            if (!prop.key.empty()) {
                instance.properties.push_back(std::move(prop));
            }
        }
    }

    return instance;
}

// ---------------------------------------------------------------------------
// WorldLoader::load
// ---------------------------------------------------------------------------

WorldData WorldLoader::load(ChunkReader& chunk) {
    WorldData world;

    // wrld chunks may be double-wrapped like entity classes:
    //   wrld -> wrld (inner) -> children
    // Handle both cases.
    std::vector<ChunkReader> top_children = chunk.get_children();

    std::vector<ChunkReader> children;
    if (!top_children.empty() && top_children[0].id() == chunk_id::wrld) {
        children = top_children[0].get_children();
    } else {
        children = std::move(top_children);
    }

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == chunk_id::inst) {
            WorldInstance instance = parse_instance(child);
            if (!instance.class_name.empty() || !instance.instance_name.empty()) {
                LOG_DEBUG("WorldLoader: instance '%s' class='%s' pos=(%.1f, %.1f, %.1f)",
                          instance.instance_name.c_str(),
                          instance.class_name.c_str(),
                          static_cast<double>(instance.transform.position[0]),
                          static_cast<double>(instance.transform.position[1]),
                          static_cast<double>(instance.transform.position[2]));
                world.instances.push_back(std::move(instance));
            }
        }
        else if (id == NAME) {
            world.name = child.read_string();
        }
        else if (id == INFO) {
            // INFO may contain world metadata (instance count, etc.)
            // Skip for now — we count instances from the inst chunks.
        }
    }

    LOG_INFO("WorldLoader: loaded %zu instances (world='%s')",
             world.instances.size(), world.name.c_str());

    return world;
}

} // namespace swbf
