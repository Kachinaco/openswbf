#pragma once

namespace swbf {

/// Manages the OpenAL device and context.  Exactly one AudioDevice should be
/// alive at a time — it owns the global audio output path.
class AudioDevice {
public:
    /// Open the default audio device and create an OpenAL context.
    /// Returns false if OpenAL initialisation fails.
    bool init();

    /// Tear down the context and close the device.
    void shutdown();

    /// Position the listener in world space (typically the camera position).
    void set_listener_position(float x, float y, float z);

    /// Set the listener's forward and up vectors.
    void set_listener_orientation(float fx, float fy, float fz,
                                  float ux, float uy, float uz);

    /// Set master gain (0.0 = silence, 1.0 = full volume).
    void set_listener_gain(float gain);

    bool is_initialized() const { return m_initialized; }

private:
    bool  m_initialized = false;
    void* m_device      = nullptr;   // ALCdevice*  — opaque to callers
    void* m_context     = nullptr;   // ALCcontext* — opaque to callers
};

} // namespace swbf
