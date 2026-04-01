#pragma once

#include <string>
#include <vector>

namespace swbf {

// =========================================================================
// Animation data types.
// =========================================================================

/// A single keyframe in an animation track.
///
/// Stores a timestamp, a position (translation), and a rotation quaternion
/// in (x, y, z, w) order -- the same convention as Transform.
struct AnimKeyframe {
    float time        = 0.0f;
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f}; // quaternion xyzw
};

/// One track per bone.  Each track contains keyframes sorted by time.
struct AnimTrack {
    std::string                bone_name;
    std::vector<AnimKeyframe>  keyframes;
};

/// A complete animation clip (e.g. "walk", "idle", "fire").
///
/// Clips are registered with the AnimationSystem at startup and referenced
/// by name when playing.  In production these come from ANM2/CYCL/KFR3
/// chunks inside .lvl files, parsed by AnimationLoader.  Procedural clips
/// can also be created for testing (see anim_idle(), anim_walk(), etc.).
struct AnimClip {
    std::string            name;
    float                  duration = 1.0f;     // seconds
    std::vector<AnimTrack> tracks;
    bool                   loop     = false;
};

/// Per-entity animation playback state.
///
/// Tracks the current clip, local time, playback speed, and cross-fade
/// blending from a previous clip.
struct AnimState {
    std::string current_clip;       // name of the playing clip ("" = none)
    float       time         = 0.0f;
    float       speed        = 1.0f;

    // Cross-fade blending.
    std::string blending_from;      // previous clip being blended out ("" = none)
    float       blend_factor = 1.0f; // 0 = fully previous, 1 = fully current
    float       blend_time   = 0.0f; // total blend duration (seconds)
    float       blend_from_time = 0.0f; // frozen time in the outgoing clip
};

// =========================================================================
// Placeholder animation clips -- these would normally come from .lvl files.
// =========================================================================

/// Standard SWBF soldier bone names used by the placeholder clips.
/// A real skeleton has ~40 bones; we model a minimal set sufficient for
/// the basic motions (breathing, walking, recoil).
///
///   bone_root
///   |-- bone_pelvis
///   |   |-- bone_spine
///   |   |   |-- bone_chest
///   |   |   |   |-- bone_head
///   |   |   |   |-- bone_l_arm
///   |   |   |   +-- bone_r_arm
///   |   |-- bone_l_leg
///   |   +-- bone_r_leg
///

/// Idle -- standing still with a subtle breathing motion on the chest.
inline AnimClip anim_idle() {
    AnimClip clip;
    clip.name     = "idle";
    clip.duration = 2.0f;
    clip.loop     = true;

    // Root -- stationary.
    {
        AnimTrack t;
        t.bone_name = "bone_root";
        t.keyframes.push_back({0.0f, {0,0,0}, {0,0,0,1}});
        t.keyframes.push_back({2.0f, {0,0,0}, {0,0,0,1}});
        clip.tracks.push_back(std::move(t));
    }
    // Chest -- breathe up/down.
    {
        AnimTrack t;
        t.bone_name = "bone_chest";
        t.keyframes.push_back({0.0f,  {0, 0.00f, 0}, {0,0,0,1}});
        t.keyframes.push_back({1.0f,  {0, 0.02f, 0}, {0,0,0,1}});
        t.keyframes.push_back({2.0f,  {0, 0.00f, 0}, {0,0,0,1}});
        clip.tracks.push_back(std::move(t));
    }
    return clip;
}

/// Walk cycle -- 1.0 s loop, legs alternate forward/back.
inline AnimClip anim_walk() {
    AnimClip clip;
    clip.name     = "walk";
    clip.duration = 1.0f;
    clip.loop     = true;

    // Pelvis -- slight vertical bob.
    {
        AnimTrack t;
        t.bone_name = "bone_pelvis";
        t.keyframes.push_back({0.00f, {0, 0.00f, 0}, {0,0,0,1}});
        t.keyframes.push_back({0.25f, {0, 0.03f, 0}, {0,0,0,1}});
        t.keyframes.push_back({0.50f, {0, 0.00f, 0}, {0,0,0,1}});
        t.keyframes.push_back({0.75f, {0, 0.03f, 0}, {0,0,0,1}});
        t.keyframes.push_back({1.00f, {0, 0.00f, 0}, {0,0,0,1}});
        clip.tracks.push_back(std::move(t));
    }
    // Left leg -- forward on first half, back on second half.
    {
        AnimTrack t;
        t.bone_name = "bone_l_leg";
        t.keyframes.push_back({0.00f, {0, 0, 0},      {0.15f, 0, 0, 0.989f}});
        t.keyframes.push_back({0.50f, {0, 0, 0},      {-0.15f, 0, 0, 0.989f}});
        t.keyframes.push_back({1.00f, {0, 0, 0},      {0.15f, 0, 0, 0.989f}});
        clip.tracks.push_back(std::move(t));
    }
    // Right leg -- opposite phase.
    {
        AnimTrack t;
        t.bone_name = "bone_r_leg";
        t.keyframes.push_back({0.00f, {0, 0, 0},      {-0.15f, 0, 0, 0.989f}});
        t.keyframes.push_back({0.50f, {0, 0, 0},      {0.15f, 0, 0, 0.989f}});
        t.keyframes.push_back({1.00f, {0, 0, 0},      {-0.15f, 0, 0, 0.989f}});
        clip.tracks.push_back(std::move(t));
    }
    // Arms -- gentle counter-swing.
    {
        AnimTrack t;
        t.bone_name = "bone_l_arm";
        t.keyframes.push_back({0.00f, {0, 0, 0}, {-0.10f, 0, 0, 0.995f}});
        t.keyframes.push_back({0.50f, {0, 0, 0}, { 0.10f, 0, 0, 0.995f}});
        t.keyframes.push_back({1.00f, {0, 0, 0}, {-0.10f, 0, 0, 0.995f}});
        clip.tracks.push_back(std::move(t));
    }
    {
        AnimTrack t;
        t.bone_name = "bone_r_arm";
        t.keyframes.push_back({0.00f, {0, 0, 0}, { 0.10f, 0, 0, 0.995f}});
        t.keyframes.push_back({0.50f, {0, 0, 0}, {-0.10f, 0, 0, 0.995f}});
        t.keyframes.push_back({1.00f, {0, 0, 0}, { 0.10f, 0, 0, 0.995f}});
        clip.tracks.push_back(std::move(t));
    }
    return clip;
}

/// Run cycle -- 0.6 s loop, larger leg swing and faster bob.
inline AnimClip anim_run() {
    AnimClip clip;
    clip.name     = "run";
    clip.duration = 0.6f;
    clip.loop     = true;

    // Pelvis -- bigger bob.
    {
        AnimTrack t;
        t.bone_name = "bone_pelvis";
        t.keyframes.push_back({0.00f, {0, 0.00f, 0}, {0,0,0,1}});
        t.keyframes.push_back({0.15f, {0, 0.05f, 0}, {0,0,0,1}});
        t.keyframes.push_back({0.30f, {0, 0.00f, 0}, {0,0,0,1}});
        t.keyframes.push_back({0.45f, {0, 0.05f, 0}, {0,0,0,1}});
        t.keyframes.push_back({0.60f, {0, 0.00f, 0}, {0,0,0,1}});
        clip.tracks.push_back(std::move(t));
    }
    // Left leg -- wider swing.
    {
        AnimTrack t;
        t.bone_name = "bone_l_leg";
        t.keyframes.push_back({0.00f, {0, 0, 0}, { 0.25f, 0, 0, 0.968f}});
        t.keyframes.push_back({0.30f, {0, 0, 0}, {-0.25f, 0, 0, 0.968f}});
        t.keyframes.push_back({0.60f, {0, 0, 0}, { 0.25f, 0, 0, 0.968f}});
        clip.tracks.push_back(std::move(t));
    }
    // Right leg -- opposite phase.
    {
        AnimTrack t;
        t.bone_name = "bone_r_leg";
        t.keyframes.push_back({0.00f, {0, 0, 0}, {-0.25f, 0, 0, 0.968f}});
        t.keyframes.push_back({0.30f, {0, 0, 0}, { 0.25f, 0, 0, 0.968f}});
        t.keyframes.push_back({0.60f, {0, 0, 0}, {-0.25f, 0, 0, 0.968f}});
        clip.tracks.push_back(std::move(t));
    }
    // Arms -- bigger counter-swing.
    {
        AnimTrack t;
        t.bone_name = "bone_l_arm";
        t.keyframes.push_back({0.00f, {0, 0, 0}, {-0.20f, 0, 0, 0.980f}});
        t.keyframes.push_back({0.30f, {0, 0, 0}, { 0.20f, 0, 0, 0.980f}});
        t.keyframes.push_back({0.60f, {0, 0, 0}, {-0.20f, 0, 0, 0.980f}});
        clip.tracks.push_back(std::move(t));
    }
    {
        AnimTrack t;
        t.bone_name = "bone_r_arm";
        t.keyframes.push_back({0.00f, {0, 0, 0}, { 0.20f, 0, 0, 0.980f}});
        t.keyframes.push_back({0.30f, {0, 0, 0}, {-0.20f, 0, 0, 0.980f}});
        t.keyframes.push_back({0.60f, {0, 0, 0}, { 0.20f, 0, 0, 0.980f}});
        clip.tracks.push_back(std::move(t));
    }
    return clip;
}

/// Death -- fall down over 0.8 seconds, non-looping.
inline AnimClip anim_death() {
    AnimClip clip;
    clip.name     = "death";
    clip.duration = 0.8f;
    clip.loop     = false;

    // Root -- drop to ground.
    {
        AnimTrack t;
        t.bone_name = "bone_root";
        t.keyframes.push_back({0.0f, {0, 0.0f,  0}, {0, 0, 0, 1}});
        t.keyframes.push_back({0.4f, {0, -0.3f, 0}, {0, 0, 0, 1}});
        t.keyframes.push_back({0.8f, {0, -1.0f, 0}, {0, 0, 0, 1}});
        clip.tracks.push_back(std::move(t));
    }
    // Spine -- tilt backward as the character falls.
    {
        AnimTrack t;
        t.bone_name = "bone_spine";
        t.keyframes.push_back({0.0f, {0,0,0}, {0.0f,  0, 0, 1.0f}});
        t.keyframes.push_back({0.5f, {0,0,0}, {0.27f, 0, 0, 0.963f}});
        t.keyframes.push_back({0.8f, {0,0,0}, {0.38f, 0, 0, 0.924f}});
        clip.tracks.push_back(std::move(t));
    }
    // Head -- slump forward at the end.
    {
        AnimTrack t;
        t.bone_name = "bone_head";
        t.keyframes.push_back({0.0f, {0,0,0}, {0.0f,  0, 0, 1.0f}});
        t.keyframes.push_back({0.6f, {0,0,0}, {0.0f,  0, 0, 1.0f}});
        t.keyframes.push_back({0.8f, {0,0,0}, {0.20f, 0, 0, 0.980f}});
        clip.tracks.push_back(std::move(t));
    }
    return clip;
}

/// Fire -- upper body recoil, 0.3 seconds, non-looping.
inline AnimClip anim_fire() {
    AnimClip clip;
    clip.name     = "fire";
    clip.duration = 0.3f;
    clip.loop     = false;

    // Chest -- kick back then return.
    {
        AnimTrack t;
        t.bone_name = "bone_chest";
        t.keyframes.push_back({0.00f, {0, 0, 0},       {0, 0, 0, 1}});
        t.keyframes.push_back({0.05f, {0, 0, -0.04f},  {-0.05f, 0, 0, 0.999f}});
        t.keyframes.push_back({0.30f, {0, 0, 0},       {0, 0, 0, 1}});
        clip.tracks.push_back(std::move(t));
    }
    // Right arm -- recoil upward.
    {
        AnimTrack t;
        t.bone_name = "bone_r_arm";
        t.keyframes.push_back({0.00f, {0, 0, 0}, {0, 0, 0, 1}});
        t.keyframes.push_back({0.05f, {0, 0, 0}, {-0.08f, 0, 0, 0.997f}});
        t.keyframes.push_back({0.30f, {0, 0, 0}, {0, 0, 0, 1}});
        clip.tracks.push_back(std::move(t));
    }
    // Head -- slight upward jolt.
    {
        AnimTrack t;
        t.bone_name = "bone_head";
        t.keyframes.push_back({0.00f, {0, 0, 0}, {0, 0, 0, 1}});
        t.keyframes.push_back({0.05f, {0, 0, 0}, {-0.03f, 0, 0, 0.9996f}});
        t.keyframes.push_back({0.30f, {0, 0, 0}, {0, 0, 0, 1}});
        clip.tracks.push_back(std::move(t));
    }
    return clip;
}

} // namespace swbf
