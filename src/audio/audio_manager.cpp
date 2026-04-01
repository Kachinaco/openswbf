#include "audio/audio_manager.h"
#include "core/log.h"

#include <AL/al.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace swbf {

// ── Lifecycle ──────────────────────────────────────────────────────────────

bool AudioManager::init() {
    if (m_initialized) {
        LOG_WARN("AudioManager::init called but already initialised");
        return true;
    }

    if (!m_device.init()) {
        LOG_ERROR("AudioManager: failed to initialise audio device");
        return false;
    }

    // Create the source pool.
    for (int i = 0; i < MAX_SOURCES; ++i) {
        m_sources[i].source.create();
        m_sources[i].in_use = false;
    }

    // Generate built-in placeholder sounds.
    generate_placeholder_sounds();

    m_initialized = true;
    LOG_INFO("AudioManager initialised (%d source pool, %zu placeholder sounds)",
             MAX_SOURCES, m_sound_bank.size());
    return true;
}

void AudioManager::update(float dt) {
    if (!m_initialized) return;

    // -- Reclaim finished SFX sources -------------------------------------
    for (int i = 0; i < MAX_SOURCES; ++i) {
        auto& ms = m_sources[i];
        if (ms.in_use && ms.is_sfx && !ms.source.is_playing()) {
            ms.source.stop();
            ms.in_use = false;
        }
    }

    // -- Music crossfade --------------------------------------------------
    auto advance_fade = [&](MusicState& ms, float effective_vol) {
        if (ms.source_idx < 0) return;

        if (ms.fading_out) {
            ms.current_gain -= ms.fade_speed * dt;
            if (ms.current_gain <= 0.0f) {
                ms.current_gain = 0.0f;
                m_sources[ms.source_idx].source.stop();
                release_source(ms.source_idx);
                ms.source_idx = -1;
                ms.fading_out = false;
                return;
            }
        } else {
            // Fading in.
            if (ms.current_gain < ms.target_gain) {
                ms.current_gain += ms.fade_speed * dt;
                if (ms.current_gain > ms.target_gain)
                    ms.current_gain = ms.target_gain;
            }
        }

        // Apply gain.
        if (ms.source_idx >= 0) {
            m_sources[ms.source_idx].source.set_gain(
                ms.current_gain * effective_vol);
        }
    };

    float eff_music = effective_music_gain();
    advance_fade(m_music_current, eff_music);
    advance_fade(m_music_next, eff_music);

    // If the old track finished fading out and a next track is pending,
    // promote next -> current.
    if (m_music_current.source_idx < 0 && m_music_next.source_idx >= 0) {
        m_music_current = m_music_next;
        m_music_next = {};
        m_music_next.source_idx = -1;
    }
}

void AudioManager::shutdown() {
    if (!m_initialized) return;

    // Stop everything.
    for (int i = 0; i < MAX_SOURCES; ++i) {
        m_sources[i].source.stop();
        m_sources[i].source.destroy();
        m_sources[i].in_use = false;
    }

    m_ambient_layers.clear();
    m_music_current = {};
    m_music_current.source_idx = -1;
    m_music_next = {};
    m_music_next.source_idx = -1;

    // Destroy all buffers.
    for (auto& [name, buf] : m_sound_bank) {
        buf.destroy();
    }
    m_sound_bank.clear();

    m_device.shutdown();
    m_initialized = false;

    LOG_INFO("AudioManager shut down");
}

// ── Sound bank ─────────────────────────────────────────────────────────────

void AudioManager::register_sound(const std::string& name, AudioBuffer buffer) {
    auto it = m_sound_bank.find(name);
    if (it != m_sound_bank.end()) {
        it->second.destroy();
        it->second = std::move(buffer);
        LOG_DEBUG("AudioManager: replaced sound '%s'", name.c_str());
    } else {
        m_sound_bank.emplace(name, std::move(buffer));
        LOG_DEBUG("AudioManager: registered sound '%s'", name.c_str());
    }
}

bool AudioManager::has_sound(const std::string& name) const {
    return m_sound_bank.find(name) != m_sound_bank.end();
}

// ── Source pool ────────────────────────────────────────────────────────────

int AudioManager::acquire_source() {
    for (int i = 0; i < MAX_SOURCES; ++i) {
        if (!m_sources[i].in_use) {
            m_sources[i].in_use = true;
            return i;
        }
    }
    // Pool is full — try to steal the oldest SFX source.
    for (int i = 0; i < MAX_SOURCES; ++i) {
        if (m_sources[i].is_sfx) {
            m_sources[i].source.stop();
            return i;
        }
    }
    return -1;
}

void AudioManager::release_source(int idx) {
    if (idx < 0 || idx >= MAX_SOURCES) return;
    m_sources[idx].source.stop();
    m_sources[idx].in_use = false;
    m_sources[idx].sound_name.clear();
}

// ── Playback ───────────────────────────────────────────────────────────────

int AudioManager::play_sound(const std::string& name, const SoundParams& params) {
    if (!m_initialized) return -1;

    auto it = m_sound_bank.find(name);
    if (it == m_sound_bank.end()) {
        LOG_WARN("AudioManager: sound '%s' not found", name.c_str());
        return -1;
    }

    int idx = acquire_source();
    if (idx < 0) {
        LOG_WARN("AudioManager: no free sources for '%s'", name.c_str());
        return -1;
    }

    auto& ms = m_sources[idx];
    ms.is_sfx     = true;
    ms.sound_name = name;

    ms.source.stop();
    ms.source.set_buffer(it->second);
    ms.source.set_gain(params.gain * effective_sfx_gain());
    ms.source.set_pitch(params.pitch);
    ms.source.set_looping(params.loop);

    if (params.positional) {
        ms.source.set_position(params.x, params.y, params.z);
        // Mark as positional for OpenAL distance model.
        alSourcei(static_cast<ALuint>(ms.source.id()), AL_SOURCE_RELATIVE, AL_FALSE);
        alSourcef(static_cast<ALuint>(ms.source.id()), AL_REFERENCE_DISTANCE, 10.0f);
        alSourcef(static_cast<ALuint>(ms.source.id()), AL_MAX_DISTANCE, 200.0f);
        alSourcef(static_cast<ALuint>(ms.source.id()), AL_ROLLOFF_FACTOR, 1.0f);
    } else {
        // 2D sound: play relative to listener at origin.
        alSourcei(static_cast<ALuint>(ms.source.id()), AL_SOURCE_RELATIVE, AL_TRUE);
        ms.source.set_position(0.0f, 0.0f, 0.0f);
    }

    ms.source.play();
    return idx;
}

int AudioManager::play_sound_3d(const std::string& name,
                                 float x, float y, float z,
                                 float gain, float pitch) {
    SoundParams p;
    p.positional = true;
    p.x = x; p.y = y; p.z = z;
    p.gain  = gain;
    p.pitch = pitch;
    return play_sound(name, p);
}

void AudioManager::stop_sound(int source_index) {
    if (source_index < 0 || source_index >= MAX_SOURCES) return;
    release_source(source_index);
}

void AudioManager::stop_all_sfx() {
    for (int i = 0; i < MAX_SOURCES; ++i) {
        if (m_sources[i].in_use && m_sources[i].is_sfx) {
            release_source(i);
        }
    }
}

// ── Music ──────────────────────────────────────────────────────────────────

void AudioManager::play_music(const std::string& name, float fade_seconds) {
    if (!m_initialized) return;

    auto it = m_sound_bank.find(name);
    if (it == m_sound_bank.end()) {
        LOG_WARN("AudioManager: music '%s' not found", name.c_str());
        return;
    }

    float fade_rate = (fade_seconds > 0.01f) ? (1.0f / fade_seconds) : 100.0f;

    // Fade out current music.
    if (m_music_current.source_idx >= 0) {
        m_music_current.fading_out = true;
        m_music_current.fade_speed = fade_rate;
    }

    // Allocate a new source for the incoming track.
    int idx = acquire_source();
    if (idx < 0) {
        LOG_WARN("AudioManager: no free source for music '%s'", name.c_str());
        return;
    }

    auto& ms = m_sources[idx];
    ms.is_sfx     = false;
    ms.sound_name = name;

    ms.source.stop();
    ms.source.set_buffer(it->second);
    ms.source.set_looping(true);
    ms.source.set_pitch(1.0f);

    // 2D (non-positional) music.
    alSourcei(static_cast<ALuint>(ms.source.id()), AL_SOURCE_RELATIVE, AL_TRUE);
    ms.source.set_position(0.0f, 0.0f, 0.0f);

    // Start at zero gain and fade in.
    ms.source.set_gain(0.0f);
    ms.source.play();

    MusicState next;
    next.source_idx  = idx;
    next.target_gain = 1.0f;
    next.current_gain = 0.0f;
    next.fade_speed  = fade_rate;
    next.fading_out  = false;

    if (m_music_current.source_idx >= 0) {
        // Current is fading out; put new track in the "next" slot.
        m_music_next = next;
    } else {
        m_music_current = next;
    }

    LOG_INFO("AudioManager: playing music '%s' (fade %.1fs)", name.c_str(),
             static_cast<double>(fade_seconds));
}

void AudioManager::stop_music(float fade_seconds) {
    float fade_rate = (fade_seconds > 0.01f) ? (1.0f / fade_seconds) : 100.0f;

    if (m_music_current.source_idx >= 0) {
        m_music_current.fading_out = true;
        m_music_current.fade_speed = fade_rate;
    }
    if (m_music_next.source_idx >= 0) {
        m_music_next.fading_out = true;
        m_music_next.fade_speed = fade_rate;
    }
}

bool AudioManager::is_music_playing() const {
    return m_music_current.source_idx >= 0 || m_music_next.source_idx >= 0;
}

// ── Ambient ────────────────────────────────────────────────────────────────

void AudioManager::play_ambient(const std::string& name, float gain) {
    if (!m_initialized) return;

    // Check if this ambient layer is already playing.
    for (const auto& layer : m_ambient_layers) {
        if (layer.name == name) return;  // already active
    }

    auto it = m_sound_bank.find(name);
    if (it == m_sound_bank.end()) {
        LOG_WARN("AudioManager: ambient '%s' not found", name.c_str());
        return;
    }

    int idx = acquire_source();
    if (idx < 0) {
        LOG_WARN("AudioManager: no free source for ambient '%s'", name.c_str());
        return;
    }

    auto& ms = m_sources[idx];
    ms.is_sfx     = false;
    ms.sound_name = name;

    ms.source.stop();
    ms.source.set_buffer(it->second);
    ms.source.set_looping(true);
    ms.source.set_pitch(1.0f);

    // 2D ambient.
    alSourcei(static_cast<ALuint>(ms.source.id()), AL_SOURCE_RELATIVE, AL_TRUE);
    ms.source.set_position(0.0f, 0.0f, 0.0f);

    ms.source.set_gain(gain * effective_ambient_gain());
    ms.source.play();

    AmbientLayer layer;
    layer.source_idx = idx;
    layer.name       = name;
    layer.base_gain  = gain;
    m_ambient_layers.push_back(layer);

    LOG_INFO("AudioManager: playing ambient '%s'", name.c_str());
}

void AudioManager::stop_ambient(const std::string& name) {
    for (auto it = m_ambient_layers.begin(); it != m_ambient_layers.end(); ++it) {
        if (it->name == name) {
            release_source(it->source_idx);
            m_ambient_layers.erase(it);
            return;
        }
    }
}

void AudioManager::stop_all_ambient() {
    for (auto& layer : m_ambient_layers) {
        release_source(layer.source_idx);
    }
    m_ambient_layers.clear();
}

// ── Volume controls ────────────────────────────────────────────────────────

static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

void AudioManager::set_master_volume(float v) {
    m_master_volume = clamp01(v);
    if (m_initialized) {
        m_device.set_listener_gain(m_master_volume);
    }
    apply_music_gain();
    apply_ambient_gains();
}

void AudioManager::set_sfx_volume(float v) {
    m_sfx_volume = clamp01(v);
    // SFX gain is applied per-sound at play time; currently playing sounds
    // won't change retroactively (acceptable for a game).
}

void AudioManager::set_music_volume(float v) {
    m_music_volume = clamp01(v);
    apply_music_gain();
}

void AudioManager::set_ambient_volume(float v) {
    m_ambient_volume = clamp01(v);
    apply_ambient_gains();
}

void AudioManager::apply_music_gain() {
    if (!m_initialized) return;
    float eff = effective_music_gain();
    if (m_music_current.source_idx >= 0) {
        m_sources[m_music_current.source_idx].source.set_gain(
            m_music_current.current_gain * eff);
    }
    if (m_music_next.source_idx >= 0) {
        m_sources[m_music_next.source_idx].source.set_gain(
            m_music_next.current_gain * eff);
    }
}

void AudioManager::apply_ambient_gains() {
    if (!m_initialized) return;
    float eff = effective_ambient_gain();
    for (auto& layer : m_ambient_layers) {
        if (layer.source_idx >= 0) {
            m_sources[layer.source_idx].source.set_gain(layer.base_gain * eff);
        }
    }
}

// ── Listener ───────────────────────────────────────────────────────────────

void AudioManager::set_listener(float px, float py, float pz,
                                float fx, float fy, float fz,
                                float ux, float uy, float uz) {
    if (!m_initialized) return;
    m_device.set_listener_position(px, py, pz);
    m_device.set_listener_orientation(fx, fy, fz, ux, uy, uz);
}

// ── Emscripten autoplay ────────────────────────────────────────────────────

void AudioManager::resume_audio_context() {
#ifdef __EMSCRIPTEN__
    if (!m_audio_context_resumed) {
        // Emscripten's OpenAL implementation wraps Web Audio.  The AudioContext
        // starts suspended due to browser autoplay policy and must be resumed
        // after a user gesture (click, keypress, touch).
        EM_ASM({
            if (typeof Module !== 'undefined' && Module.AL && Module.AL.currentCtx) {
                var ctx = Module.AL.currentCtx.audioCtx;
                if (ctx && ctx.state === 'suspended') {
                    ctx.resume();
                }
            }
        });
        m_audio_context_resumed = true;
        LOG_INFO("AudioManager: requested Web Audio context resume");
    }
#else
    // No-op on native.
    m_audio_context_resumed = true;
#endif
}

// ── Placeholder sound generation ───────────────────────────────────────────
//
// These synthesised sounds let the audio system produce audible output even
// when no .lvl audio data is available.  Each is a short PCM waveform:
//
//   blaster_fire    — short sine-wave "pew" with frequency sweep down
//   blaster_impact  — short burst of filtered noise
//   explosion       — low rumble noise envelope
//   footstep        — brief thump (low-freq sine impulse)
//   menu_click      — very short high-frequency tick
//   menu_select     — two-tone ascending beep
//   music_battle    — looping low drone + rhythm (placeholder ambience)
//   ambient_wind    — filtered noise loop
//

static constexpr int SAMPLE_RATE = 22050;
static constexpr float PI = 3.14159265358979323846f;

// Simple pseudo-random for noise generation (deterministic, no seed needed).
static uint32_t s_rng_state = 0x12345678u;
static float noise() {
    s_rng_state ^= s_rng_state << 13;
    s_rng_state ^= s_rng_state >> 17;
    s_rng_state ^= s_rng_state << 5;
    return static_cast<float>(s_rng_state) / static_cast<float>(0xFFFFFFFFu) * 2.0f - 1.0f;
}

// Generate a buffer and register it.
static AudioBuffer make_buffer(const std::vector<int16_t>& samples, int channels = 1) {
    AudioBuffer buf;
    buf.load_pcm(samples.data(), samples.size(), SAMPLE_RATE, channels);
    return buf;
}

void AudioManager::generate_placeholder_sounds() {
    // -- blaster_fire: 150ms frequency sweep from 880 Hz to 220 Hz ----------
    {
        int n = SAMPLE_RATE * 15 / 100;  // 0.15s
        std::vector<int16_t> samples(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(SAMPLE_RATE);
            float progress = static_cast<float>(i) / static_cast<float>(n);
            float freq = 880.0f - 660.0f * progress;  // sweep 880 -> 220
            float envelope = 1.0f - progress;          // linear fade-out
            envelope *= envelope;                       // quadratic decay
            float sample = std::sin(2.0f * PI * freq * t) * envelope;
            samples[static_cast<size_t>(i)] = static_cast<int16_t>(sample * 24000.0f);
        }
        register_sound("blaster_fire", make_buffer(samples));
    }

    // -- blaster_impact: 100ms noise burst with fast decay -------------------
    {
        int n = SAMPLE_RATE / 10;  // 0.1s
        std::vector<int16_t> samples(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            float progress = static_cast<float>(i) / static_cast<float>(n);
            float envelope = (1.0f - progress);
            envelope = envelope * envelope * envelope;
            float sample = noise() * envelope;
            samples[static_cast<size_t>(i)] = static_cast<int16_t>(sample * 18000.0f);
        }
        register_sound("blaster_impact", make_buffer(samples));
    }

    // -- explosion: 500ms low rumble with noise -----------------------------
    {
        int n = SAMPLE_RATE / 2;  // 0.5s
        std::vector<int16_t> samples(static_cast<size_t>(n));
        float lp = 0.0f;  // simple low-pass state
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(SAMPLE_RATE);
            float progress = static_cast<float>(i) / static_cast<float>(n);

            // Envelope: quick attack, slow decay.
            float envelope = 0.0f;
            if (progress < 0.05f) {
                envelope = progress / 0.05f;
            } else {
                float decay_t = (progress - 0.05f) / 0.95f;
                envelope = 1.0f - decay_t;
                envelope *= envelope;
            }

            // Mix low sine + filtered noise.
            float lo_sine = std::sin(2.0f * PI * 60.0f * t);
            float raw_noise = noise();
            lp = lp * 0.9f + raw_noise * 0.1f;  // low-pass filter

            float sample = (lo_sine * 0.6f + lp * 0.4f) * envelope;
            samples[static_cast<size_t>(i)] = static_cast<int16_t>(sample * 28000.0f);
        }
        register_sound("explosion", make_buffer(samples));
    }

    // -- footstep: 60ms low thump -------------------------------------------
    {
        int n = SAMPLE_RATE * 6 / 100;  // 0.06s
        std::vector<int16_t> samples(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(SAMPLE_RATE);
            float progress = static_cast<float>(i) / static_cast<float>(n);
            float envelope = 1.0f - progress;
            envelope *= envelope;
            float sample = std::sin(2.0f * PI * 80.0f * t) * envelope;
            // Add a click at the very beginning.
            if (progress < 0.05f) {
                sample += noise() * (1.0f - progress / 0.05f) * 0.3f;
            }
            samples[static_cast<size_t>(i)] = static_cast<int16_t>(sample * 16000.0f);
        }
        register_sound("footstep", make_buffer(samples));
    }

    // -- menu_click: 20ms high-frequency tick -------------------------------
    {
        int n = SAMPLE_RATE / 50;  // 0.02s
        std::vector<int16_t> samples(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(SAMPLE_RATE);
            float progress = static_cast<float>(i) / static_cast<float>(n);
            float envelope = 1.0f - progress;
            float sample = std::sin(2.0f * PI * 2000.0f * t) * envelope;
            samples[static_cast<size_t>(i)] = static_cast<int16_t>(sample * 12000.0f);
        }
        register_sound("menu_click", make_buffer(samples));
    }

    // -- menu_select: 80ms two-tone ascending beep --------------------------
    {
        int n = SAMPLE_RATE * 8 / 100;  // 0.08s
        std::vector<int16_t> samples(static_cast<size_t>(n));
        int half = n / 2;
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(SAMPLE_RATE);
            float progress = static_cast<float>(i) / static_cast<float>(n);
            float freq = (i < half) ? 660.0f : 880.0f;
            float envelope = 1.0f - progress * 0.5f;
            float sample = std::sin(2.0f * PI * freq * t) * envelope;
            samples[static_cast<size_t>(i)] = static_cast<int16_t>(sample * 10000.0f);
        }
        register_sound("menu_select", make_buffer(samples));
    }

    // -- music_battle: 4-second looping low drone ---------------------------
    {
        int n = SAMPLE_RATE * 4;  // 4s loop
        std::vector<int16_t> samples(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(SAMPLE_RATE);

            // Bass drone — two detuned sines.
            float bass = std::sin(2.0f * PI * 55.0f * t) * 0.3f
                       + std::sin(2.0f * PI * 55.5f * t) * 0.2f;

            // Rhythmic pulse (amplitude modulation at ~2 Hz).
            float pulse = 0.6f + 0.4f * std::sin(2.0f * PI * 2.0f * t);

            // High overtone for "tension."
            float high = std::sin(2.0f * PI * 220.0f * t) * 0.08f;

            float sample = (bass * pulse + high) * 0.5f;
            samples[static_cast<size_t>(i)] = static_cast<int16_t>(sample * 20000.0f);
        }
        register_sound("music_battle", make_buffer(samples));
    }

    // -- music_menu: 4-second peaceful melody loop --------------------------
    {
        int n = SAMPLE_RATE * 4;
        std::vector<int16_t> samples(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(SAMPLE_RATE);

            // Simple arpeggiated chord (C major: C4, E4, G4 cycling).
            float arp_freq = 261.63f;
            float beat = std::fmod(t * 3.0f, 3.0f);  // cycle at 1 Hz
            if (beat < 1.0f)      arp_freq = 261.63f;  // C4
            else if (beat < 2.0f) arp_freq = 329.63f;  // E4
            else                  arp_freq = 392.00f;  // G4

            float melody = std::sin(2.0f * PI * arp_freq * t) * 0.25f;

            // Pad: slow sine at the root.
            float pad = std::sin(2.0f * PI * 130.81f * t) * 0.15f;

            float sample = melody + pad;
            samples[static_cast<size_t>(i)] = static_cast<int16_t>(sample * 16000.0f);
        }
        register_sound("music_menu", make_buffer(samples));
    }

    // -- ambient_wind: 3-second filtered noise loop -------------------------
    {
        int n = SAMPLE_RATE * 3;
        std::vector<int16_t> samples(static_cast<size_t>(n));
        float lp = 0.0f;
        float lp2 = 0.0f;
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(SAMPLE_RATE);

            // Slow modulation.
            float mod = 0.5f + 0.5f * std::sin(2.0f * PI * 0.3f * t);

            float raw = noise() * mod;
            lp  = lp  * 0.95f + raw  * 0.05f;  // first low-pass
            lp2 = lp2 * 0.95f + lp   * 0.05f;  // second pass — softer

            float sample = lp2 * 0.6f;
            samples[static_cast<size_t>(i)] = static_cast<int16_t>(sample * 14000.0f);
        }
        register_sound("ambient_wind", make_buffer(samples));
    }

    // -- ambient_forest: 3-second chirps + rustle ---------------------------
    {
        int n = SAMPLE_RATE * 3;
        std::vector<int16_t> samples(static_cast<size_t>(n));
        float lp = 0.0f;
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(SAMPLE_RATE);

            // Background rustle.
            float rustle = noise() * 0.08f;
            lp = lp * 0.92f + rustle * 0.08f;

            // Occasional chirp (bird-like sine bursts).
            float chirp = 0.0f;
            float chirp_t = std::fmod(t, 1.5f);
            if (chirp_t < 0.08f) {
                float env = 1.0f - chirp_t / 0.08f;
                chirp = std::sin(2.0f * PI * (2500.0f + 500.0f * chirp_t / 0.08f) * t)
                        * env * env * 0.15f;
            }

            float sample = lp + chirp;
            samples[static_cast<size_t>(i)] = static_cast<int16_t>(sample * 16000.0f);
        }
        register_sound("ambient_forest", make_buffer(samples));
    }

    LOG_INFO("AudioManager: generated %zu placeholder sounds", m_sound_bank.size());
}

} // namespace swbf
