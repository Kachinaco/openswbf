#include "renderer/particle_system.h"
#include "core/log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace swbf {

// ===========================================================================
// ParticleEmitter
// ===========================================================================

ParticleEmitter::ParticleEmitter() {
    // Seed RNG from the address of this object — gives each emitter
    // a different sequence without requiring an external seed.
    m_rng_state = static_cast<u32>(reinterpret_cast<uintptr_t>(this) & 0xFFFFFFFFu);
    if (m_rng_state == 0) m_rng_state = 1;

    // Zero-initialise the particle pool.
    std::memset(m_pool, 0, sizeof(m_pool));
}

// ---- Configuration -------------------------------------------------------

void ParticleEmitter::set_position(float x, float y, float z) {
    m_x = x; m_y = y; m_z = z;
}

void ParticleEmitter::get_position(float& x, float& y, float& z) const {
    x = m_x; y = m_y; z = m_z;
}

void ParticleEmitter::set_emission_rate(float particles_per_second) {
    m_rate = particles_per_second;
}

void ParticleEmitter::set_lifetime_range(float min_life, float max_life) {
    m_life_min = min_life;
    m_life_max = max_life;
}

void ParticleEmitter::set_velocity_range(float vx_min, float vy_min, float vz_min,
                                          float vx_max, float vy_max, float vz_max) {
    m_vx_min = vx_min; m_vy_min = vy_min; m_vz_min = vz_min;
    m_vx_max = vx_max; m_vy_max = vy_max; m_vz_max = vz_max;
}

void ParticleEmitter::set_spawn_offset(float ox, float oy, float oz) {
    m_offset_x = ox; m_offset_y = oy; m_offset_z = oz;
}

void ParticleEmitter::set_size_range(float start_min, float start_max,
                                      float end_min, float end_max) {
    m_size_start_min = start_min; m_size_start_max = start_max;
    m_size_end_min   = end_min;   m_size_end_max   = end_max;
}

void ParticleEmitter::set_color_start(float r, float g, float b, float a) {
    m_r0 = r; m_g0 = g; m_b0 = b; m_a0 = a;
}

void ParticleEmitter::set_color_end(float r, float g, float b, float a) {
    m_r1 = r; m_g1 = g; m_b1 = b; m_a1 = a;
}

void ParticleEmitter::set_gravity(float g) { m_gravity = g; }
void ParticleEmitter::set_drag(float d)    { m_drag = d; }

void ParticleEmitter::set_blend_mode(ParticleBlendMode mode) {
    m_blend_mode = mode;
}

// ---- Control -------------------------------------------------------------

void ParticleEmitter::burst(u32 count) {
    for (u32 i = 0; i < count; ++i) {
        spawn_particle();
    }
}

void ParticleEmitter::stop() {
    m_emitting = false;
}

bool ParticleEmitter::alive() const {
    return m_emitting || m_active_count > 0;
}

// ---- Simulation ----------------------------------------------------------

void ParticleEmitter::update(float dt) {
    // 1. Spawn new particles based on emission rate.
    if (m_emitting && m_rate > 0.0f) {
        m_accumulator += m_rate * dt;
        while (m_accumulator >= 1.0f) {
            spawn_particle();
            m_accumulator -= 1.0f;
        }
    }

    // 2. Update all alive particles.
    m_active_count = 0;
    for (u32 i = 0; i < MAX_PARTICLES; ++i) {
        Particle& p = m_pool[i];
        if (!p.alive) continue;

        // Age the particle.
        p.age += dt;
        if (p.age >= p.lifetime) {
            p.alive = false;
            continue;
        }

        // Apply gravity (world Y axis).
        p.vy += m_gravity * dt;

        // Apply drag.
        if (m_drag > 0.0f) {
            float factor = 1.0f - m_drag * dt;
            if (factor < 0.0f) factor = 0.0f;
            p.vx *= factor;
            p.vy *= factor;
            p.vz *= factor;
        }

        // Integrate position.
        p.px += p.vx * dt;
        p.py += p.vy * dt;
        p.pz += p.vz * dt;

        ++m_active_count;
    }
}

// ---- Internals -----------------------------------------------------------

void ParticleEmitter::spawn_particle() {
    // Find a dead slot.
    for (u32 i = 0; i < MAX_PARTICLES; ++i) {
        if (!m_pool[i].alive) {
            Particle& p = m_pool[i];
            p.alive = true;
            p.age   = 0.0f;

            // Randomise lifetime.
            p.lifetime = rand_range(m_life_min, m_life_max);

            // Position with random offset.
            p.px = m_x + rand_range(-m_offset_x, m_offset_x);
            p.py = m_y + rand_range(-m_offset_y, m_offset_y);
            p.pz = m_z + rand_range(-m_offset_z, m_offset_z);

            // Random velocity.
            p.vx = rand_range(m_vx_min, m_vx_max);
            p.vy = rand_range(m_vy_min, m_vy_max);
            p.vz = rand_range(m_vz_min, m_vz_max);

            // Size over life.
            p.size_start = rand_range(m_size_start_min, m_size_start_max);
            p.size_end   = rand_range(m_size_end_min, m_size_end_max);

            // Color at birth and death.
            p.r0 = m_r0; p.g0 = m_g0; p.b0 = m_b0; p.a0 = m_a0;
            p.r1 = m_r1; p.g1 = m_g1; p.b1 = m_b1; p.a1 = m_a1;

            ++m_active_count;
            return;
        }
    }
    // Pool full — particle is dropped.
}

u32 ParticleEmitter::xorshift32() const {
    u32 x = m_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    m_rng_state = x;
    return x;
}

float ParticleEmitter::rand_range(float lo, float hi) const {
    if (lo >= hi) return lo;
    float t = static_cast<float>(xorshift32() & 0x00FFFFFFu) / 16777215.0f;
    return lo + (hi - lo) * t;
}

// ===========================================================================
// ParticleSystem
// ===========================================================================

ParticleSystem::~ParticleSystem() {
    clear();
}

ParticleEmitter* ParticleSystem::add_emitter() {
    auto* e = new ParticleEmitter();
    m_emitters.push_back(e);
    return e;
}

void ParticleSystem::update(float dt) {
    // Update all emitters, then remove dead ones.
    for (auto* e : m_emitters) {
        e->update(dt);
    }

    // Erase-remove dead emitters.
    auto it = std::remove_if(m_emitters.begin(), m_emitters.end(),
        [](const ParticleEmitter* e) {
            if (!e->alive()) {
                delete e;
                return true;
            }
            return false;
        });
    m_emitters.erase(it, m_emitters.end());
}

u32 ParticleSystem::total_particle_count() const {
    u32 total = 0;
    for (const auto* e : m_emitters) {
        total += e->active_count();
    }
    return total;
}

void ParticleSystem::clear() {
    for (auto* e : m_emitters) {
        delete e;
    }
    m_emitters.clear();
}

} // namespace swbf
