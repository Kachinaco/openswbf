#pragma once

#include "core/types.h"
#include "game/entity.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace swbf {

// Forward declarations.
class EntityManager;
class CommandPostSystem;

/// A unit class definition — maps an ODF class name to a team.
struct UnitClassDef {
    std::string odf_name;    // e.g. "rep_inf_ep3_rifleman"
    int         min_count;   // minimum number of this class to keep spawned
    int         max_count;   // maximum allowed
};

/// Per-team configuration set by mission scripts.
struct TeamConfig {
    int         team_index = 0;
    std::string name;                       // e.g. "Republic", "CIS"
    std::string hero_class;                 // hero unit ODF
    int         unit_count = 32;            // reinforcement count
    std::vector<UnitClassDef> unit_classes; // available unit classes
};

/// Unit class selection for SWBF.
enum class UnitClass {
    Soldier,    // Standard infantry
    Heavy,      // Rocket launcher / heavy weapons
    Sniper,     // Long-range marksman
    Engineer,   // Repair / utility class
    Special     // Faction-specific special unit
};

/// A spawn point in the world.
struct SpawnPoint {
    uint32_t    id          = 0;
    std::string name;
    float       position[3] = {0, 0, 0};
    float       rotation    = 0.0f;    // yaw in radians
    int         owner       = 0;       // team
    uint32_t    linked_post = 0;       // command post ID, 0 = none
    bool        active      = true;
};

/// A pending respawn request.
struct RespawnRequest {
    EntityId  entity     = INVALID_ENTITY;
    int       team       = 0;
    UnitClass unit_class = UnitClass::Soldier;
    float     timer      = 0.0f;
    uint32_t  spawn_point = 0;   // preferred spawn point ID, 0 = auto
};

/// SpawnSystem — manages team configuration, spawn points, and respawn timers.
///
/// Unified from Lua API (team config) and platform+tests (spawn points) branches.
class SpawnSystem {
public:
    SpawnSystem() = default;
    ~SpawnSystem() = default;

    // -- Team configuration (Lua API) -----------------------------------------

    /// Configure the human-readable name for a team (1-indexed).
    void set_team_name(int team, const std::string& name);

    /// Add a unit class to a team.
    void add_unit_class(int team, const std::string& odf_name,
                        int min_count, int max_count);

    /// Set the hero class for a team.
    void set_hero_class(int team, const std::string& odf_name);

    /// Set the reinforcement count for a team.
    void set_unit_count(int team, int count);

    /// Set the spawn delay in seconds.
    void set_spawn_delay(float seconds);
    float spawn_delay() const { return m_spawn_delay; }

    /// Enable or disable single-player hero rules.
    void set_sp_hero_rules(bool enabled);
    bool sp_hero_rules() const { return m_sp_hero_rules; }

    /// Access team config.  Creates the entry if it doesn't exist.
    TeamConfig& team(int index);

    /// Read-only access.  Returns nullptr if team doesn't exist.
    const TeamConfig* team(int index) const;

    // -- Spawn points (platform+tests) ----------------------------------------

    /// Register a spawn point.  Returns its ID.
    uint32_t add_spawn_point(const std::string& name,
                              float x, float y, float z,
                              int owner = 0,
                              uint32_t linked_post = 0);

    /// Remove a spawn point.
    void remove_spawn_point(uint32_t id);

    /// Get a spawn point by ID, or nullptr.
    const SpawnPoint* get_spawn_point(uint32_t id) const;

    /// Get all spawn points available to a team.
    std::vector<const SpawnPoint*> get_team_spawns(int team) const;

    /// Update spawn point ownership based on command post state.
    void sync_with_posts(const CommandPostSystem& cps);

    /// Request a respawn.
    void request_respawn(EntityId entity, int team,
                         UnitClass unit_class,
                         float respawn_delay = 5.0f,
                         uint32_t preferred_spawn = 0);

    /// Advance respawn timers.  Returns entities that spawned this frame.
    std::vector<EntityId> update(float dt, EntityManager& entities);

    /// Number of registered spawn points.
    size_t spawn_point_count() const;

    /// Number of pending respawn requests.
    size_t pending_respawn_count() const;

    static constexpr float DEFAULT_RESPAWN_DELAY = 5.0f;

    // -- Reset ----------------------------------------------------------------
    void clear();

private:
    // Team configs (Lua API).
    std::unordered_map<int, TeamConfig> m_teams;
    float m_spawn_delay   = 5.0f;
    bool  m_sp_hero_rules = false;

    // Spawn points (platform+tests).
    uint32_t m_next_sp_id = 1;
    std::unordered_map<uint32_t, SpawnPoint> m_spawn_points;
    std::vector<RespawnRequest> m_pending;
};

} // namespace swbf
