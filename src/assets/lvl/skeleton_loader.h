#pragma once

#include "core/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// Skeleton data structures
//
// Skeletons in SWBF define the bone hierarchy used for skinned mesh animation.
// In munged .lvl files, skeleton data appears as skel chunks containing:
//   - Bone definitions with name hashes and parent indices
//   - Transform data (bind pose) for each bone
//
// The skel chunk in munged .lvl files uses a different layout from the .msh
// SKL2 chunk. The munged format stores:
//
//   skel
//     INFO  — bone count and metadata
//     NAME  — skeleton name
//     BMAP  — bone name hash array (FNV hashes for runtime lookup)
//     BNAM  — concatenated null-terminated bone name strings
//     BPAR  — parent bone indices (int32 per bone, -1 = root)
//     TRAN  — bone bind-pose transforms (mat4x3 or decomposed)
// ---------------------------------------------------------------------------

/// A single bone in the skeleton hierarchy.
struct Bone {
    std::string name;            // Bone name (e.g. "bone_root", "bone_l_upperarm")
    uint32_t    name_hash = 0;   // FNV hash of the bone name for runtime lookup
    int32_t     parent    = -1;  // Index of parent bone (-1 = root bone)

    // Bind-pose transform (local space relative to parent).
    float rotation[4]    = {0.0f, 0.0f, 0.0f, 1.0f};  // Quaternion (x,y,z,w)
    float position[3]    = {0.0f, 0.0f, 0.0f};         // Translation
    float scale[3]       = {1.0f, 1.0f, 1.0f};         // Scale (usually uniform 1.0)
};

/// A fully parsed skeleton.
struct Skeleton {
    std::string       name;    // Skeleton name
    std::vector<Bone> bones;   // Bones in hierarchy order (index 0 is typically root)

    /// Find a bone by name hash. Returns -1 if not found.
    int find_bone(uint32_t name_hash) const;

    /// Find a bone by name. Returns -1 if not found.
    int find_bone(const std::string& name) const;
};

// ---------------------------------------------------------------------------
// SkeletonLoader — parses skel UCFB chunks from munged .lvl files.
//
// Usage:
//   SkeletonLoader loader;
//   Skeleton skel = loader.load(skel_chunk);
// ---------------------------------------------------------------------------

class SkeletonLoader {
public:
    SkeletonLoader() = default;

    /// Parse a skel chunk and return the decoded skeleton.
    Skeleton load(ChunkReader& chunk);
};

} // namespace swbf
