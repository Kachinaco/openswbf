#pragma once

#include "core/types.h"
#include "game/entity.h"

#include <functional>
#include <unordered_map>

namespace swbf {

/// Health component data for a single entity.
struct HealthData {
    float current = 100.0f;
    float max     = 100.0f;
    bool  dead    = false;
};

/// Callback signature for death events.
using DeathCallback = std::function<void(EntityId)>;

/// Manages entity health: damage, healing, death detection, and callbacks.
///
/// Unified from platform+tests (full ECS-style system with per-entity tracking)
/// and HUD (simple player health display) branches.
class HealthSystem {
public:
    // -- Per-entity health (platform+tests) -----------------------------------

    /// Register an entity with the health system.
    void add(EntityId id, float max_health);

    /// Remove an entity from the health system.
    void remove(EntityId id);

    /// Apply damage.  Returns actual damage dealt.  Fires death callback.
    float damage(EntityId id, float amount);

    /// Heal an entity.  Returns actual healing applied.
    float heal(EntityId id, float amount);

    /// Get health of a tracked entity, or -1 if not tracked.
    float get_health(EntityId id) const;

    /// Get max health of a tracked entity, or -1 if not tracked.
    float get_max_health(EntityId id) const;

    /// Check whether a tracked entity is dead.
    bool is_dead(EntityId id) const;

    /// Set a callback for when any entity dies.
    void set_death_callback(DeathCallback cb);

    /// Number of entities tracked.
    size_t count() const;

    // -- Simple player health for HUD -----------------------------------------

    void set_health(float hp)     { m_player_health = hp; }
    void set_max_health(float hp) { m_player_max_health = hp; }

    float health()     const { return m_player_health; }
    float max_health() const { return m_player_max_health; }

    /// Returns health as a fraction in [0, 1].
    float health_fraction() const {
        return m_player_max_health > 0.0f
            ? m_player_health / m_player_max_health : 0.0f;
    }

    /// Apply damage to player.  Clamps to zero.
    void take_damage(float amount);

    /// Heal player up to max.
    void heal_player(float amount);

    bool is_alive() const { return m_player_health > 0.0f; }

    // -- Reset ----------------------------------------------------------------
    void clear();

private:
    // Per-entity tracking.
    std::unordered_map<EntityId, HealthData> m_health;
    DeathCallback m_death_callback;

    // Simple player health for HUD.
    float m_player_health     = 100.0f;
    float m_player_max_health = 100.0f;
};

} // namespace swbf
