#pragma once

#include "core/types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace swbf {

// ---------------------------------------------------------------------------
// Particle — a single particle in the simulation.
// ---------------------------------------------------------------------------

struct Particle {
    // Position in world space.
    float px, py, pz;

    // Velocity in world units per second.
    float vx, vy, vz;

    // Current age and maximum lifetime (seconds).
    float age;
    float lifetime;

    // Start and end size (interpolated linearly over lifetime).
    float size_start;
    float size_end;

    // Start and end color/alpha (interpolated linearly over lifetime).
    float r0, g0, b0, a0;   // color at birth
    float r1, g1, b1, a1;   // color at death

    // Is this particle slot alive?
    bool alive;
};

// ---------------------------------------------------------------------------
// BlendMode — how particles are composited.
// ---------------------------------------------------------------------------

enum class ParticleBlendMode : u8 {
    Alpha,     // Standard alpha blending (smoke, dust)
    Additive   // Additive blending (fire, sparks, blaster bolts)
};

// ---------------------------------------------------------------------------
// ParticleEmitter — spawns and manages a pool of particles.
//
// Fixed-size pool: dead particles are recycled.  The emitter tracks its own
// position, emission rate, and per-particle randomisation ranges.
// ---------------------------------------------------------------------------

class ParticleEmitter {
public:
    // Maximum particles this emitter can have alive at once.
    static constexpr u32 MAX_PARTICLES = 256;

    ParticleEmitter();

    // ---- Configuration (set before first update) -------------------------

    /// Set the emitter position in world space.
    void set_position(float x, float y, float z);
    void get_position(float& x, float& y, float& z) const;

    /// Particles spawned per second. 0 = manual burst only.
    void set_emission_rate(float particles_per_second);

    /// Range of lifetimes for spawned particles (seconds).
    void set_lifetime_range(float min_life, float max_life);

    /// Initial velocity range — each component randomised independently.
    void set_velocity_range(float vx_min, float vy_min, float vz_min,
                            float vx_max, float vy_max, float vz_max);

    /// Spawn offset range — particles appear randomly within this box
    /// centered on the emitter position.
    void set_spawn_offset(float ox, float oy, float oz);

    /// Size over lifetime.
    void set_size_range(float start_min, float start_max,
                        float end_min, float end_max);

    /// Color gradient over lifetime.
    void set_color_start(float r, float g, float b, float a);
    void set_color_end(float r, float g, float b, float a);

    /// Gravity acceleration applied each frame (world Y axis).
    void set_gravity(float g);

    /// Drag coefficient — velocity *= (1 - drag * dt) each frame.
    void set_drag(float d);

    /// Blend mode for rendering.
    void set_blend_mode(ParticleBlendMode mode);
    ParticleBlendMode blend_mode() const { return m_blend_mode; }

    // ---- Control ---------------------------------------------------------

    /// Immediately spawn a burst of particles.
    void burst(u32 count);

    /// Mark the emitter as finished — it will stop spawning and die when
    /// all particles expire.
    void stop();

    /// Is the emitter still active (has live particles or is still emitting)?
    bool alive() const;

    // ---- Simulation ------------------------------------------------------

    /// Advance simulation by dt seconds.
    void update(float dt);

    // ---- Data access (for renderer) --------------------------------------

    const Particle* particles() const { return m_pool; }
    u32 max_particles() const { return MAX_PARTICLES; }
    u32 active_count() const { return m_active_count; }

private:
    void spawn_particle();
    float rand_range(float lo, float hi) const;

    // Particle pool (fixed-size array, recycled).
    Particle m_pool[MAX_PARTICLES];
    u32 m_active_count = 0;

    // Emitter position.
    float m_x = 0.0f, m_y = 0.0f, m_z = 0.0f;

    // Emission rate and accumulator.
    float m_rate       = 0.0f;   // particles per second
    float m_accumulator = 0.0f;
    bool  m_emitting    = true;

    // Per-particle randomisation ranges.
    float m_life_min = 0.5f, m_life_max = 1.5f;

    float m_vx_min = -1.0f, m_vy_min =  0.0f, m_vz_min = -1.0f;
    float m_vx_max =  1.0f, m_vy_max =  3.0f, m_vz_max =  1.0f;

    float m_offset_x = 0.0f, m_offset_y = 0.0f, m_offset_z = 0.0f;

    float m_size_start_min = 0.3f, m_size_start_max = 0.5f;
    float m_size_end_min   = 0.0f, m_size_end_max   = 0.1f;

    float m_r0 = 1.0f, m_g0 = 1.0f, m_b0 = 1.0f, m_a0 = 1.0f;
    float m_r1 = 1.0f, m_g1 = 1.0f, m_b1 = 1.0f, m_a1 = 0.0f;

    float m_gravity = -9.8f;
    float m_drag    = 0.0f;

    ParticleBlendMode m_blend_mode = ParticleBlendMode::Additive;

    // Simple RNG state (xorshift32, seeded from emitter address).
    mutable u32 m_rng_state;
    u32 xorshift32() const;
};

// ---------------------------------------------------------------------------
// ParticleSystem — owns multiple emitters, updates them, culls dead ones.
// ---------------------------------------------------------------------------

class ParticleSystem {
public:
    ParticleSystem() = default;

    /// Add an emitter. Returns a raw pointer (owned by the system).
    /// The pointer remains valid until the emitter dies and is removed
    /// during the next update().
    ParticleEmitter* add_emitter();

    /// Update all emitters by dt seconds. Removes dead emitters.
    void update(float dt);

    /// Access all live emitters (for the renderer).
    const std::vector<ParticleEmitter*>& emitters() const { return m_emitters; }

    /// Total number of active particles across all emitters.
    u32 total_particle_count() const;

    /// Remove all emitters immediately.
    void clear();

    ~ParticleSystem();

    // Non-copyable.
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

private:
    std::vector<ParticleEmitter*> m_emitters;
};

} // namespace swbf
