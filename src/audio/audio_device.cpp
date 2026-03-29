#include "audio/audio_device.h"
#include "core/log.h"

#include <AL/al.h>
#include <AL/alc.h>

namespace swbf {

bool AudioDevice::init() {
    if (m_initialized) {
        LOG_WARN("AudioDevice::init called but already initialised");
        return true;
    }

    ALCdevice* device = alcOpenDevice(nullptr);  // default device
    if (!device) {
        LOG_ERROR("Failed to open default OpenAL device");
        return false;
    }

    ALCcontext* context = alcCreateContext(device, nullptr);
    if (!context) {
        LOG_ERROR("Failed to create OpenAL context");
        alcCloseDevice(device);
        return false;
    }

    if (!alcMakeContextCurrent(context)) {
        LOG_ERROR("Failed to make OpenAL context current");
        alcDestroyContext(context);
        alcCloseDevice(device);
        return false;
    }

    m_device  = static_cast<void*>(device);
    m_context = static_cast<void*>(context);
    m_initialized = true;

    // Default listener at the origin, facing -Z, up +Y (OpenAL convention).
    set_listener_position(0.0f, 0.0f, 0.0f);
    set_listener_orientation(0.0f, 0.0f, -1.0f,
                             0.0f, 1.0f,  0.0f);
    set_listener_gain(1.0f);

    const char* device_name = alcGetString(device, ALC_DEVICE_SPECIFIER);
    LOG_INFO("OpenAL audio device opened: %s", device_name ? device_name : "(unknown)");

    return true;
}

void AudioDevice::shutdown() {
    if (!m_initialized) return;

    auto* context = static_cast<ALCcontext*>(m_context);
    auto* device  = static_cast<ALCdevice*>(m_device);

    alcMakeContextCurrent(nullptr);

    if (context) {
        alcDestroyContext(context);
    }
    if (device) {
        alcCloseDevice(device);
    }

    m_context     = nullptr;
    m_device      = nullptr;
    m_initialized = false;

    LOG_INFO("OpenAL audio device shut down");
}

void AudioDevice::set_listener_position(float x, float y, float z) {
    if (!m_initialized) return;
    alListener3f(AL_POSITION, x, y, z);
}

void AudioDevice::set_listener_orientation(float fx, float fy, float fz,
                                           float ux, float uy, float uz) {
    if (!m_initialized) return;
    // OpenAL expects a 6-float array: forward xyz, then up xyz.
    float orientation[6] = { fx, fy, fz, ux, uy, uz };
    alListenerfv(AL_ORIENTATION, orientation);
}

void AudioDevice::set_listener_gain(float gain) {
    if (!m_initialized) return;
    alListenerf(AL_GAIN, gain);
}

} // namespace swbf
