#include "skeleton_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <cstring>

namespace swbf {

// ---------------------------------------------------------------------------
// Sub-chunk FourCCs used inside skel chunks
// ---------------------------------------------------------------------------

namespace {

constexpr FourCC INFO = make_fourcc('I', 'N', 'F', 'O');
constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');
constexpr FourCC BMAP = make_fourcc('B', 'M', 'A', 'P');
constexpr FourCC BNAM = make_fourcc('B', 'N', 'A', 'M');
constexpr FourCC BPAR = make_fourcc('B', 'P', 'A', 'R');
constexpr FourCC TRAN = make_fourcc('T', 'R', 'A', 'N');
constexpr FourCC SKL2 = make_fourcc('S', 'K', 'L', '2');
constexpr FourCC BLN2 = make_fourcc('B', 'L', 'N', '2');

} // anonymous namespace

// ---------------------------------------------------------------------------
// SkeletonLoader
// ---------------------------------------------------------------------------

Skeleton SkeletonLoader::load(ChunkReader& chunk) {
    Skeleton skeleton;

    if (chunk.id() != chunk_id::skel) {
        LOG_WARN("SkeletonLoader: expected skel chunk, got 0x%08X", chunk.id());
        return skeleton;
    }

    uint32_t bone_count = 0;

    // Collect all children for multi-pass processing.
    // We need to read INFO first to know bone_count, then BNAM/BMAP/BPAR/TRAN.
    std::vector<ChunkReader> children = chunk.get_children();

    // Pass 1: Find INFO and NAME to get bone count and skeleton name.
    for (auto& child : children) {
        if (child.id() == INFO) {
            if (child.remaining() >= 4) {
                bone_count = child.read<uint32_t>();
            }
        } else if (child.id() == NAME) {
            skeleton.name = child.read_string();
        }
    }

    if (bone_count == 0) {
        LOG_WARN("SkeletonLoader: bone_count is 0, skeleton '%s' is empty",
                 skeleton.name.c_str());
        return skeleton;
    }

    // Pre-allocate bones.
    skeleton.bones.resize(bone_count);

    // Pass 2: Read bone data from BNAM, BMAP, BPAR, TRAN chunks.
    for (auto& child : children) {
        FourCC id = child.id();

        if (id == BNAM) {
            // Concatenated null-terminated bone name strings.
            // Read them sequentially; the order matches bone indices.
            for (uint32_t i = 0; i < bone_count; ++i) {
                if (child.remaining() == 0) break;
                try {
                    skeleton.bones[i].name = child.read_string();
                } catch (...) {
                    break;
                }
            }
        }
        else if (id == BMAP) {
            // Array of uint32 name hashes (CRC32), one per bone.
            for (uint32_t i = 0; i < bone_count; ++i) {
                if (child.remaining() < 4) break;
                skeleton.bones[i].name_crc = child.read<uint32_t>();
            }
        }
        else if (id == BPAR) {
            // Array of int32 parent indices, one per bone.
            // -1 means root (no parent).
            for (uint32_t i = 0; i < bone_count; ++i) {
                if (child.remaining() < 4) break;
                skeleton.bones[i].parent = child.read<int32_t>();
            }
        }
        else if (id == TRAN) {
            // Bind-pose transforms for each bone.
            // Each transform is stored as: scale(3 floats) + rotation(4 floats)
            // + position(3 floats) = 40 bytes per bone.
            //
            // Some formats use a compact 28-byte layout:
            //   rotation(4 floats) + position(3 floats)
            //
            // Detect layout from total size.
            std::size_t bytes_per_bone = 0;
            if (child.remaining() >= bone_count * 40) {
                bytes_per_bone = 40;  // Full: scale + rotation + position
            } else if (child.remaining() >= bone_count * 28) {
                bytes_per_bone = 28;  // Compact: rotation + position
            } else if (child.remaining() >= bone_count * 12) {
                bytes_per_bone = 12;  // Minimal: position only
            }

            for (uint32_t i = 0; i < bone_count; ++i) {
                if (child.remaining() < bytes_per_bone) break;

                if (bytes_per_bone == 40) {
                    // Scale (3 floats)
                    skeleton.bones[i].bind_scale[0] = child.read<float>();
                    skeleton.bones[i].bind_scale[1] = child.read<float>();
                    skeleton.bones[i].bind_scale[2] = child.read<float>();
                    // Rotation quaternion (4 floats: x,y,z,w)
                    skeleton.bones[i].bind_rotation[0] = child.read<float>();
                    skeleton.bones[i].bind_rotation[1] = child.read<float>();
                    skeleton.bones[i].bind_rotation[2] = child.read<float>();
                    skeleton.bones[i].bind_rotation[3] = child.read<float>();
                    // Position (3 floats)
                    skeleton.bones[i].bind_position[0] = child.read<float>();
                    skeleton.bones[i].bind_position[1] = child.read<float>();
                    skeleton.bones[i].bind_position[2] = child.read<float>();
                } else if (bytes_per_bone == 28) {
                    // Rotation quaternion (4 floats)
                    skeleton.bones[i].bind_rotation[0] = child.read<float>();
                    skeleton.bones[i].bind_rotation[1] = child.read<float>();
                    skeleton.bones[i].bind_rotation[2] = child.read<float>();
                    skeleton.bones[i].bind_rotation[3] = child.read<float>();
                    // Position (3 floats)
                    skeleton.bones[i].bind_position[0] = child.read<float>();
                    skeleton.bones[i].bind_position[1] = child.read<float>();
                    skeleton.bones[i].bind_position[2] = child.read<float>();
                } else if (bytes_per_bone == 12) {
                    // Position only (3 floats)
                    skeleton.bones[i].bind_position[0] = child.read<float>();
                    skeleton.bones[i].bind_position[1] = child.read<float>();
                    skeleton.bones[i].bind_position[2] = child.read<float>();
                }
            }
        }
        else if (id == SKL2) {
            // Alternate skeleton format: SKL2 sub-chunk from .msh format.
            // Contains per-bone: CRC hash (4), type (4), constraint (4),
            //                    length1 (4), length2 (4) = 20 bytes/bone
            //
            // First 4 bytes = bone count.
            if (child.remaining() >= 4) {
                uint32_t skl2_count = child.read<uint32_t>();

                // If we haven't allocated bones yet (no INFO chunk), do so now.
                if (skeleton.bones.empty()) {
                    bone_count = skl2_count;
                    skeleton.bones.resize(bone_count);
                }

                uint32_t limit = std::min(skl2_count, bone_count);
                for (uint32_t i = 0; i < limit; ++i) {
                    if (child.remaining() < 20) break;
                    skeleton.bones[i].name_crc = child.read<uint32_t>();
                    child.skip(16); // type, constraint, length1, length2
                }
            }
        }
    }

    // Build the lookup maps (crc_to_index, name_to_index) now that all
    // bone data has been populated.
    for (std::size_t i = 0; i < skeleton.bones.size(); ++i) {
        const auto& bone = skeleton.bones[i];
        if (bone.name_crc != 0) {
            skeleton.crc_to_index[bone.name_crc] = static_cast<int>(i);
        }
        if (!bone.name.empty()) {
            skeleton.name_to_index[bone.name] = static_cast<int>(i);
        }
    }

    LOG_DEBUG("SkeletonLoader: loaded '%s' with %u bones",
              skeleton.name.c_str(), bone_count);

    return skeleton;
}

} // namespace swbf
