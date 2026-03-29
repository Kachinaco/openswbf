#include "audio/audio_buffer.h"
#include "core/log.h"

#include <AL/al.h>

namespace swbf {

// ── Move operations ─────────────────────────────────────────────────────────

AudioBuffer::AudioBuffer(AudioBuffer&& other) noexcept
    : m_buffer(other.m_buffer)
{
    other.m_buffer = 0;
}

AudioBuffer& AudioBuffer::operator=(AudioBuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        m_buffer       = other.m_buffer;
        other.m_buffer = 0;
    }
    return *this;
}

// ── Public API ──────────────────────────────────────────────────────────────

bool AudioBuffer::load_pcm(const int16_t* data, size_t sample_count,
                            int sample_rate, int channels) {
    if (!data || sample_count == 0) {
        LOG_ERROR("AudioBuffer::load_pcm called with null/empty data");
        return false;
    }

    ALenum format = 0;
    if (channels == 1) {
        format = AL_FORMAT_MONO16;
    } else if (channels == 2) {
        format = AL_FORMAT_STEREO16;
    } else {
        LOG_ERROR("AudioBuffer::load_pcm — unsupported channel count %d", channels);
        return false;
    }

    // Tear down any previous buffer.
    destroy();

    ALuint buf = 0;
    alGenBuffers(1, &buf);
    if (alGetError() != AL_NO_ERROR) {
        LOG_ERROR("alGenBuffers failed");
        return false;
    }

    // Size in bytes: each sample is a 16-bit (2-byte) value.
    ALsizei size_bytes = static_cast<ALsizei>(sample_count * sizeof(int16_t));

    alBufferData(buf, format, data, size_bytes, static_cast<ALsizei>(sample_rate));
    ALenum err = alGetError();
    if (err != AL_NO_ERROR) {
        LOG_ERROR("alBufferData failed (error 0x%04x)", static_cast<int>(err));
        alDeleteBuffers(1, &buf);
        return false;
    }

    m_buffer = static_cast<uint32_t>(buf);

    LOG_DEBUG("AudioBuffer loaded: %zu samples, %d Hz, %d ch (id %u)",
              sample_count, sample_rate, channels, m_buffer);
    return true;
}

void AudioBuffer::destroy() {
    if (m_buffer != 0) {
        ALuint buf = static_cast<ALuint>(m_buffer);
        alDeleteBuffers(1, &buf);
        m_buffer = 0;
    }
}

} // namespace swbf
