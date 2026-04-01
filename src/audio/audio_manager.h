#pragma once

#include "audio/audio_buffer.h"
#include "audio/audio_device.h"
#include "audio/audio_source.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace swbf {

/// Describes how a sound should be played.
struct SoundParams {
    float x = 0.0f, y = 0.0f, z = 0.0f;  // world position (ignored if !positional)
    bool  positional = false;               // true = 3D, false = 2D
    float gain       = 1.0f;               // per-sound volume multiplier
    float pitch      = 1.0f;               // playback speed/pitch
    bool  loop       = false;
};

/// Central audio manager.  Owns the AudioDevice, maintains a sound bank of
/// cached AudioBuffers, and manages a pool of AudioSource instances for
/// concurrent playback.
///
/// Provides separate volume channels: master, SFX, music, and ambient, each
/// in the range [0.0, 1.0].
///
/// Call init() once at startup, update() every frame, and shutdown() on exit.
class AudioManager {
public:
    AudioManager()  = default;
    ~AudioManager() = default;

    // Non-copyable, non-movable (owns OpenAL state).
    AudioManager(const AudioManager&)            = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // ---- Lifecycle ---------------------------------------------------------

    /// Initialise the audio device and generate built-in placeholder sounds.
    /// Returns false if OpenAL cannot be initialised.
    bool init();

    /// Per-frame update.  Reclaims finished sources, advances music crossfade,
    /// and applies volume changes.  Call once per frame.
    void update(float dt);

    /// Stop all playback and release every OpenAL resource.
    void shutdown();

    bool is_initialized() const { return m_initialized; }

    // ---- Sound bank --------------------------------------------------------

    /// Register an AudioBuffer under the given name.  The manager takes
    /// ownership.  If a buffer with this name already exists it is replaced.
    void register_sound(const std::string& name, AudioBuffer buffer);

    /// Returns true if a buffer with this name exists in the bank.
    bool has_sound(const std::string& name) const;

    // ---- Playback ----------------------------------------------------------

    /// Play a sound effect by name.  Returns the source index (for later
    /// stop/query) or -1 on failure.
    int play_sound(const std::string& name, const SoundParams& params = {});

    /// Convenience: play a positional (3D) sound at a world-space location.
    int play_sound_3d(const std::string& name, float x, float y, float z,
                      float gain = 1.0f, float pitch = 1.0f);

    /// Stop a specific source by index.
    void stop_sound(int source_index);

    /// Stop all currently playing sound effects.
    void stop_all_sfx();

    // ---- Music -------------------------------------------------------------

    /// Start playing a music track (by buffer name).  Crossfades from any
    /// currently playing music over `fade_seconds`.
    void play_music(const std::string& name, float fade_seconds = 1.0f);

    /// Stop music playback, fading out over `fade_seconds`.
    void stop_music(float fade_seconds = 1.0f);

    /// Returns true if music is currently playing or fading.
    bool is_music_playing() const;

    // ---- Ambient -----------------------------------------------------------

    /// Start a looping ambient layer by buffer name.
    void play_ambient(const std::string& name, float gain = 0.5f);

    /// Stop a specific ambient layer.
    void stop_ambient(const std::string& name);

    /// Stop all ambient layers.
    void stop_all_ambient();

    // ---- Volume controls ---------------------------------------------------

    void  set_master_volume(float v);
    float master_volume() const { return m_master_volume; }

    void  set_sfx_volume(float v);
    float sfx_volume() const { return m_sfx_volume; }

    void  set_music_volume(float v);
    float music_volume() const { return m_music_volume; }

    void  set_ambient_volume(float v);
    float ambient_volume() const { return m_ambient_volume; }

    // ---- Listener ----------------------------------------------------------

    /// Update the listener position and orientation (typically called with the
    /// camera transform each frame).
    void set_listener(float px, float py, float pz,
                      float fx, float fy, float fz,
                      float ux, float uy, float uz);

    // ---- Built-in placeholder sounds ---------------------------------------

    /// Generate and register procedural placeholder sounds so the system
    /// produces audible output even without .lvl audio data.
    /// Called automatically by init().
    void generate_placeholder_sounds();

    // ---- Emscripten autoplay -----------------------------------------------

    /// On web browsers, the audio context is typically suspended until a user
    /// gesture.  Call this once on the first click/keypress to ensure audio
    /// plays.  No-op on native builds.
    void resume_audio_context();

private:
    static constexpr int MAX_SOURCES = 32;  // source pool size

    // A managed source entry.
    struct ManagedSource {
        AudioSource source;
        bool        in_use   = false;
        bool        is_sfx   = true;   // false = music or ambient
        std::string sound_name;
    };

    // Ambient layer bookkeeping.
    struct AmbientLayer {
        int         source_idx = -1;
        std::string name;
        float       base_gain  = 0.5f;
    };

    // Music crossfade state.
    struct MusicState {
        int   source_idx     = -1;
        float target_gain    = 0.0f;
        float current_gain   = 0.0f;
        float fade_speed     = 0.0f;  // gain per second
        bool  fading_out     = false;
    };

    // Allocate a source from the pool.  Returns index or -1 if full.
    int acquire_source();

    // Release a source back to the pool.
    void release_source(int idx);

    // Compute effective gain for a channel.
    float effective_sfx_gain() const    { return m_master_volume * m_sfx_volume; }
    float effective_music_gain() const  { return m_master_volume * m_music_volume; }
    float effective_ambient_gain() const { return m_master_volume * m_ambient_volume; }

    // Apply current volume to a music or ambient source.
    void apply_music_gain();
    void apply_ambient_gains();

    bool         m_initialized    = false;
    AudioDevice  m_device;

    // Volume channels.
    float m_master_volume  = 1.0f;
    float m_sfx_volume     = 1.0f;
    float m_music_volume   = 0.7f;
    float m_ambient_volume = 0.5f;

    // Sound bank: name -> buffer.
    std::unordered_map<std::string, AudioBuffer> m_sound_bank;

    // Source pool.
    ManagedSource m_sources[MAX_SOURCES];

    // Music state (supports crossfade between two tracks).
    MusicState m_music_current;
    MusicState m_music_next;

    // Ambient layers.
    std::vector<AmbientLayer> m_ambient_layers;

    // Emscripten autoplay flag.
    bool m_audio_context_resumed = false;
};

} // namespace swbf
