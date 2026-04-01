#pragma once

#include "core/types.h"
#include "game/entity.h"

#include <cstddef>
#include <unordered_map>

namespace swbf {

// Forward declarations.
class EntityManager;
class CommandPostSystem;
class Pathfinder;
class PhysicsWorld;

/// AI finite-state-machine states.
///
/// Each AI soldier is in exactly one state at a time.  Transitions are
/// evaluated every frame based on tactical conditions.
enum class AIState : u8 {
    Idle,          ///< Standing at a post, waiting for orders.
    Moving,        ///< Navigating toward a waypoint or objective.
    Attacking,     ///< Engaging an enemy target.
    Defending,     ///< Holding position at a friendly command post.
    Retreating     ///< Falling back to a friendly post (low health).
};

/// Per-entity AI data.
///
/// Stored separately from Entity so that AI logic doesn't bloat the
/// entity struct for non-AI objects (vehicles, projectiles, etc.).
struct AIData {
    AIState   state          = AIState::Idle;
    EntityId  target         = INVALID_ENTITY;
    float     aggro_range    = 50.0f;    ///< Range to start attacking.
    float     retreat_health = 30.0f;    ///< Health threshold for retreat.
    float     waypoint[3]    = {0, 0, 0};
    float     state_timer    = 0.0f;     ///< Time spent in current state.
    float     fire_cooldown  = 0.0f;
    float     fire_rate      = 0.5f;
    float     move_speed     = 6.0f;
};

/// AI system — drives all AI-controlled entities each frame.
///
/// Unified from all branches:
///   - FSM with state transitions            (pathfinding, platform+tests)
///   - Pathfinding integration (optional)    (pathfinding)
///   - View multiplier for Lua API           (Lua API)
///   - External notification interface       (platform+tests)
class AISystem {
public:
    AISystem() = default;
    ~AISystem() = default;

    /// Initialize with optional subsystem pointers.
    /// Pathfinder / CommandPostSystem / PhysicsWorld are all optional.
    bool init(EntityManager* entities = nullptr,
              CommandPostSystem* command_posts = nullptr,
              Pathfinder* pathfinder = nullptr,
              const PhysicsWorld* physics = nullptr);

    void shutdown();

    /// Tick all AI data.  Call once per frame.
    void update(float dt);

    // -- Brain management -------------------------------------------------------

    /// Register an entity for AI control.
    void add(EntityId id, const AIData& data = AIData{});

    /// Remove an entity from AI control.
    void remove(EntityId id);

    /// Get AI data for an entity, or nullptr.
    AIData*       get_data(EntityId id);
    const AIData* get_data(EntityId id) const;

    /// Get the current AI state for an entity.
    AIState get_state(EntityId id) const;

    /// Set a movement waypoint, transitioning from Idle to Moving.
    void set_waypoint(EntityId id, float x, float y, float z);

    /// Set an attack target.
    void set_target(EntityId id, EntityId target);

    /// Notify about health change (may trigger retreat).
    void notify_health(EntityId id, float current_health);

    /// Notify that a target was lost.
    void notify_target_lost(EntityId id);

    /// Number of entities with AI.
    size_t count() const { return m_data.size(); }

    // -- Lua API configuration --------------------------------------------------

    /// Set the AI view distance multiplier (from SetAIViewMultiplier).
    void set_view_multiplier(float multiplier);
    float view_multiplier() const { return m_view_multiplier; }

    // -- Reset ------------------------------------------------------------------
    void clear();

private:
    std::unordered_map<EntityId, AIData> m_data;

    // Optional subsystem pointers (non-owning).
    EntityManager*      m_entities      = nullptr;
    CommandPostSystem*  m_command_posts = nullptr;
    Pathfinder*         m_pathfinder    = nullptr;
    const PhysicsWorld* m_physics       = nullptr;

    float m_view_multiplier = 1.0f;

    static constexpr float RETREAT_DURATION    = 3.0f;
    static constexpr float WAYPOINT_REACH_DIST = 2.0f;
};

} // namespace swbf
