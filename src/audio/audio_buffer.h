#pragma once

#include <cstddef>
#include <cstdint>

namespace swbf {

/// Holds a single OpenAL buffer — an immutable block of PCM sample data that
/// can be attached to one or more AudioSource instances.
class AudioBuffer {
public:
    AudioBuffer()  = default;
    ~AudioBuffer() = default;

    // Non-copyable (owns an OpenAL resource), but movable.
    AudioBuffer(const AudioBuffer&)            = delete;
    AudioBuffer& operator=(const AudioBuffer&) = delete;
    AudioBuffer(AudioBuffer&& other) noexcept;
    AudioBuffer& operator=(AudioBuffer&& other) noexcept;

    /// Upload raw PCM data into a new OpenAL buffer.
    /// @param data         Pointer to interleaved 16-bit signed samples.
    /// @param sample_count Total number of samples (frames * channels).
    /// @param sample_rate  Sample rate in Hz (e.g. 22050, 44100).
    /// @param channels     1 for mono, 2 for stereo.
    /// @return true on success.
    bool load_pcm(const int16_t* data, size_t sample_count,
                  int sample_rate, int channels);

    /// Release the underlying OpenAL buffer.  Safe to call if no buffer has
    /// been created.
    void destroy();

    /// The raw OpenAL buffer name (0 if not created).
    uint32_t id() const { return m_buffer; }

private:
    uint32_t m_buffer = 0;
};

} // namespace swbf
