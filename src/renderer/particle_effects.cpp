#include "renderer/particle_effects.h"

namespace swbf {
namespace effects {

// ===========================================================================
// Blaster bolt trail
// ===========================================================================

ParticleEmitter* blaster_trail(ParticleSystem& system,
                                float x, float y, float z,
                                float r, float g, float b) {
    auto* e = system.add_emitter();

    e->set_position(x, y, z);
    e->set_emission_rate(120.0f);           // rapid stream
    e->set_lifetime_range(0.05f, 0.15f);    // very short-lived

    // Particles drift slightly outward from the bolt path.
    e->set_velocity_range(-0.5f, -0.5f, -0.5f,
                           0.5f,  0.5f,  0.5f);
    e->set_spawn_offset(0.05f, 0.05f, 0.05f);

    // Tiny bright particles.
    e->set_size_range(0.15f, 0.25f, 0.0f, 0.05f);

    // Bright core color fading to transparent.
    e->set_color_start(r, g, b, 1.0f);
    e->set_color_end(r * 0.5f, g * 0.5f, b * 0.5f, 0.0f);

    e->set_gravity(0.0f);   // no gravity on bolt trail
    e->set_drag(0.0f);
    e->set_blend_mode(ParticleBlendMode::Additive);

    return e;
}

// ===========================================================================
// Explosion
// ===========================================================================

void explosion(ParticleSystem& system,
               float x, float y, float z,
               float scale) {
    // --- Fire burst ---
    {
        auto* e = system.add_emitter();
        e->set_position(x, y, z);
        e->set_emission_rate(0.0f);  // burst only
        e->set_lifetime_range(0.3f * scale, 0.8f * scale);

        float spd = 8.0f * scale;
        e->set_velocity_range(-spd, -spd * 0.3f, -spd,
                               spd,  spd * 1.5f,  spd);
        e->set_spawn_offset(0.3f * scale, 0.3f * scale, 0.3f * scale);

        e->set_size_range(0.8f * scale, 1.5f * scale,
                          2.0f * scale, 3.0f * scale);

        // Orange/yellow fire fading to dark red.
        e->set_color_start(1.0f, 0.7f, 0.2f, 1.0f);
        e->set_color_end(0.8f, 0.15f, 0.0f, 0.0f);

        e->set_gravity(-2.0f);  // slight upward drift (negative = up in our convention... but gravity is applied as += gravity*dt to vy)
        // Actually gravity field adds to vy, so negative means downward.
        // For explosion fire, use positive to push up.
        e->set_gravity(2.0f);
        e->set_drag(1.5f);
        e->set_blend_mode(ParticleBlendMode::Additive);

        e->burst(static_cast<u32>(40 * scale));
        e->stop();  // no further emission
    }

    // --- Smoke ring ---
    {
        auto* e = system.add_emitter();
        e->set_position(x, y, z);
        e->set_emission_rate(0.0f);
        e->set_lifetime_range(1.0f * scale, 2.5f * scale);

        float spd = 4.0f * scale;
        e->set_velocity_range(-spd, 0.5f, -spd,
                               spd, 3.0f * scale, spd);
        e->set_spawn_offset(0.5f * scale, 0.2f * scale, 0.5f * scale);

        e->set_size_range(1.0f * scale, 1.5f * scale,
                          4.0f * scale, 6.0f * scale);

        // Dark gray smoke fading out.
        e->set_color_start(0.3f, 0.3f, 0.3f, 0.6f);
        e->set_color_end(0.15f, 0.15f, 0.15f, 0.0f);

        e->set_gravity(1.0f);   // rises
        e->set_drag(2.0f);
        e->set_blend_mode(ParticleBlendMode::Alpha);

        e->burst(static_cast<u32>(25 * scale));
        e->stop();
    }
}

// ===========================================================================
// Smoke
// ===========================================================================

ParticleEmitter* smoke(ParticleSystem& system,
                        float x, float y, float z) {
    auto* e = system.add_emitter();

    e->set_position(x, y, z);
    e->set_emission_rate(15.0f);
    e->set_lifetime_range(2.0f, 4.0f);

    e->set_velocity_range(-0.3f, 0.5f, -0.3f,
                           0.3f, 2.0f,  0.3f);
    e->set_spawn_offset(0.3f, 0.1f, 0.3f);

    e->set_size_range(0.5f, 0.8f, 2.5f, 4.0f);

    // Gray, semi-transparent, fading out.
    e->set_color_start(0.4f, 0.4f, 0.4f, 0.5f);
    e->set_color_end(0.2f, 0.2f, 0.2f, 0.0f);

    e->set_gravity(0.5f);   // slow rise
    e->set_drag(0.8f);
    e->set_blend_mode(ParticleBlendMode::Alpha);

    return e;
}

// ===========================================================================
// Sparks
// ===========================================================================

void sparks(ParticleSystem& system,
            float x, float y, float z,
            float nx, float ny, float nz) {
    auto* e = system.add_emitter();

    e->set_position(x, y, z);
    e->set_emission_rate(0.0f);  // burst only
    e->set_lifetime_range(0.1f, 0.4f);

    // Sparks fly away from the surface normal with spread.
    float speed = 10.0f;
    float spread = 5.0f;
    e->set_velocity_range(nx * speed - spread, ny * speed - spread, nz * speed - spread,
                           nx * speed + spread, ny * speed + spread, nz * speed + spread);
    e->set_spawn_offset(0.1f, 0.1f, 0.1f);

    // Tiny bright particles.
    e->set_size_range(0.05f, 0.12f, 0.0f, 0.02f);

    // Bright yellow/white sparks.
    e->set_color_start(1.0f, 0.9f, 0.5f, 1.0f);
    e->set_color_end(1.0f, 0.4f, 0.0f, 0.0f);

    e->set_gravity(-9.8f);   // sparks fall
    e->set_drag(0.5f);
    e->set_blend_mode(ParticleBlendMode::Additive);

    e->burst(20);
    e->stop();
}

// ===========================================================================
// Dust
// ===========================================================================

ParticleEmitter* dust(ParticleSystem& system,
                       float x, float y, float z) {
    auto* e = system.add_emitter();

    e->set_position(x, y, z);
    e->set_emission_rate(8.0f);
    e->set_lifetime_range(0.8f, 1.5f);

    e->set_velocity_range(-1.0f, 0.2f, -1.0f,
                           1.0f, 1.5f,  1.0f);
    e->set_spawn_offset(0.5f, 0.0f, 0.5f);

    e->set_size_range(0.3f, 0.5f, 1.0f, 1.5f);

    // Tan/brown dust color.
    e->set_color_start(0.6f, 0.5f, 0.35f, 0.4f);
    e->set_color_end(0.5f, 0.45f, 0.3f, 0.0f);

    e->set_gravity(0.3f);   // slight rise
    e->set_drag(1.5f);
    e->set_blend_mode(ParticleBlendMode::Alpha);

    return e;
}

// ===========================================================================
// Muzzle flash
// ===========================================================================

void muzzle_flash(ParticleSystem& system,
                   float x, float y, float z,
                   float dx, float dy, float dz) {
    auto* e = system.add_emitter();

    e->set_position(x, y, z);
    e->set_emission_rate(0.0f);  // burst only
    e->set_lifetime_range(0.03f, 0.08f);

    // Flash biased in the firing direction.
    float speed = 3.0f;
    float spread = 2.0f;
    e->set_velocity_range(dx * speed - spread, dy * speed - spread, dz * speed - spread,
                           dx * speed + spread, dy * speed + spread, dz * speed + spread);
    e->set_spawn_offset(0.05f, 0.05f, 0.05f);

    // Bright flash.
    e->set_size_range(0.3f, 0.6f, 0.1f, 0.2f);

    // White-yellow flash.
    e->set_color_start(1.0f, 0.95f, 0.7f, 1.0f);
    e->set_color_end(1.0f, 0.6f, 0.1f, 0.0f);

    e->set_gravity(0.0f);
    e->set_drag(0.0f);
    e->set_blend_mode(ParticleBlendMode::Additive);

    e->burst(12);
    e->stop();
}

} // namespace effects
} // namespace swbf
