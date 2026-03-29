#pragma once

#include "audio/audio_buffer.h"

namespace swbf {

/// Wraps an OpenAL source — a point in 3D space that emits sound from an
/// attached AudioBuffer.  Used for SFX, ambient loops, voice lines, etc.
class AudioSource {
public:
    AudioSource()  = default;
    ~AudioSource() = default;

    // Non-copyable, movable.
    AudioSource(const AudioSource&)            = delete;
    AudioSource& operator=(const AudioSource&) = delete;
    AudioSource(AudioSource&& other) noexcept;
    AudioSource& operator=(AudioSource&& other) noexcept;

    /// Generate the underlying OpenAL source.
    void create();

    /// Delete the OpenAL source.  Safe to call if not yet created.
    void destroy();

    /// Attach a buffer to this source.  The buffer must outlive any playback.
    void set_buffer(const AudioBuffer& buffer);

    /// Set the source's position in world space.
    void set_position(float x, float y, float z);

    /// Set the source's velocity (used for Doppler calculations).
    void set_velocity(float vx, float vy, float vz);

    /// Set playback volume (0.0 = silence, 1.0 = full, >1.0 amplified).
    void set_gain(float gain);

    /// Set playback speed / pitch multiplier (1.0 = normal).
    void set_pitch(float pitch);

    /// Enable or disable looping.
    void set_looping(bool loop);

    /// Start playback.
    void play();

    /// Stop playback and rewind.
    void stop();

    /// Pause playback (resume with play()).
    void pause();

    /// Returns true if the source is currently in the AL_PLAYING state.
    bool is_playing() const;

    /// The raw OpenAL source name (0 if not created).
    uint32_t id() const { return m_source; }

private:
    uint32_t m_source = 0;
};

} // namespace swbf
