#pragma once

#include "core/types.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace swbf {

// ===========================================================================
// Skeleton data structures
//
// A Skeleton describes a bone hierarchy loaded from .lvl files (SKL2 chunks
// and MODL node trees).  Each bone is identified by a CRC32 hash of its name
// (as stored in SKL2/KFR3) and optionally by its string name (resolved from
// MODL NAME chunks).
//
// The skeleton is shared between the model and the animation system so that
// the animation loader can resolve bone CRC hashes to hierarchy indices and
// the renderer can build the final bone matrix palette.
// ===========================================================================

/// Maximum number of bones supported per skeleton.  SWBF soldier skeletons
/// have ~40 bones; vehicles have fewer.  64 is more than enough and keeps
/// the GPU uniform array a fixed size.
static constexpr int MAX_SKELETON_BONES = 64;

/// CRC32 hash function matching the Zero Engine bone name hashing.
/// This is a standard CRC32 (ISO 3309 / ITU-T V.42) with polynomial 0xEDB88320
/// applied to the lowercase version of each character.
inline u32 swbf_crc32(const char* str) {
    u32 crc = 0xFFFFFFFF;
    while (*str) {
        // Convert to lowercase for case-insensitive matching.
        char c = *str;
        if (c >= 'A' && c <= 'Z') c += ('a' - 'A');

        crc ^= static_cast<u32>(static_cast<u8>(c));
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
        ++str;
    }
    return crc ^ 0xFFFFFFFF;
}

inline u32 swbf_crc32(const std::string& str) {
    return swbf_crc32(str.c_str());
}

// ---------------------------------------------------------------------------
// Bone
// ---------------------------------------------------------------------------

/// A single bone in a skeleton hierarchy.
struct Bone {
    std::string name;         // e.g. "bone_root", "bone_l_upperarm"
    u32         name_crc = 0; // CRC32 of the name (for matching KFR3 data)
    int         parent   = -1; // parent bone index, -1 = root

    // Local bind pose transform (from TRAN chunk in MODL).
    float bind_position[3]  = {0.0f, 0.0f, 0.0f};
    float bind_rotation[4]  = {0.0f, 0.0f, 0.0f, 1.0f}; // quaternion xyzw
    float bind_scale[3]     = {1.0f, 1.0f, 1.0f};

    // Inverse bind pose matrix (world-space).  Computed once after the skeleton
    // is fully loaded.  This transforms vertices from model space to bone space.
    float inv_bind_matrix[16] = {
        1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1
    };
};

// ---------------------------------------------------------------------------
// Skeleton
// ---------------------------------------------------------------------------

/// A complete skeleton definition, typically loaded from a model's MODL
/// hierarchy (PRNT/TRAN chains) and/or SKL2 chunk.
struct Skeleton {
    std::string name;
    std::vector<Bone> bones;

    // Fast lookup: CRC32 hash -> bone index.
    std::unordered_map<u32, int> crc_to_index;

    // Fast lookup: name string -> bone index.
    std::unordered_map<std::string, int> name_to_index;

    // ENVL indirection table (from GEOM > ENVL chunks).
    // envl_to_bone[i] maps ENVL index i to the bone index in this skeleton.
    // Bone weights in WGHT reference ENVL indices, which then map through
    // this table to actual bones.
    std::vector<int> envl_to_bone;

    /// Add a bone and update the lookup maps.  Returns the index of the
    /// newly added bone.
    int add_bone(const Bone& bone) {
        int idx = static_cast<int>(bones.size());
        bones.push_back(bone);
        if (bone.name_crc != 0) {
            crc_to_index[bone.name_crc] = idx;
        }
        if (!bone.name.empty()) {
            name_to_index[bone.name] = idx;
        }
        return idx;
    }

    /// Find a bone by CRC32 hash.  Returns -1 if not found.
    int find_by_crc(u32 crc) const {
        auto it = crc_to_index.find(crc);
        return (it != crc_to_index.end()) ? it->second : -1;
    }

    /// Find a bone by name.  Returns -1 if not found.
    int find_by_name(const std::string& name) const {
        auto it = name_to_index.find(name);
        return (it != name_to_index.end()) ? it->second : -1;
    }

    /// Compute the inverse bind matrices for all bones.
    /// Must be called after the skeleton hierarchy and bind poses are fully set up.
    void compute_inverse_bind_matrices();
};

} // namespace swbf
