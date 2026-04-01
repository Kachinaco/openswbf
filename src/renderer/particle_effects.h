#pragma once

#include "renderer/particle_system.h"

namespace swbf {

// ---------------------------------------------------------------------------
// Built-in particle effect factory functions.
//
// Each function creates and configures one or more ParticleEmitters on the
// given ParticleSystem.  The emitters are set to stop() after their burst
// and will be automatically cleaned up when all particles die.
//
// For continuous effects (smoke, dust), the emitter is returned so the
// caller can stop() it manually when no longer needed.
// ---------------------------------------------------------------------------

namespace effects {

    /// Blaster bolt trail — short-lived bright particles along a projectile
    /// path.  Spawns a continuous stream of tiny bright particles.
    /// Call stop() when the projectile is destroyed.
    ///
    /// @param system   The particle system to add the emitter to.
    /// @param x, y, z  Starting position of the bolt.
    /// @param r, g, b  Bolt color (e.g. 1,0,0 for red, 0,0.5,1 for blue).
    /// @return          Pointer to the emitter (to update position each frame).
    ParticleEmitter* blaster_trail(ParticleSystem& system,
                                   float x, float y, float z,
                                   float r = 1.0f, float g = 0.2f, float b = 0.2f);

    /// Explosion — burst of orange/yellow fire particles plus expanding
    /// smoke ring.  One-shot: emitters stop themselves automatically.
    ///
    /// @param system   The particle system.
    /// @param x, y, z  Center of the explosion.
    /// @param scale    Size multiplier (1.0 = standard grenade).
    void explosion(ParticleSystem& system,
                   float x, float y, float z,
                   float scale = 1.0f);

    /// Smoke plume — slow-rising, expanding, fading gray particles.
    /// Continuous emission until stop() is called.
    ///
    /// @param system   The particle system.
    /// @param x, y, z  Base position of the smoke source.
    /// @return          Pointer to the emitter (call stop() to end).
    ParticleEmitter* smoke(ParticleSystem& system,
                           float x, float y, float z);

    /// Sparks — fast, short-lived bright particles on surface impact.
    /// One-shot burst.
    ///
    /// @param system   The particle system.
    /// @param x, y, z  Impact point.
    /// @param nx, ny, nz  Surface normal (sparks fly away from surface).
    void sparks(ParticleSystem& system,
                float x, float y, float z,
                float nx = 0.0f, float ny = 1.0f, float nz = 0.0f);

    /// Dust — kicked up by footsteps or vehicles near terrain.
    /// Continuous emission until stop() is called.
    ///
    /// @param system   The particle system.
    /// @param x, y, z  Base position (at ground level).
    /// @return          Pointer to the emitter (call stop() to end).
    ParticleEmitter* dust(ParticleSystem& system,
                          float x, float y, float z);

    /// Muzzle flash — brief bright flash at a weapon barrel.
    /// One-shot burst.
    ///
    /// @param system   The particle system.
    /// @param x, y, z  Position of the barrel.
    /// @param dx, dy, dz  Firing direction (flash is biased forward).
    void muzzle_flash(ParticleSystem& system,
                      float x, float y, float z,
                      float dx = 0.0f, float dy = 0.0f, float dz = -1.0f);

} // namespace effects
} // namespace swbf
