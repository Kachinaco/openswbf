#include "animation_loader.h"

#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace swbf {

// ===========================================================================
// Public: load animation clips from a chunk
// ===========================================================================

std::vector<AnimClip> AnimationLoader::load(ChunkReader chunk,
                                            const Skeleton* skeleton) {
    std::vector<AnimClip> clips;

    FourCC id = chunk.id();

    // If this IS an ANM2 chunk directly, parse it.
    if (id == chunk_id::ANM2) {
        parse_anm2(chunk, skeleton, clips);
        return clips;
    }

    // For zaf_, zaa_, or other wrapper chunks, walk children looking for ANM2.
    if (id == chunk_id::zaf_ || id == chunk_id::zaa_ ||
        id == chunk_id::ucfb || id == chunk_id::HEDR) {
        auto children = chunk.get_children();
        for (auto& child : children) {
            FourCC child_id = child.id();

            if (child_id == chunk_id::ANM2) {
                parse_anm2(child, skeleton, clips);
            }
            else if (child_id == chunk_id::ucfb ||
                     child_id == chunk_id::HEDR ||
                     child_id == chunk_id::MSH2) {
                // Recurse into wrapper containers.
                auto sub_clips = load(child, skeleton);
                clips.insert(clips.end(), sub_clips.begin(), sub_clips.end());
            }
        }
    }

    if (clips.empty()) {
        LOG_WARN("AnimationLoader: no ANM2 data found in chunk");
    }

    return clips;
}

// ===========================================================================
// Public: load skeleton from a skel chunk
// ===========================================================================

SkeletonLoadResult AnimationLoader::load_skeleton(ChunkReader chunk) {
    SkeletonLoadResult result;

    auto children = chunk.get_children();
    for (auto& child : children) {
        if (child.id() == chunk_id::SKL2) {
            // SKL2 format:
            //   u32 bone_count
            //   per bone (20 bytes):
            //     u32 crc32_name_hash
            //     u32 bone_type
            //     u32 constraint_type
            //     float bone_length_1
            //     float bone_length_2

            if (child.remaining() < 4) continue;

            u32 bone_count = child.read<u32>();

            LOG_INFO("AnimationLoader: SKL2 with %u bones", bone_count);

            for (u32 i = 0; i < bone_count; ++i) {
                if (child.remaining() < 20) break;

                Bone bone;
                bone.name_crc = child.read<u32>();
                u32 bone_type = child.read<u32>();
                child.skip(4);  // constraint_type
                child.skip(4);  // bone_length_1
                child.skip(4);  // bone_length_2

                (void)bone_type;

                // Name will be resolved later from MODL hierarchy.
                // For now, use a placeholder.
                char name_buf[32];
                std::snprintf(name_buf, sizeof(name_buf),
                              "bone_0x%08X", bone.name_crc);
                bone.name = name_buf;

                result.skeleton.add_bone(bone);
            }

            result.valid = true;
        }
    }

    return result;
}

// ===========================================================================
// Public: load skeleton from MODL hierarchy
// ===========================================================================

SkeletonLoadResult AnimationLoader::load_skeleton_from_modl(
        ChunkReader msh2_chunk) {
    SkeletonLoadResult result;

    // Walk all MODL sub-chunks inside MSH2 to find bone nodes.
    // Bones are identified by:
    //   1. MTYP == 3 (bone type)
    //   2. Name starting with "bone_"
    //   3. Name starting with "hp_" (hardpoints, treated as bones)
    //
    // Each MODL has: MTYP, MNDX, NAME, PRNT, TRAN

    struct NodeInfo {
        std::string name;
        std::string parent_name;
        u32         mndx = 0;  // 1-based model index
        u32         mtyp = 0;  // model type
        float       position[3]  = {0, 0, 0};
        float       rotation[4]  = {0, 0, 0, 1};
        float       scale[3]     = {1, 1, 1};
    };

    std::vector<NodeInfo> nodes;

    auto children = msh2_chunk.get_children();
    for (auto& child : children) {
        if (child.id() != chunk_id::MODL_sub) continue;

        NodeInfo node;
        auto modl_children = child.get_children();

        for (auto& mc : modl_children) {
            FourCC mc_id = mc.id();

            if (mc_id == chunk_id::MTYP) {
                node.mtyp = mc.read<u32>();
            } else if (mc_id == chunk_id::MNDX) {
                node.mndx = mc.read<u32>();
            } else if (mc_id == chunk_id::NAME) {
                node.name = mc.read_string();
            } else if (mc_id == chunk_id::PRNT) {
                node.parent_name = mc.read_string();
            } else if (mc_id == chunk_id::TRAN) {
                // TRAN: float scale[3], float rotation[4] (xyzw), float translation[3]
                // Total: 40 bytes
                if (mc.size() >= 40) {
                    node.scale[0]    = mc.read<float>();
                    node.scale[1]    = mc.read<float>();
                    node.scale[2]    = mc.read<float>();
                    node.rotation[0] = mc.read<float>();
                    node.rotation[1] = mc.read<float>();
                    node.rotation[2] = mc.read<float>();
                    node.rotation[3] = mc.read<float>();
                    node.position[0] = mc.read<float>();
                    node.position[1] = mc.read<float>();
                    node.position[2] = mc.read<float>();
                }
            }
        }

        if (!node.name.empty()) {
            nodes.push_back(std::move(node));
        }
    }

    // Now build the skeleton from all nodes that are bones.
    // Include ALL nodes in the hierarchy since even non-bone nodes may be
    // parents in the hierarchy chain.
    for (const auto& node : nodes) {
        Bone bone;
        bone.name = node.name;
        bone.name_crc = swbf_crc32(node.name);
        bone.bind_position[0] = node.position[0];
        bone.bind_position[1] = node.position[1];
        bone.bind_position[2] = node.position[2];
        bone.bind_rotation[0] = node.rotation[0];
        bone.bind_rotation[1] = node.rotation[1];
        bone.bind_rotation[2] = node.rotation[2];
        bone.bind_rotation[3] = node.rotation[3];
        bone.bind_scale[0] = node.scale[0];
        bone.bind_scale[1] = node.scale[1];
        bone.bind_scale[2] = node.scale[2];

        result.skeleton.add_bone(bone);
    }

    // Resolve parent indices by name.
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (!nodes[i].parent_name.empty()) {
            int parent_idx = result.skeleton.find_by_name(nodes[i].parent_name);
            if (parent_idx >= 0) {
                result.skeleton.bones[i].parent = parent_idx;
            }
        }
    }

    // Compute inverse bind matrices.
    result.skeleton.compute_inverse_bind_matrices();
    result.valid = !result.skeleton.bones.empty();

    if (result.valid) {
        LOG_INFO("AnimationLoader: loaded skeleton from MODL hierarchy (%zu bones)",
                 result.skeleton.bones.size());
    }

    return result;
}

// ===========================================================================
// Internal: parse ANM2 container
// ===========================================================================

void AnimationLoader::parse_anm2(ChunkReader anm2, const Skeleton* skeleton,
                                 std::vector<AnimClip>& out_clips) {
    std::vector<CycleDef>       cycles;
    std::vector<BoneKeyframes>  keyframes;

    auto children = anm2.get_children();
    for (auto& child : children) {
        if (child.id() == chunk_id::CYCL) {
            cycles = parse_cycl(child);
        } else if (child.id() == chunk_id::KFR3) {
            keyframes = parse_kfr3(child);
        }
    }

    if (cycles.empty()) {
        LOG_WARN("AnimationLoader: ANM2 has no CYCL data");
        return;
    }

    if (keyframes.empty()) {
        LOG_WARN("AnimationLoader: ANM2 has no KFR3 data");
        return;
    }

    auto clips = build_clips(cycles, keyframes, skeleton);
    out_clips.insert(out_clips.end(), clips.begin(), clips.end());

    LOG_INFO("AnimationLoader: parsed %zu animation clips from ANM2",
             clips.size());
}

// ===========================================================================
// Internal: parse CYCL (cycle definitions)
// ===========================================================================

std::vector<AnimationLoader::CycleDef> AnimationLoader::parse_cycl(
        ChunkReader cycl) {
    std::vector<CycleDef> cycles;

    if (cycl.remaining() < 4) return cycles;

    u32 count = cycl.read<u32>();

    for (u32 i = 0; i < count; ++i) {
        CycleDef def;

        // Animation name: 64 bytes, null-padded.
        if (cycl.remaining() < 80) break; // 64 + 4 + 4 + 4 + 4 = 80

        char name_buf[65] = {};
        for (int c = 0; c < 64; ++c) {
            name_buf[c] = static_cast<char>(cycl.read<u8>());
        }
        name_buf[64] = '\0';
        def.name = name_buf;

        // Trim trailing nulls/spaces.
        while (!def.name.empty() &&
               (def.name.back() == '\0' || def.name.back() == ' ')) {
            def.name.pop_back();
        }

        def.fps         = cycl.read<float>();
        def.play_style  = cycl.read<u32>();
        def.first_frame = cycl.read<u32>();
        def.last_frame  = cycl.read<u32>();

        LOG_DEBUG("AnimationLoader: CYCL[%u] '%s' fps=%.1f frames=%u-%u style=%u",
                  i, def.name.c_str(), static_cast<double>(def.fps),
                  def.first_frame, def.last_frame, def.play_style);

        cycles.push_back(std::move(def));
    }

    return cycles;
}

// ===========================================================================
// Internal: parse KFR3 (keyframe data)
// ===========================================================================

std::vector<AnimationLoader::BoneKeyframes> AnimationLoader::parse_kfr3(
        ChunkReader kfr3) {
    std::vector<BoneKeyframes> all_bones;

    if (kfr3.remaining() < 4) return all_bones;

    u32 bone_count = kfr3.read<u32>();

    for (u32 b = 0; b < bone_count; ++b) {
        BoneKeyframes bk;

        if (kfr3.remaining() < 16) break;

        bk.bone_crc = kfr3.read<u32>();
        u32 keyframe_type = kfr3.read<u32>(); // typically 0
        (void)keyframe_type;

        u32 num_translations = kfr3.read<u32>();
        u32 num_rotations    = kfr3.read<u32>();

        // Translation keyframes: 16 bytes each (u32 frame + float x,y,z)
        bk.translations.reserve(num_translations);
        for (u32 t = 0; t < num_translations; ++t) {
            if (kfr3.remaining() < 16) break;

            BoneKeyframes::TranslationKey tk;
            tk.frame = kfr3.read<u32>();
            tk.x     = kfr3.read<float>();
            tk.y     = kfr3.read<float>();
            tk.z     = kfr3.read<float>();
            bk.translations.push_back(tk);
        }

        // Rotation keyframes: 20 bytes each (u32 frame + float x,y,z,w)
        bk.rotations.reserve(num_rotations);
        for (u32 r = 0; r < num_rotations; ++r) {
            if (kfr3.remaining() < 20) break;

            BoneKeyframes::RotationKey rk;
            rk.frame = kfr3.read<u32>();
            rk.x     = kfr3.read<float>();
            rk.y     = kfr3.read<float>();
            rk.z     = kfr3.read<float>();
            rk.w     = kfr3.read<float>();
            bk.rotations.push_back(rk);
        }

        all_bones.push_back(std::move(bk));
    }

    LOG_DEBUG("AnimationLoader: KFR3 parsed %zu bone tracks", all_bones.size());
    return all_bones;
}

// ===========================================================================
// Internal: build AnimClips from cycle definitions + keyframes
// ===========================================================================

std::vector<AnimClip> AnimationLoader::build_clips(
        const std::vector<CycleDef>& cycles,
        const std::vector<BoneKeyframes>& all_keyframes,
        const Skeleton* skeleton) {
    std::vector<AnimClip> clips;

    for (const auto& cycle : cycles) {
        AnimClip clip;
        clip.name = cycle.name;
        clip.loop = (cycle.play_style == 1); // 0=once, 1=loop

        float fps = cycle.fps;
        if (fps <= 0.0f) fps = 29.97f; // default SWBF framerate

        u32 frame_count = 0;
        if (cycle.last_frame >= cycle.first_frame) {
            frame_count = cycle.last_frame - cycle.first_frame;
        }
        clip.duration = static_cast<float>(frame_count) / fps;
        if (clip.duration <= 0.0f) clip.duration = 0.001f; // avoid zero

        // Build an AnimTrack for each bone that has keyframes in this
        // cycle's frame range.
        for (const auto& bk : all_keyframes) {
            AnimTrack track;
            track.bone_name = resolve_bone_name(bk.bone_crc, skeleton);

            // Collect translation keyframes within this cycle's range.
            // The KFR3 translation and rotation keyframes may have different
            // counts and timings, so we need to build a merged keyframe list.
            //
            // Strategy: create a keyframe at each unique frame index from
            // both translation and rotation data within the cycle range.

            // Build a set of all frame indices in range.
            std::vector<u32> frame_indices;
            for (const auto& tk : bk.translations) {
                if (tk.frame >= cycle.first_frame &&
                    tk.frame <= cycle.last_frame) {
                    frame_indices.push_back(tk.frame);
                }
            }
            for (const auto& rk : bk.rotations) {
                if (rk.frame >= cycle.first_frame &&
                    rk.frame <= cycle.last_frame) {
                    frame_indices.push_back(rk.frame);
                }
            }

            // Remove duplicates and sort.
            std::sort(frame_indices.begin(), frame_indices.end());
            frame_indices.erase(
                std::unique(frame_indices.begin(), frame_indices.end()),
                frame_indices.end());

            if (frame_indices.empty()) continue;

            // For each unique frame, interpolate both translation and rotation.
            for (u32 frame : frame_indices) {
                AnimKeyframe kf;

                // Convert frame index to local time within this cycle.
                float local_frame = static_cast<float>(frame - cycle.first_frame);
                kf.time = local_frame / fps;

                // Interpolate translation at this frame.
                if (!bk.translations.empty()) {
                    // Find the bracketing translation keyframes.
                    const auto& tkeys = bk.translations;
                    if (tkeys.size() == 1) {
                        kf.position[0] = tkeys[0].x;
                        kf.position[1] = tkeys[0].y;
                        kf.position[2] = tkeys[0].z;
                    } else {
                        // Find bracket.
                        std::size_t idx = 0;
                        for (std::size_t i = 0; i < tkeys.size() - 1; ++i) {
                            if (frame >= tkeys[i].frame &&
                                frame <= tkeys[i + 1].frame) {
                                idx = i;
                                break;
                            }
                            if (i == tkeys.size() - 2) idx = i;
                        }

                        // Clamp to ends.
                        if (frame <= tkeys.front().frame) {
                            kf.position[0] = tkeys.front().x;
                            kf.position[1] = tkeys.front().y;
                            kf.position[2] = tkeys.front().z;
                        } else if (frame >= tkeys.back().frame) {
                            kf.position[0] = tkeys.back().x;
                            kf.position[1] = tkeys.back().y;
                            kf.position[2] = tkeys.back().z;
                        } else {
                            float seg = static_cast<float>(
                                tkeys[idx + 1].frame - tkeys[idx].frame);
                            float t = (seg > 0.0f)
                                ? static_cast<float>(frame - tkeys[idx].frame) / seg
                                : 0.0f;
                            kf.position[0] = tkeys[idx].x + (tkeys[idx+1].x - tkeys[idx].x) * t;
                            kf.position[1] = tkeys[idx].y + (tkeys[idx+1].y - tkeys[idx].y) * t;
                            kf.position[2] = tkeys[idx].z + (tkeys[idx+1].z - tkeys[idx].z) * t;
                        }
                    }
                }

                // Interpolate rotation at this frame.
                if (!bk.rotations.empty()) {
                    const auto& rkeys = bk.rotations;
                    if (rkeys.size() == 1) {
                        kf.rotation[0] = rkeys[0].x;
                        kf.rotation[1] = rkeys[0].y;
                        kf.rotation[2] = rkeys[0].z;
                        kf.rotation[3] = rkeys[0].w;
                    } else {
                        // Find bracket.
                        if (frame <= rkeys.front().frame) {
                            kf.rotation[0] = rkeys.front().x;
                            kf.rotation[1] = rkeys.front().y;
                            kf.rotation[2] = rkeys.front().z;
                            kf.rotation[3] = rkeys.front().w;
                        } else if (frame >= rkeys.back().frame) {
                            kf.rotation[0] = rkeys.back().x;
                            kf.rotation[1] = rkeys.back().y;
                            kf.rotation[2] = rkeys.back().z;
                            kf.rotation[3] = rkeys.back().w;
                        } else {
                            std::size_t idx = 0;
                            for (std::size_t i = 0; i < rkeys.size() - 1; ++i) {
                                if (frame >= rkeys[i].frame &&
                                    frame <= rkeys[i + 1].frame) {
                                    idx = i;
                                    break;
                                }
                                if (i == rkeys.size() - 2) idx = i;
                            }
                            // Direct copy for exact frame matches.
                            // For interpolation, just use the nearest key
                            // (SLERP is done at runtime by AnimationSystem).
                            float seg = static_cast<float>(
                                rkeys[idx + 1].frame - rkeys[idx].frame);
                            float t = (seg > 0.0f)
                                ? static_cast<float>(frame - rkeys[idx].frame) / seg
                                : 0.0f;

                            // Simple nlerp for baking (runtime does proper slerp).
                            float ax = rkeys[idx].x, ay = rkeys[idx].y;
                            float az = rkeys[idx].z, aw = rkeys[idx].w;
                            float bx = rkeys[idx+1].x, by = rkeys[idx+1].y;
                            float bz = rkeys[idx+1].z, bw = rkeys[idx+1].w;

                            // Ensure shortest path.
                            float dot = ax*bx + ay*by + az*bz + aw*bw;
                            if (dot < 0.0f) {
                                bx = -bx; by = -by; bz = -bz; bw = -bw;
                            }

                            kf.rotation[0] = ax + (bx - ax) * t;
                            kf.rotation[1] = ay + (by - ay) * t;
                            kf.rotation[2] = az + (bz - az) * t;
                            kf.rotation[3] = aw + (bw - aw) * t;

                            // Normalize.
                            float len = std::sqrt(
                                kf.rotation[0]*kf.rotation[0] +
                                kf.rotation[1]*kf.rotation[1] +
                                kf.rotation[2]*kf.rotation[2] +
                                kf.rotation[3]*kf.rotation[3]);
                            if (len > 0.0f) {
                                float inv = 1.0f / len;
                                kf.rotation[0] *= inv;
                                kf.rotation[1] *= inv;
                                kf.rotation[2] *= inv;
                                kf.rotation[3] *= inv;
                            }
                        }
                    }
                }

                track.keyframes.push_back(kf);
            }

            if (!track.keyframes.empty()) {
                clip.tracks.push_back(std::move(track));
            }
        }

        LOG_INFO("AnimationLoader: built clip '%s' (%.3fs, %zu tracks, loop=%d)",
                 clip.name.c_str(), static_cast<double>(clip.duration),
                 clip.tracks.size(), clip.loop ? 1 : 0);

        clips.push_back(std::move(clip));
    }

    return clips;
}

// ===========================================================================
// Internal: resolve bone CRC to name
// ===========================================================================

std::string AnimationLoader::resolve_bone_name(u32 crc,
                                                const Skeleton* skeleton) {
    if (skeleton) {
        int idx = skeleton->find_by_crc(crc);
        if (idx >= 0) {
            return skeleton->bones[static_cast<std::size_t>(idx)].name;
        }
    }

    // Fallback: use CRC as name.
    char buf[32];
    std::snprintf(buf, sizeof(buf), "bone_0x%08X", crc);
    return buf;
}

} // namespace swbf
