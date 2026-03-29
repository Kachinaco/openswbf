#include "audio/audio_source.h"
#include "core/log.h"

#include <AL/al.h>

namespace swbf {

// ── Move operations ─────────────────────────────────────────────────────────

AudioSource::AudioSource(AudioSource&& other) noexcept
    : m_source(other.m_source)
{
    other.m_source = 0;
}

AudioSource& AudioSource::operator=(AudioSource&& other) noexcept {
    if (this != &other) {
        destroy();
        m_source       = other.m_source;
        other.m_source = 0;
    }
    return *this;
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

void AudioSource::create() {
    if (m_source != 0) {
        LOG_WARN("AudioSource::create called but source already exists (id %u)", m_source);
        return;
    }

    ALuint src = 0;
    alGenSources(1, &src);
    if (alGetError() != AL_NO_ERROR) {
        LOG_ERROR("alGenSources failed");
        return;
    }

    m_source = static_cast<uint32_t>(src);
}

void AudioSource::destroy() {
    if (m_source != 0) {
        ALuint src = static_cast<ALuint>(m_source);
        alDeleteSources(1, &src);
        m_source = 0;
    }
}

// ── Properties ──────────────────────────────────────────────────────────────

void AudioSource::set_buffer(const AudioBuffer& buffer) {
    if (m_source == 0) return;
    alSourcei(static_cast<ALuint>(m_source), AL_BUFFER,
              static_cast<ALint>(buffer.id()));
}

void AudioSource::set_position(float x, float y, float z) {
    if (m_source == 0) return;
    alSource3f(static_cast<ALuint>(m_source), AL_POSITION, x, y, z);
}

void AudioSource::set_velocity(float vx, float vy, float vz) {
    if (m_source == 0) return;
    alSource3f(static_cast<ALuint>(m_source), AL_VELOCITY, vx, vy, vz);
}

void AudioSource::set_gain(float gain) {
    if (m_source == 0) return;
    alSourcef(static_cast<ALuint>(m_source), AL_GAIN, gain);
}

void AudioSource::set_pitch(float pitch) {
    if (m_source == 0) return;
    alSourcef(static_cast<ALuint>(m_source), AL_PITCH, pitch);
}

void AudioSource::set_looping(bool loop) {
    if (m_source == 0) return;
    alSourcei(static_cast<ALuint>(m_source), AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
}

// ── Playback ────────────────────────────────────────────────────────────────

void AudioSource::play() {
    if (m_source == 0) return;
    alSourcePlay(static_cast<ALuint>(m_source));
}

void AudioSource::stop() {
    if (m_source == 0) return;
    alSourceStop(static_cast<ALuint>(m_source));
}

void AudioSource::pause() {
    if (m_source == 0) return;
    alSourcePause(static_cast<ALuint>(m_source));
}

bool AudioSource::is_playing() const {
    if (m_source == 0) return false;

    ALint state = 0;
    alGetSourcei(static_cast<ALuint>(m_source), AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

} // namespace swbf
