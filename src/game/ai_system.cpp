#include "game/ai_system.h"
#include "game/entity_manager.h"
#include "game/command_post_system.h"
#include "game/pathfinder.h"
#include "core/log.h"

#include <algorithm>
#include <cmath>

namespace swbf {

bool AISystem::init(EntityManager* entities,
                    CommandPostSystem* command_posts,
                    Pathfinder* pathfinder,
                    const PhysicsWorld* physics) {
    m_entities      = entities;
    m_command_posts = command_posts;
    m_pathfinder    = pathfinder;
    m_physics       = physics;
    LOG_INFO("AISystem::init");
    return true;
}

void AISystem::shutdown() {
    clear();
    m_entities      = nullptr;
    m_command_posts = nullptr;
    m_pathfinder    = nullptr;
    m_physics       = nullptr;
    LOG_INFO("AISystem::shutdown");
}

// -- Brain management -------------------------------------------------------

void AISystem::add(EntityId id, const AIData& data) {
    m_data[id] = data;
}

void AISystem::remove(EntityId id) {
    m_data.erase(id);
}

AIData* AISystem::get_data(EntityId id) {
    auto it = m_data.find(id);
    return (it != m_data.end()) ? &it->second : nullptr;
}

const AIData* AISystem::get_data(EntityId id) const {
    auto it = m_data.find(id);
    return (it != m_data.end()) ? &it->second : nullptr;
}

AIState AISystem::get_state(EntityId id) const {
    auto it = m_data.find(id);
    if (it == m_data.end()) return AIState::Idle;
    return it->second.state;
}

void AISystem::set_waypoint(EntityId id, float x, float y, float z) {
    auto it = m_data.find(id);
    if (it == m_data.end()) return;

    AIData& d = it->second;
    d.waypoint[0] = x;
    d.waypoint[1] = y;
    d.waypoint[2] = z;

    if (d.state == AIState::Idle) {
        d.state = AIState::Moving;
        d.state_timer = 0.0f;
    }
}

void AISystem::set_target(EntityId id, EntityId target) {
    auto it = m_data.find(id);
    if (it == m_data.end()) return;

    AIData& d = it->second;
    d.target = target;

    if (d.state == AIState::Idle || d.state == AIState::Moving) {
        d.state = AIState::Attacking;
        d.state_timer = 0.0f;
    }
}

void AISystem::notify_health(EntityId id, float current_health) {
    auto it = m_data.find(id);
    if (it == m_data.end()) return;

    AIData& d = it->second;
    if (d.state == AIState::Attacking && current_health <= d.retreat_health) {
        d.state = AIState::Retreating;
        d.state_timer = 0.0f;
    }
}

void AISystem::notify_target_lost(EntityId id) {
    auto it = m_data.find(id);
    if (it == m_data.end()) return;

    AIData& d = it->second;
    if (d.state == AIState::Attacking) {
        d.state = AIState::Idle;
        d.target = INVALID_ENTITY;
        d.state_timer = 0.0f;
    }
}

// -- Update -----------------------------------------------------------------

void AISystem::update(float dt) {
    for (auto& [id, d] : m_data) {
        d.state_timer += dt;

        switch (d.state) {
        case AIState::Retreating:
            if (d.state_timer >= RETREAT_DURATION) {
                d.state = AIState::Idle;
                d.target = INVALID_ENTITY;
                d.state_timer = 0.0f;
            }
            break;

        case AIState::Moving:
            // Check if waypoint reached.
            if (m_entities) {
                Entity* ent = m_entities->get(id);
                if (ent) {
                    float dx = ent->position[0] - d.waypoint[0];
                    float dz = ent->position[2] - d.waypoint[2];
                    if (std::sqrt(dx * dx + dz * dz) < WAYPOINT_REACH_DIST) {
                        d.state = AIState::Idle;
                        d.state_timer = 0.0f;
                    }
                }
            }
            break;

        case AIState::Idle:
        case AIState::Attacking:
        case AIState::Defending:
            // These states are driven externally or by higher-level logic.
            break;
        }
    }
}

// -- Lua API ----------------------------------------------------------------

void AISystem::set_view_multiplier(float multiplier) {
    m_view_multiplier = multiplier;
    LOG_INFO("AISystem: view multiplier set to %.2f",
             static_cast<double>(multiplier));
}

// -- Reset ------------------------------------------------------------------

void AISystem::clear() {
    m_data.clear();
    m_view_multiplier = 1.0f;
    LOG_INFO("AISystem: cleared");
}

} // namespace swbf
