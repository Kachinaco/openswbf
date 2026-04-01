#include "audio/game_audio.h"
#include "assets/lvl/sound_loader.h"
#include "input/input_system.h"
#include "renderer/camera.h"
#include "core/log.h"

#include <cmath>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace swbf {

// How fast the player must be moving (camera delta per frame) to trigger
// footstep sounds, and the interval between steps.
static constexpr float FOOTSTEP_SPEED_THRESHOLD = 0.5f;
static constexpr float FOOTSTEP_INTERVAL        = 0.35f;

// ── Public API ─────────────────────────────────────────────────────────────

void GameAudio::update(float dt, const Camera& camera, const InputSystem& input) {
    if (!m_mgr) return;

    // On Emscripten, defer OpenAL init until first user interaction.
    if (!m_audio_ready) {
        try_deferred_init(input);
        if (!m_audio_ready) return;
    }

    // Update the AudioManager (crossfade, source reclaim, etc.).
    m_mgr->update(dt);

    // Sync listener to camera.
    update_listener(camera);

    // Drive footsteps from camera movement.
    update_footsteps(dt, camera);
}

void GameAudio::on_weapon_fire(float x, float y, float z) {
    if (!m_mgr || !m_audio_ready) return;
    m_mgr->play_sound_3d("blaster_fire", x, y, z, 0.8f);
}

void GameAudio::on_impact(float x, float y, float z) {
    if (!m_mgr || !m_audio_ready) return;
    m_mgr->play_sound_3d("blaster_impact", x, y, z, 0.6f);
}

void GameAudio::on_explosion(float x, float y, float z) {
    if (!m_mgr || !m_audio_ready) return;
    m_mgr->play_sound_3d("explosion", x, y, z, 1.0f);
}

void GameAudio::on_command_post_captured(float x, float y, float z) {
    if (!m_mgr || !m_audio_ready) return;
    // Use menu_select as placeholder for capture jingle.
    m_mgr->play_sound_3d("menu_select", x, y, z, 1.0f);
}

void GameAudio::on_menu_click() {
    if (!m_mgr || !m_audio_ready) return;
    m_mgr->play_sound("menu_click");
}

void GameAudio::on_menu_select() {
    if (!m_mgr || !m_audio_ready) return;
    m_mgr->play_sound("menu_select");
}

void GameAudio::start_battle_music() {
    if (!m_mgr || !m_audio_ready) return;
    if (m_battle_music_playing) return;

    m_mgr->play_music("music_battle", 2.0f);
    m_battle_music_playing = true;
}

void GameAudio::stop_music(float fade_seconds) {
    if (!m_mgr) return;
    m_mgr->stop_music(fade_seconds);
    m_battle_music_playing = false;
}

void GameAudio::start_ambient(const std::string& name) {
    if (!m_mgr || !m_audio_ready) return;
    if (m_ambient_playing) return;

    m_mgr->play_ambient(name, 0.4f);
    m_ambient_playing = true;
}

void GameAudio::stop_ambient() {
    if (!m_mgr) return;
    m_mgr->stop_all_ambient();
    m_ambient_playing = false;
}

void GameAudio::on_ai_weapon_fire(float x, float y, float z) {
    if (!m_mgr || !m_audio_ready) return;
    // Slightly quieter than player fire.
    m_mgr->play_sound_3d("blaster_fire", x, y, z, 0.5f, 1.05f);
}

void GameAudio::on_vehicle_engine(float x, float y, float z) {
    if (!m_mgr || !m_audio_ready) return;
    // Placeholder: low rumble at vehicle position.
    m_mgr->play_sound_3d("footstep", x, y, z, 0.3f, 0.5f);
}

void GameAudio::register_lvl_sound(const Sound& sound) {
    if (!m_mgr) return;
    if (sound.sample_data.empty() || sound.name.empty()) return;

    // Only PCM16 is currently supported for AudioBuffer upload.
    if (sound.format != SoundFormat::PCM16) {
        LOG_WARN("GameAudio: skipping non-PCM16 sound '%s' (format %u)",
                 sound.name.c_str(), static_cast<unsigned>(sound.format));
        return;
    }

    // Convert raw bytes to int16_t samples.
    const auto* pcm = reinterpret_cast<const int16_t*>(sound.sample_data.data());
    size_t sample_count = sound.sample_data.size() / sizeof(int16_t);

    // If audio isn't ready yet (Emscripten deferred init), queue for later.
    if (!m_audio_ready) {
        PendingSound ps;
        ps.name = sound.name;
        ps.pcm.assign(pcm, pcm + sample_count);
        ps.sample_rate = static_cast<int>(sound.sample_rate);
        ps.channels = static_cast<int>(sound.channels);
        m_pending_sounds.push_back(std::move(ps));
        LOG_DEBUG("GameAudio: queued LVL sound '%s' for deferred upload",
                  sound.name.c_str());
        return;
    }

    AudioBuffer buf;
    if (buf.load_pcm(pcm, sample_count,
                     static_cast<int>(sound.sample_rate),
                     static_cast<int>(sound.channels))) {
        m_mgr->register_sound(sound.name, std::move(buf));
        LOG_INFO("GameAudio: registered LVL sound '%s' (%.2fs)",
                 sound.name.c_str(), static_cast<double>(sound.duration()));
    } else {
        LOG_WARN("GameAudio: failed to upload PCM for '%s'", sound.name.c_str());
    }
}

void GameAudio::flush_pending_sounds() {
    if (!m_mgr || m_pending_sounds.empty()) return;

    LOG_INFO("GameAudio: uploading %zu queued sounds", m_pending_sounds.size());
    for (auto& ps : m_pending_sounds) {
        AudioBuffer buf;
        if (buf.load_pcm(ps.pcm.data(), ps.pcm.size(),
                         ps.sample_rate, ps.channels)) {
            m_mgr->register_sound(ps.name, std::move(buf));
            LOG_INFO("GameAudio: registered queued sound '%s'", ps.name.c_str());
        }
    }
    m_pending_sounds.clear();
}

bool GameAudio::is_audio_ready() const {
    return m_audio_ready;
}

// ── Private helpers ────────────────────────────────────────────────────────

void GameAudio::try_deferred_init(
        [[maybe_unused]] const InputSystem& input) {
#ifdef __EMSCRIPTEN__
    // On Emscripten, we must not init OpenAL until after a user gesture.
    if (m_audio_init_attempted) return;

    bool user_gesture = input.mouse_clicked(1)  // SDL_BUTTON_LEFT
                     || input.key_pressed(SDL_SCANCODE_RETURN)
                     || input.key_pressed(SDL_SCANCODE_SPACE)
                     || input.mouse_clicked(3); // SDL_BUTTON_RIGHT

    if (!user_gesture) return;

    m_audio_init_attempted = true;
    LOG_INFO("GameAudio: user gesture detected, initializing audio...");

    if (m_mgr->init()) {
        m_mgr->resume_audio_context();
        m_audio_ready = true;
        LOG_INFO("GameAudio: deferred audio init succeeded");

        // Upload any sounds that were loaded from .lvl before audio was ready.
        flush_pending_sounds();

        // Auto-start ambient and music now that audio is available.
        start_ambient("ambient_wind");
        start_battle_music();
    } else {
        LOG_WARN("GameAudio: deferred audio init failed");
    }
#else
    // On native, audio is already initialized in main().
    if (m_mgr && m_mgr->is_initialized()) {
        m_audio_ready = true;
    }
#endif
}

void GameAudio::update_listener(const Camera& camera) {
    if (!m_mgr || !m_audio_ready) return;

    // Derive forward and up vectors from camera yaw/pitch.
    float cp = std::cos(camera.pitch());
    float sp = std::sin(camera.pitch());
    float cy = std::cos(camera.yaw());
    float sy = std::sin(camera.yaw());

    // Forward direction (matches camera.cpp conventions).
    float fx = cp * sy;
    float fy = sp;
    float fz = -cp * cy;

    // Up direction.
    float ux = -sp * sy;
    float uy = cp;
    float uz = sp * cy;

    m_mgr->set_listener(camera.x(), camera.y(), camera.z(),
                         fx, fy, fz,
                         ux, uy, uz);
}

void GameAudio::update_footsteps(float dt, const Camera& camera) {
    if (!m_mgr || !m_audio_ready) return;

    if (!m_footstep_tracking) {
        // First frame: just record position.
        m_prev_cam_x = camera.x();
        m_prev_cam_z = camera.z();
        m_footstep_tracking = true;
        return;
    }

    float dx = camera.x() - m_prev_cam_x;
    float dz = camera.z() - m_prev_cam_z;
    float move_dist = std::sqrt(dx * dx + dz * dz);

    m_prev_cam_x = camera.x();
    m_prev_cam_z = camera.z();

    if (move_dist > FOOTSTEP_SPEED_THRESHOLD * dt) {
        m_footstep_timer -= dt;
        if (m_footstep_timer <= 0.0f) {
            m_mgr->play_sound_3d("footstep",
                                  camera.x(), camera.y() - 1.5f, camera.z(),
                                  0.4f);
            m_footstep_timer = FOOTSTEP_INTERVAL;
        }
    } else {
        // Not moving: reset timer so next step is immediate.
        m_footstep_timer = 0.0f;
    }
}

} // namespace swbf
