#pragma once

#include "core/types.h"
#include "game/entity.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace swbf {

// Forward declaration.
class EntityManager;

/// Configuration for a weapon type (from ODF).
struct WeaponConfig {
    std::string name        = "blaster";
    float damage            = 25.0f;
    float fire_rate         = 0.2f;   // seconds between shots
    float projectile_speed  = 100.0f;
    float max_range         = 200.0f;
};

/// A live projectile in the world.
struct Projectile {
    float    position[3]         = {0, 0, 0};
    float    direction[3]        = {0, 0, -1};
    float    speed               = 100.0f;
    float    damage              = 25.0f;
    float    distance_travelled  = 0.0f;
    float    max_range           = 200.0f;
    EntityId owner               = INVALID_ENTITY;
    bool     active              = true;
};

/// Collision result from a projectile check.
struct ProjectileHit {
    EntityId target      = INVALID_ENTITY;
    float    hit_pos[3]  = {0, 0, 0};
    float    damage      = 0.0f;
};

/// Manages weapons, projectiles, and ammo tracking.
///
/// Unified from Lua API (ODF registration), platform+tests (projectile system),
/// and HUD (ammo tracking) branches.
class WeaponSystem {
public:
    // -- ODF weapon registry (Lua API) ----------------------------------------

    /// Register a weapon ODF by name.
    void register_weapon(const std::string& odf_name);

    /// Check if a weapon is registered.
    bool has_weapon(const std::string& odf_name) const;

    // -- Ammo tracking for HUD ------------------------------------------------

    void set_ammo(int current, int max);
    void set_weapon_name(const std::string& name);

    int  ammo()     const { return m_ammo; }
    int  max_ammo() const { return m_max_ammo; }
    const std::string& weapon_name() const { return m_weapon_name; }

    /// Returns ammo as a fraction in [0, 1].
    float ammo_fraction() const {
        return m_max_ammo > 0
            ? static_cast<float>(m_ammo) / static_cast<float>(m_max_ammo) : 0.0f;
    }

    /// Consume one round.  Returns false if empty.
    bool fire_ammo();

    /// Refill ammo to max.
    void reload();

    // -- Projectile system (platform+tests) -----------------------------------

    /// Fire a projectile.  Returns the index of the created projectile.
    size_t fire(const WeaponConfig& weapon,
                EntityId owner,
                float ox, float oy, float oz,
                float dx, float dy, float dz);

    /// Advance all active projectiles by @p dt seconds.
    /// Returns hits detected this frame.
    std::vector<ProjectileHit> update(float dt,
                                       EntityManager& entities,
                                       float hit_radius = 1.0f);

    /// Number of currently active projectiles.
    size_t active_count() const;

    /// Total projectiles (including spent ones).
    size_t total_count() const;

    /// Access a projectile by index.
    const Projectile* get_projectile(size_t index) const;

    // -- Reset ----------------------------------------------------------------

    /// Remove all projectiles and registered weapons.
    void clear();

private:
    // ODF registry.
    std::unordered_map<std::string, int> m_weapons;

    // Ammo state (for HUD).
    int         m_ammo     = 50;
    int         m_max_ammo = 50;
    std::string m_weapon_name = "DC-15A Blaster";

    // Active projectiles.
    std::vector<Projectile> m_projectiles;
};

} // namespace swbf
