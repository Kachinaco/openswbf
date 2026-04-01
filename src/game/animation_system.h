#pragma once

#include "game/animation.h"
#include "game/entity.h"
#include "game/skeleton.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace swbf {

/// Drives skeletal animation for entities.
///
/// The AnimationSystem owns a library of AnimClips (registered at startup) and
/// maintains per-entity AnimState instances.  Each frame the caller invokes
/// update(dt), which advances all active animations, evaluates keyframe
/// interpolation, handles cross-fade blending, and produces per-bone 4x4
/// matrices ready for GPU skinning.
///
/// Supports two modes:
///   1. Standard skeleton -- hardcoded 9-bone hierarchy for basic motions.
///   2. Dynamic skeleton -- loaded from .lvl files via AnimationLoader,
///      supports arbitrary bone hierarchies with CRC32 name matching and
///      inverse bind pose matrices for proper skinning.
///
/// Usage:
///   AnimationSystem anims;
///   anims.register_clip(anim_idle());
///
///   // For .lvl-loaded skeletons:
///   int skel_idx = anims.register_skeleton(loaded_skeleton);
///   anims.set_entity_skeleton(soldier_id, skel_idx);
///
///   anims.play(soldier_id, "idle", true);
///
///   // each frame:
///   anims.update(dt);
///   auto& bones = anims.get_bone_transforms(soldier_id);
///   // upload bones to shader...
class AnimationSystem {
public:
    AnimationSystem() = default;

    // -- Skeleton registry ---------------------------------------------------

    /// Register a skeleton definition.  Returns the skeleton index.
    /// Skeletons are shared -- multiple entities can reference the same one.
    int register_skeleton(const Skeleton& skeleton);

    /// Get a registered skeleton by index (nullptr if invalid).
    const Skeleton* get_skeleton(int index) const;

    /// Total number of registered skeletons.
    std::size_t skeleton_count() const { return m_skeletons.size(); }

    /// Assign a skeleton to an entity (by skeleton registry index).
    /// Entities with a skeleton use it for bone lookup and matrix computation
    /// instead of the standard hardcoded skeleton.
    void set_entity_skeleton(EntityId entity_id, int skeleton_index);

    /// Get the skeleton index for an entity (-1 if using standard skeleton).
    int get_entity_skeleton(EntityId entity_id) const;

    // -- Clip library --------------------------------------------------------

    /// Register a clip.  The clip is copied into the system and referenced
    /// by its name field.  Registering a clip with a duplicate name
    /// overwrites the previous one.
    void register_clip(const AnimClip& clip);

    /// Look up a clip by name (nullptr if not found).
    const AnimClip* find_clip(const std::string& name) const;

    // -- Playback control ----------------------------------------------------

    /// Start playing a clip on an entity.
    /// @p entity_id   Target entity.
    /// @p clip_name   Name of a previously-registered AnimClip.
    /// @p loop        Whether the clip loops when it reaches the end.
    /// @p blend_time  Cross-fade duration from the current clip (seconds).
    ///                0 = instant switch.
    void play(EntityId entity_id, const std::string& clip_name,
              bool loop, float blend_time = 0.2f);

    /// Stop all animation on an entity and remove its state.
    void stop(EntityId entity_id);

    /// Set playback speed for an entity (1.0 = normal, 0 = paused).
    void set_speed(EntityId entity_id, float speed);

    // -- Per-frame update ----------------------------------------------------

    /// Advance all active animations by @p dt seconds.
    void update(float dt);

    // -- Queries -------------------------------------------------------------

    /// Get the final bone transforms for an entity.
    /// Returns a vector of column-major 4x4 matrices, one per bone.
    ///
    /// When a dynamic skeleton is assigned, the returned matrices are:
    ///   bone_world[i] * inv_bind_matrix[i]
    /// This transforms vertices from bind pose -> animated world space,
    /// ready for GPU skinning.
    ///
    /// When using the standard skeleton, the matrices are the world-space
    /// bone transforms (no inverse bind pose applied).
    const std::vector<float>& get_bone_transforms(EntityId entity_id) const;

    /// Get the animation state for an entity (nullptr if none).
    const AnimState* get_state(EntityId entity_id) const;

    /// Total number of entities with active animations.
    std::size_t active_count() const { return m_states.size(); }

    /// Standard skeleton constants.
    static constexpr int MAX_BONES = 9;

    /// Names of the bones in the standard skeleton, in hierarchy order.
    static const char* bone_names[MAX_BONES];

    /// Parent index for each bone (-1 = root).
    static const int   bone_parents[MAX_BONES];

    // -- Math helpers (public for testing) -----------------------------------

    /// Evaluate a single AnimTrack at the given local time and write the
    /// resulting position and rotation into @p out_pos / @p out_rot.
    static void sample_track(const AnimTrack& track, float time, bool loop,
                             float duration,
                             float out_pos[3], float out_rot[4]);

    /// Linearly interpolate two 3-component vectors.
    static void lerp3(const float a[3], const float b[3], float t,
                      float out[3]);

    /// Spherical linear interpolation for unit quaternions (xyzw).
    static void slerp(const float a[4], const float b[4], float t,
                      float out[4]);

    /// Build a column-major 4x4 matrix from position and quaternion rotation.
    static void compose_matrix(const float pos[3], const float rot[4],
                               float out_mat4[16]);

    /// Multiply two column-major 4x4 matrices: out = A * B.
    static void mul_mat4(const float a[16], const float b[16],
                         float out[16]);

private:
    // -- Internal helpers ----------------------------------------------------

    /// Find the index of a bone by name, or -1 if not in the standard skeleton.
    static int find_bone_index(const std::string& name);

    /// Update an entity using its assigned dynamic skeleton.
    void update_with_skeleton(EntityId entity_id, AnimState& state,
                              const Skeleton& skeleton);

    /// Update an entity using the standard hardcoded skeleton.
    void update_standard(EntityId entity_id, AnimState& state);

    // -- State ---------------------------------------------------------------

    /// Registered clips, keyed by name.
    std::unordered_map<std::string, AnimClip> m_clips;

    /// Per-entity animation state.
    std::unordered_map<EntityId, AnimState> m_states;

    /// Per-entity computed bone matrices.
    std::unordered_map<EntityId, std::vector<float>> m_bone_matrices;

    /// Per-entity skeleton assignment (-1 = use standard skeleton).
    std::unordered_map<EntityId, int> m_entity_skeletons;

    /// Registered skeletons.
    std::vector<Skeleton> m_skeletons;

    /// Returned when an entity has no animation data.
    static const std::vector<float> s_empty_matrices;
};

} // namespace swbf
