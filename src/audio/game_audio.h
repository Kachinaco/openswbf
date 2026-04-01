#pragma once

#include "audio/audio_manager.h"

namespace swbf {

// Forward declarations.
class Camera;
class InputSystem;

/// Integrates the audio subsystem with gameplay events.
///
/// Owns no audio resources directly -- delegates to AudioManager for all
/// buffer/source management.  Call update() once per frame after input and
/// camera have been updated.
///
/// On Emscripten the OpenAL context is created lazily on first user
/// interaction to comply with browser autoplay policy.
class GameAudio {
public:
    GameAudio() = default;

    /// Bind the audio manager that will be used for all playback.
    /// Must be called before update().
    void set_audio_manager(AudioManager* mgr) { m_mgr = mgr; }

    /// Call once per frame.  Handles deferred init on Emscripten,
    /// updates the 3D listener from the camera, and drives
    /// periodic sounds (footsteps, ambient, music).
    void update(float dt, const Camera& camera, const InputSystem& input);

    // -- Game event triggers ------------------------------------------------

    /// A weapon was fired at the given world position.
    void on_weapon_fire(float x, float y, float z);

    /// A projectile impacted at the given world position.
    void on_impact(float x, float y, float z);

    /// An entity died / exploded at the given world position.
    void on_explosion(float x, float y, float z);

    /// A command post changed teams at the given world position.
    void on_command_post_captured(float x, float y, float z);

    /// A menu button was clicked.
    void on_menu_click();

    /// A menu item was selected / confirmed.
    void on_menu_select();

    /// Start battle music (looping).
    void start_battle_music();

    /// Stop music playback.
    void stop_music(float fade_seconds = 1.0f);

    /// Start ambient layer for the current map.
    void start_ambient(const std::string& name = "ambient_wind");

    /// Stop all ambient layers.
    void stop_ambient();

    /// AI or remote entity fired a weapon at a world position.
    void on_ai_weapon_fire(float x, float y, float z);

    /// A vehicle is at a world position -- play engine loop.
    void on_vehicle_engine(float x, float y, float z);

    /// Feed loaded .lvl sound data into the audio manager.
    /// Converts the Sound struct's PCM data into an AudioBuffer and
    /// registers it under the sound's name.
    void register_lvl_sound(const struct Sound& sound);

    /// Whether audio has been initialized (deferred init on Emscripten).
    bool is_audio_ready() const;

private:
    /// Attempt deferred audio init on first user interaction (Emscripten).
    void try_deferred_init(const InputSystem& input);

    /// Update the OpenAL listener from camera position and orientation.
    void update_listener(const Camera& camera);

    /// Drive periodic footstep sounds based on movement.
    void update_footsteps(float dt, const Camera& camera);

    /// Upload any queued sounds that were registered before audio was ready.
    void flush_pending_sounds();

    AudioManager* m_mgr = nullptr;

    // Deferred init state (Emscripten).
    bool m_audio_init_attempted = false;
    bool m_audio_ready          = false;

    // Sounds queued for upload when OpenAL isn't ready yet.
    struct PendingSound {
        std::string name;
        std::vector<int16_t> pcm;
        int sample_rate;
        int channels;
    };
    std::vector<PendingSound> m_pending_sounds;

    // Footstep timing.
    float m_footstep_timer    = 0.0f;
    float m_prev_cam_x        = 0.0f;
    float m_prev_cam_z        = 0.0f;
    bool  m_footstep_tracking = false;

    // Music state.
    bool m_battle_music_playing = false;

    // Ambient state.
    bool m_ambient_playing = false;
};

} // namespace swbf
