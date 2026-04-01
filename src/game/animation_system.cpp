#include "game/animation_system.h"
#include "core/log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace swbf {

// =========================================================================
// Standard soldier skeleton
// =========================================================================
//
// Index  Name           Parent
// -----  -------------- ------
//   0    bone_root        -1
//   1    bone_pelvis       0
//   2    bone_spine        1
//   3    bone_chest        2
//   4    bone_head         3
//   5    bone_l_arm        3
//   6    bone_r_arm        3
//   7    bone_l_leg        1
//   8    bone_r_leg        1

const char* AnimationSystem::bone_names[MAX_BONES] = {
    "bone_root",
    "bone_pelvis",
    "bone_spine",
    "bone_chest",
    "bone_head",
    "bone_l_arm",
    "bone_r_arm",
    "bone_l_leg",
    "bone_r_leg",
};

const int AnimationSystem::bone_parents[MAX_BONES] = {
    -1,  // bone_root
     0,  // bone_pelvis  -> bone_root
     1,  // bone_spine   -> bone_pelvis
     2,  // bone_chest   -> bone_spine
     3,  // bone_head    -> bone_chest
     3,  // bone_l_arm   -> bone_chest
     3,  // bone_r_arm   -> bone_chest
     1,  // bone_l_leg   -> bone_pelvis
     1,  // bone_r_leg   -> bone_pelvis
};

const std::vector<float> AnimationSystem::s_empty_matrices;

// =========================================================================
// Skeleton registry
// =========================================================================

int AnimationSystem::register_skeleton(const Skeleton& skeleton) {
    int idx = static_cast<int>(m_skeletons.size());
    m_skeletons.push_back(skeleton);
    LOG_INFO("AnimationSystem: registered skeleton '%s' (%zu bones, index %d)",
             skeleton.name.c_str(), skeleton.bones.size(), idx);
    return idx;
}

const Skeleton* AnimationSystem::get_skeleton(int index) const {
    if (index < 0 || index >= static_cast<int>(m_skeletons.size())) {
        return nullptr;
    }
    return &m_skeletons[static_cast<std::size_t>(index)];
}

void AnimationSystem::set_entity_skeleton(EntityId entity_id,
                                           int skeleton_index) {
    m_entity_skeletons[entity_id] = skeleton_index;
}

int AnimationSystem::get_entity_skeleton(EntityId entity_id) const {
    auto it = m_entity_skeletons.find(entity_id);
    return (it != m_entity_skeletons.end()) ? it->second : -1;
}

// =========================================================================
// Clip library
// =========================================================================

void AnimationSystem::register_clip(const AnimClip& clip) {
    LOG_DEBUG("AnimationSystem: registered clip '%s' (%.2fs, %zu tracks, loop=%d)",
              clip.name.c_str(), static_cast<double>(clip.duration),
              clip.tracks.size(), clip.loop ? 1 : 0);
    m_clips[clip.name] = clip;
}

const AnimClip* AnimationSystem::find_clip(const std::string& name) const {
    auto it = m_clips.find(name);
    return (it != m_clips.end()) ? &it->second : nullptr;
}

// =========================================================================
// Playback control
// =========================================================================

void AnimationSystem::play(EntityId entity_id, const std::string& clip_name,
                           bool loop, float blend_time) {
    const AnimClip* clip = find_clip(clip_name);
    if (!clip) {
        LOG_WARN("AnimationSystem: clip '%s' not found", clip_name.c_str());
        return;
    }

    auto it = m_states.find(entity_id);
    if (it != m_states.end()) {
        AnimState& state = it->second;

        // Already playing this clip -- don't restart.
        if (state.current_clip == clip_name) return;

        // Set up cross-fade from previous clip.
        if (blend_time > 0.0f && !state.current_clip.empty()) {
            state.blending_from   = state.current_clip;
            state.blend_from_time = state.time;
            state.blend_factor    = 0.0f;
            state.blend_time      = blend_time;
        } else {
            state.blending_from.clear();
            state.blend_factor = 1.0f;
            state.blend_time   = 0.0f;
        }

        state.current_clip = clip_name;
        state.time         = 0.0f;
    } else {
        // First animation for this entity -- no blend.
        AnimState state;
        state.current_clip = clip_name;
        state.time         = 0.0f;
        state.speed        = 1.0f;
        state.blend_factor = 1.0f;
        m_states[entity_id] = state;
    }

    // Override the clip's loop flag with the caller's preference.
    m_clips[clip_name].loop = loop;

    LOG_DEBUG("AnimationSystem: play '%s' on entity %u (blend=%.2f)",
              clip_name.c_str(), entity_id, static_cast<double>(blend_time));
}

void AnimationSystem::stop(EntityId entity_id) {
    m_states.erase(entity_id);
    m_bone_matrices.erase(entity_id);
    m_entity_skeletons.erase(entity_id);
}

void AnimationSystem::set_speed(EntityId entity_id, float speed) {
    auto it = m_states.find(entity_id);
    if (it != m_states.end()) {
        it->second.speed = speed;
    }
}

// =========================================================================
// Per-frame update
// =========================================================================

void AnimationSystem::update(float dt) {
    for (auto& [entity_id, state] : m_states) {
        const AnimClip* clip = find_clip(state.current_clip);
        if (!clip) continue;

        // Advance time.
        state.time += dt * state.speed;

        // Handle looping / end.
        if (clip->loop) {
            if (clip->duration > 0.0f) {
                state.time = std::fmod(state.time, clip->duration);
                if (state.time < 0.0f) state.time += clip->duration;
            }
        } else {
            if (state.time > clip->duration) {
                state.time = clip->duration;
            }
        }

        // Advance blend.
        if (state.blend_factor < 1.0f && state.blend_time > 0.0f) {
            state.blend_factor += dt / state.blend_time;
            if (state.blend_factor >= 1.0f) {
                state.blend_factor = 1.0f;
                state.blending_from.clear();
            }
        }

        // Choose update path: dynamic skeleton or standard.
        int skel_idx = get_entity_skeleton(entity_id);
        const Skeleton* skel = get_skeleton(skel_idx);
        if (skel && !skel->bones.empty()) {
            update_with_skeleton(entity_id, state, *skel);
        } else {
            update_standard(entity_id, state);
        }
    }
}

// =========================================================================
// Standard skeleton update (hardcoded 9-bone)
// =========================================================================

void AnimationSystem::update_standard(EntityId entity_id, AnimState& state) {
    const AnimClip* clip = find_clip(state.current_clip);
    if (!clip) return;

    // Local transforms: position + quaternion per bone.
    float local_pos[MAX_BONES][3];
    float local_rot[MAX_BONES][4];

    // Start with identity for every bone.
    for (int i = 0; i < MAX_BONES; ++i) {
        local_pos[i][0] = 0.0f;
        local_pos[i][1] = 0.0f;
        local_pos[i][2] = 0.0f;
        local_rot[i][0] = 0.0f;
        local_rot[i][1] = 0.0f;
        local_rot[i][2] = 0.0f;
        local_rot[i][3] = 1.0f;
    }

    // Sample the current clip for every track that maps to a known bone.
    for (const auto& track : clip->tracks) {
        int bone_idx = find_bone_index(track.bone_name);
        if (bone_idx < 0) continue;
        sample_track(track, state.time, clip->loop, clip->duration,
                     local_pos[bone_idx], local_rot[bone_idx]);
    }

    // If blending, sample the outgoing clip and mix.
    if (state.blend_factor < 1.0f && !state.blending_from.empty()) {
        const AnimClip* from_clip = find_clip(state.blending_from);
        if (from_clip) {
            float from_pos[MAX_BONES][3];
            float from_rot[MAX_BONES][4];

            for (int i = 0; i < MAX_BONES; ++i) {
                from_pos[i][0] = 0.0f;
                from_pos[i][1] = 0.0f;
                from_pos[i][2] = 0.0f;
                from_rot[i][0] = 0.0f;
                from_rot[i][1] = 0.0f;
                from_rot[i][2] = 0.0f;
                from_rot[i][3] = 1.0f;
            }

            for (const auto& track : from_clip->tracks) {
                int bone_idx = find_bone_index(track.bone_name);
                if (bone_idx < 0) continue;
                sample_track(track, state.blend_from_time,
                             from_clip->loop, from_clip->duration,
                             from_pos[bone_idx], from_rot[bone_idx]);
            }

            // Mix: lerp positions, slerp rotations.
            float t = state.blend_factor;
            for (int i = 0; i < MAX_BONES; ++i) {
                float blended_pos[3];
                float blended_rot[4];
                lerp3(from_pos[i], local_pos[i], t, blended_pos);
                slerp(from_rot[i], local_rot[i], t, blended_rot);
                local_pos[i][0] = blended_pos[0];
                local_pos[i][1] = blended_pos[1];
                local_pos[i][2] = blended_pos[2];
                local_rot[i][0] = blended_rot[0];
                local_rot[i][1] = blended_rot[1];
                local_rot[i][2] = blended_rot[2];
                local_rot[i][3] = blended_rot[3];
            }
        }
    }

    // Compute world bone matrices (local * parent, recursive).
    float bone_world[MAX_BONES][16];
    for (int i = 0; i < MAX_BONES; ++i) {
        float local_mat[16];
        compose_matrix(local_pos[i], local_rot[i], local_mat);

        int parent = bone_parents[i];
        if (parent < 0) {
            std::memcpy(bone_world[i], local_mat, sizeof(float) * 16);
        } else {
            mul_mat4(bone_world[parent], local_mat, bone_world[i]);
        }
    }

    // Write into cached output.
    auto& out = m_bone_matrices[entity_id];
    out.resize(MAX_BONES * 16);
    std::memcpy(out.data(), bone_world, sizeof(float) * MAX_BONES * 16);
}

// =========================================================================
// Dynamic skeleton update
// =========================================================================

void AnimationSystem::update_with_skeleton(EntityId entity_id,
                                            AnimState& state,
                                            const Skeleton& skeleton) {
    const AnimClip* clip = find_clip(state.current_clip);
    if (!clip) return;

    const int bone_count = static_cast<int>(skeleton.bones.size());
    const int clamped_count = std::min(bone_count, MAX_SKELETON_BONES);

    // Allocate local transforms for all bones.
    float local_pos[MAX_SKELETON_BONES][3];
    float local_rot[MAX_SKELETON_BONES][4];

    // Initialize with bind pose (identity for animations -- the animation
    // keyframes define the full local transform, not a delta from bind pose).
    for (int i = 0; i < clamped_count; ++i) {
        local_pos[i][0] = 0.0f;
        local_pos[i][1] = 0.0f;
        local_pos[i][2] = 0.0f;
        local_rot[i][0] = 0.0f;
        local_rot[i][1] = 0.0f;
        local_rot[i][2] = 0.0f;
        local_rot[i][3] = 1.0f;
    }

    // Sample the current clip.  Match tracks to bones by name.
    for (const auto& track : clip->tracks) {
        int bone_idx = skeleton.find_by_name(track.bone_name);
        if (bone_idx < 0 || bone_idx >= clamped_count) continue;
        sample_track(track, state.time, clip->loop, clip->duration,
                     local_pos[bone_idx], local_rot[bone_idx]);
    }

    // If blending, sample the outgoing clip and mix.
    if (state.blend_factor < 1.0f && !state.blending_from.empty()) {
        const AnimClip* from_clip = find_clip(state.blending_from);
        if (from_clip) {
            float from_pos[MAX_SKELETON_BONES][3];
            float from_rot[MAX_SKELETON_BONES][4];

            for (int i = 0; i < clamped_count; ++i) {
                from_pos[i][0] = 0.0f;
                from_pos[i][1] = 0.0f;
                from_pos[i][2] = 0.0f;
                from_rot[i][0] = 0.0f;
                from_rot[i][1] = 0.0f;
                from_rot[i][2] = 0.0f;
                from_rot[i][3] = 1.0f;
            }

            for (const auto& track : from_clip->tracks) {
                int bone_idx = skeleton.find_by_name(track.bone_name);
                if (bone_idx < 0 || bone_idx >= clamped_count) continue;
                sample_track(track, state.blend_from_time,
                             from_clip->loop, from_clip->duration,
                             from_pos[bone_idx], from_rot[bone_idx]);
            }

            float t = state.blend_factor;
            for (int i = 0; i < clamped_count; ++i) {
                float blended_pos[3];
                float blended_rot[4];
                lerp3(from_pos[i], local_pos[i], t, blended_pos);
                slerp(from_rot[i], local_rot[i], t, blended_rot);
                local_pos[i][0] = blended_pos[0];
                local_pos[i][1] = blended_pos[1];
                local_pos[i][2] = blended_pos[2];
                local_rot[i][0] = blended_rot[0];
                local_rot[i][1] = blended_rot[1];
                local_rot[i][2] = blended_rot[2];
                local_rot[i][3] = blended_rot[3];
            }
        }
    }

    // Compute world bone matrices using the skeleton hierarchy.
    float bone_world[MAX_SKELETON_BONES][16];
    for (int i = 0; i < clamped_count; ++i) {
        float local_mat[16];
        compose_matrix(local_pos[i], local_rot[i], local_mat);

        int parent = skeleton.bones[static_cast<std::size_t>(i)].parent;
        if (parent < 0 || parent >= clamped_count) {
            std::memcpy(bone_world[i], local_mat, sizeof(float) * 16);
        } else {
            mul_mat4(bone_world[parent], local_mat, bone_world[i]);
        }
    }

    // Compute final skinning matrices: bone_world[i] * inv_bind[i]
    // This transforms vertices from bind-pose model space to current
    // animated world space.
    auto& out = m_bone_matrices[entity_id];
    out.resize(static_cast<std::size_t>(clamped_count) * 16);

    for (int i = 0; i < clamped_count; ++i) {
        float skinning_mat[16];
        mul_mat4(bone_world[i],
                 skeleton.bones[static_cast<std::size_t>(i)].inv_bind_matrix,
                 skinning_mat);
        std::memcpy(&out[static_cast<std::size_t>(i) * 16],
                    skinning_mat, sizeof(float) * 16);
    }
}

// =========================================================================
// Queries
// =========================================================================

const std::vector<float>& AnimationSystem::get_bone_transforms(
        EntityId entity_id) const {
    auto it = m_bone_matrices.find(entity_id);
    if (it != m_bone_matrices.end()) return it->second;
    return s_empty_matrices;
}

const AnimState* AnimationSystem::get_state(EntityId entity_id) const {
    auto it = m_states.find(entity_id);
    return (it != m_states.end()) ? &it->second : nullptr;
}

// =========================================================================
// Keyframe sampling
// =========================================================================

void AnimationSystem::sample_track(const AnimTrack& track, float time,
                                   bool loop, float duration,
                                   float out_pos[3], float out_rot[4]) {
    const auto& keys = track.keyframes;
    if (keys.empty()) {
        out_pos[0] = out_pos[1] = out_pos[2] = 0.0f;
        out_rot[0] = out_rot[1] = out_rot[2] = 0.0f;
        out_rot[3] = 1.0f;
        return;
    }

    // Clamp / wrap time.
    if (loop && duration > 0.0f) {
        time = std::fmod(time, duration);
        if (time < 0.0f) time += duration;
    } else {
        if (time < keys.front().time) time = keys.front().time;
        if (time > keys.back().time)  time = keys.back().time;
    }

    // Single keyframe -- return it directly.
    if (keys.size() == 1) {
        out_pos[0] = keys[0].position[0];
        out_pos[1] = keys[0].position[1];
        out_pos[2] = keys[0].position[2];
        out_rot[0] = keys[0].rotation[0];
        out_rot[1] = keys[0].rotation[1];
        out_rot[2] = keys[0].rotation[2];
        out_rot[3] = keys[0].rotation[3];
        return;
    }

    // Find the two keyframes bracketing `time`.
    std::size_t idx = 0;
    for (std::size_t i = 0; i < keys.size() - 1; ++i) {
        if (time >= keys[i].time && time <= keys[i + 1].time) {
            idx = i;
            break;
        }
        if (i == keys.size() - 2) {
            idx = i;
        }
    }

    const AnimKeyframe& a = keys[idx];
    const AnimKeyframe& b = keys[idx + 1];

    float segment = b.time - a.time;
    float t = (segment > 0.0f) ? (time - a.time) / segment : 0.0f;
    t = std::max(0.0f, std::min(1.0f, t));

    lerp3(a.position, b.position, t, out_pos);
    slerp(a.rotation, b.rotation, t, out_rot);
}

// =========================================================================
// Math helpers
// =========================================================================

void AnimationSystem::lerp3(const float a[3], const float b[3], float t,
                            float out[3]) {
    out[0] = a[0] + (b[0] - a[0]) * t;
    out[1] = a[1] + (b[1] - a[1]) * t;
    out[2] = a[2] + (b[2] - a[2]) * t;
}

void AnimationSystem::slerp(const float a[4], const float b[4], float t,
                            float out[4]) {
    float dot = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];

    float nb[4] = {b[0], b[1], b[2], b[3]};
    if (dot < 0.0f) {
        dot = -dot;
        nb[0] = -nb[0];
        nb[1] = -nb[1];
        nb[2] = -nb[2];
        nb[3] = -nb[3];
    }

    constexpr float THRESHOLD = 0.9995f;
    if (dot > THRESHOLD) {
        out[0] = a[0] + (nb[0] - a[0]) * t;
        out[1] = a[1] + (nb[1] - a[1]) * t;
        out[2] = a[2] + (nb[2] - a[2]) * t;
        out[3] = a[3] + (nb[3] - a[3]) * t;

        float len = std::sqrt(out[0]*out[0] + out[1]*out[1] +
                              out[2]*out[2] + out[3]*out[3]);
        if (len > 0.0f) {
            float inv = 1.0f / len;
            out[0] *= inv;
            out[1] *= inv;
            out[2] *= inv;
            out[3] *= inv;
        }
        return;
    }

    float theta   = std::acos(dot);
    float sin_th  = std::sin(theta);
    float factor_a = std::sin((1.0f - t) * theta) / sin_th;
    float factor_b = std::sin(t * theta) / sin_th;

    out[0] = a[0] * factor_a + nb[0] * factor_b;
    out[1] = a[1] * factor_a + nb[1] * factor_b;
    out[2] = a[2] * factor_a + nb[2] * factor_b;
    out[3] = a[3] * factor_a + nb[3] * factor_b;
}

void AnimationSystem::compose_matrix(const float pos[3], const float rot[4],
                                     float m[16]) {
    const float qx = rot[0];
    const float qy = rot[1];
    const float qz = rot[2];
    const float qw = rot[3];

    const float xx = qx * qx;
    const float yy = qy * qy;
    const float zz = qz * qz;
    const float xy = qx * qy;
    const float xz = qx * qz;
    const float yz = qy * qz;
    const float wx = qw * qx;
    const float wy = qw * qy;
    const float wz = qw * qz;

    m[0]  = 1.0f - 2.0f * (yy + zz);
    m[1]  =        2.0f * (xy + wz);
    m[2]  =        2.0f * (xz - wy);
    m[3]  = 0.0f;

    m[4]  =        2.0f * (xy - wz);
    m[5]  = 1.0f - 2.0f * (xx + zz);
    m[6]  =        2.0f * (yz + wx);
    m[7]  = 0.0f;

    m[8]  =        2.0f * (xz + wy);
    m[9]  =        2.0f * (yz - wx);
    m[10] = 1.0f - 2.0f * (xx + yy);
    m[11] = 0.0f;

    m[12] = pos[0];
    m[13] = pos[1];
    m[14] = pos[2];
    m[15] = 1.0f;
}

void AnimationSystem::mul_mat4(const float a[16], const float b[16],
                               float out[16]) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            out[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
}

int AnimationSystem::find_bone_index(const std::string& name) {
    for (int i = 0; i < MAX_BONES; ++i) {
        if (name == bone_names[i]) return i;
    }
    return -1;
}

} // namespace swbf
