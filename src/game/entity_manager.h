#pragma once

#include "core/types.h"
#include "game/components/transform.h"
#include "game/entity.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace swbf {

/// Unique team identifier.
using TeamId = u8;

/// Central registry for all game entities.
///
/// Combines features needed by every subsystem:
///   - Spawn / destroy / lookup                 (all branches)
///   - Name-based lookup                        (Lua API, platform+tests)
///   - Spatial queries (radius, nearest)         (pathfinding AI)
///   - Team filtering                           (pathfinding, HUD)
///   - Transform access                         (platform+tests)
///   - Iteration (for_each)                     (platform+tests, HUD)
///   - Player / scoreboard helpers              (HUD)
class EntityManager {
public:
    bool init();
    void shutdown();

    // -- Lifetime ---------------------------------------------------------------

    /// Create a new entity and return its id.
    EntityId create(const std::string& name = "",
                    int team = 0,
                    bool is_ai = false);

    /// Remove an entity by id.  Returns true if it existed.
    bool destroy(EntityId id);

    // -- Queries ----------------------------------------------------------------

    /// Check whether an entity is currently alive.
    bool alive(EntityId id) const;

    /// Get a mutable pointer to an entity.  Returns nullptr if not found.
    Entity* get(EntityId id);
    const Entity* get(EntityId id) const;

    /// Get the transform for an entity, or nullptr.
    Transform* get_transform(EntityId id);
    const Transform* get_transform(EntityId id) const;

    /// Get all entity ids.
    std::vector<EntityId> all_ids() const;

    /// Get all entity ids on a given team.
    std::vector<EntityId> ids_on_team(int team) const;

    /// Get all AI-controlled entity ids.
    std::vector<EntityId> ai_ids() const;

    /// Total entity count.
    size_t count() const { return m_entities.size(); }

    // -- Name lookup ------------------------------------------------------------

    /// Attach or change the name of an entity.
    void set_name(EntityId id, const std::string& name);

    /// Look up an entity by name.  Returns INVALID_ENTITY if not found.
    EntityId find_by_name(const std::string& name) const;

    /// Get the name of an entity.  Returns "" if not found.
    const std::string& get_name(EntityId id) const;

    // -- Spatial queries --------------------------------------------------------

    /// Find all living entities within @p radius of @p center.
    std::vector<EntityId> find_in_radius(float cx, float cy, float cz,
                                          float radius) const;

    /// Find the nearest living entity to @p center that is on @p team.
    /// Returns INVALID_ENTITY if none found.
    EntityId find_nearest_on_team(float cx, float cy, float cz,
                                  int team) const;

    /// Find the nearest living enemy (different team) relative to @p my_team.
    EntityId find_nearest_enemy(float cx, float cy, float cz,
                                int my_team) const;

    // -- Position helpers -------------------------------------------------------

    /// Set an entity's world-space position.  Returns false if entity is dead.
    bool set_position(EntityId id, float x, float y, float z);

    // -- Iteration --------------------------------------------------------------

    /// Iterate all living entities.  Callback receives (id, entity&).
    void for_each(const std::function<void(EntityId, Entity&)>& fn);

    // -- Player / HUD helpers ---------------------------------------------------

    /// Get all living entities on the given team.
    std::vector<const Entity*> alive_on_team(int team) const;

    /// Get all entities on the given team (alive or dead) for scoreboard.
    std::vector<const Entity*> on_team(int team) const;

    /// Find the local player entity, or nullptr.
    const Entity* local_player() const;
    Entity* local_player_mut();

    // -- Reset ------------------------------------------------------------------

    /// Clear all entities — call between levels.
    void clear();

private:
    std::unordered_map<EntityId, Entity>       m_entities;
    std::unordered_map<EntityId, Transform>    m_transforms;
    std::unordered_map<std::string, EntityId>  m_name_lookup;
    EntityId m_next_id = 1;  // 0 is reserved as "invalid".

    static const std::string s_empty_name;
};

} // namespace swbf
