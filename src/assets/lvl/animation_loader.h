#pragma once

#include "assets/ucfb/chunk_reader.h"
#include "game/animation.h"
#include "game/skeleton.h"

#include <string>
#include <vector>

namespace swbf {

// ---------------------------------------------------------------------------
// AnimationLoader -- parses animation data from .lvl UCFB chunks
//
// SWBF .lvl files store animation data in two chunk types:
//   - zaf_  (animation file -- contains the raw ANM2 hierarchy)
//   - zaa_  (animation set -- a named group referencing zaf_ data)
//
// Inside those wrappers, the actual animation data lives in the standard
// MSH-format chunks:
//   - ANM2  (animation container)
//     - CYCL (cycle definitions: name, fps, frame range, play style)
//     - KFR3 (keyframe data: per-bone translation + rotation keyframes)
//
// The loader also handles:
//   - SKL2  (skeleton definition with CRC32 bone name hashes)
//   - BLN2  (blend factors, informational only)
//
// Usage:
//   AnimationLoader loader;
//   // Parse a zaf_ or ANM2 chunk:
//   auto clips = loader.load(chunk, &skeleton);
//   for (auto& clip : clips) {
//       anim_system.register_clip(clip);
//   }
// ---------------------------------------------------------------------------

/// Result of loading a skeleton from SKL2 + MODL hierarchy.
struct SkeletonLoadResult {
    Skeleton skeleton;
    bool     valid = false;
};

class AnimationLoader {
public:
    /// Parse animation clips from a top-level animation chunk.
    ///
    /// Accepts chunks tagged as zaf_, zaa_, or ANM2.  The chunk is walked
    /// recursively until ANM2 > CYCL + KFR3 are found.
    ///
    /// @param chunk     The UCFB chunk to parse.
    /// @param skeleton  Optional skeleton for resolving CRC32 bone hashes to
    ///                  names.  If nullptr, bones are named by their CRC hash.
    /// @return A vector of AnimClip instances (one per CYCL entry).
    std::vector<AnimClip> load(ChunkReader chunk,
                               const Skeleton* skeleton = nullptr);

    /// Parse a skeleton from a skel chunk or from MODL hierarchy nodes.
    /// The skel chunk (chunk_id::skel) wraps a SKL2 chunk with CRC32 bone
    /// hashes.  Bone names can be resolved from the MODL node hierarchy.
    SkeletonLoadResult load_skeleton(ChunkReader chunk);

    /// Parse a skeleton from the MODL node hierarchy within an MSH2 chunk.
    /// Extracts bone names, parent relationships, and bind pose transforms
    /// from MODL sub-chunks whose names start with "bone_" or whose MTYP
    /// indicates bone type (3).
    SkeletonLoadResult load_skeleton_from_modl(ChunkReader msh2_chunk);

private:
    /// Parse an ANM2 container chunk.
    void parse_anm2(ChunkReader anm2, const Skeleton* skeleton,
                    std::vector<AnimClip>& out_clips);

    /// Parse a CYCL chunk (animation cycle definitions).
    struct CycleDef {
        std::string name;
        float       fps        = 29.97f;
        uint32_t    play_style = 0;
        uint32_t    first_frame = 0;
        uint32_t    last_frame  = 0;
    };
    std::vector<CycleDef> parse_cycl(ChunkReader cycl);

    /// Parse a KFR3 chunk (keyframe data).
    /// Returns per-bone animation tracks keyed by bone CRC32 hash.
    struct BoneKeyframes {
        u32  bone_crc = 0;
        // Separate translation and rotation keyframes (as stored in KFR3).
        struct TranslationKey {
            u32   frame;
            float x, y, z;
        };
        struct RotationKey {
            u32   frame;
            float x, y, z, w;
        };
        std::vector<TranslationKey> translations;
        std::vector<RotationKey>    rotations;
    };
    std::vector<BoneKeyframes> parse_kfr3(ChunkReader kfr3);

    /// Convert parsed cycle definitions + bone keyframes into AnimClip instances.
    /// Each cycle defines a frame range; keyframes within that range are
    /// extracted and converted to time-based AnimTracks.
    std::vector<AnimClip> build_clips(
        const std::vector<CycleDef>& cycles,
        const std::vector<BoneKeyframes>& all_keyframes,
        const Skeleton* skeleton);

    /// Resolve a bone CRC32 hash to a name string.
    /// Uses the skeleton if available, otherwise generates a name like
    /// "bone_0xDEADBEEF".
    static std::string resolve_bone_name(u32 crc, const Skeleton* skeleton);
};

} // namespace swbf
