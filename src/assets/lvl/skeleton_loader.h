#pragma once

#include "core/types.h"
#include "game/skeleton.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// SkeletonLoader -- parses skel UCFB chunks from munged .lvl files.
//
// Outputs the unified Skeleton/Bone types from game/skeleton.h, which are
// shared by the asset pipeline, animation system, and renderer.
//
// The skel chunk in munged .lvl files stores:
//   skel
//     INFO  -- bone count and metadata
//     NAME  -- skeleton name
//     BMAP  -- bone name hash array (CRC32 hashes for runtime lookup)
//     BNAM  -- concatenated null-terminated bone name strings
//     BPAR  -- parent bone indices (int32 per bone, -1 = root)
//     TRAN  -- bone bind-pose transforms (mat4x3 or decomposed)
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
