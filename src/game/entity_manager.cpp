#include "game/entity_manager.h"
#include "core/log.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace swbf {

const std::string EntityManager::s_empty_name;

bool EntityManager::init() {
    LOG_INFO("EntityManager::init");
    return true;
}

void EntityManager::shutdown() {
    clear();
    LOG_INFO("EntityManager::shutdown");
}

// -- Lifetime ---------------------------------------------------------------

EntityId EntityManager::create(const std::string& name,
                               int team,
                               bool is_ai) {
    EntityId id = m_next_id++;

    Entity ent;
    ent.id       = id;
    ent.team     = team;
    ent.name     = name.empty() ? ("entity_" + std::to_string(id)) : name;
    ent.alive    = true;
    ent.is_player = false;

    m_entities[id] = std::move(ent);
    m_transforms[id] = Transform{};

    // Update name lookup.
    if (!name.empty()) {
        m_name_lookup[name] = id;
    }

    LOG_DEBUG("EntityManager: created entity %u (\"%s\", team=%d)",
              id, m_entities[id].name.c_str(), team);
    return id;
}

bool EntityManager::destroy(EntityId id) {
    auto it = m_entities.find(id);
    if (it == m_entities.end()) return false;

    // Clean up name lookup.
    if (!it->second.name.empty()) {
        m_name_lookup.erase(it->second.name);
    }

    m_entities.erase(it);
    m_transforms.erase(id);
    return true;
}

// -- Queries ----------------------------------------------------------------

bool EntityManager::alive(EntityId id) const {
    auto it = m_entities.find(id);
    return it != m_entities.end() && it->second.alive;
}

Entity* EntityManager::get(EntityId id) {
    auto it = m_entities.find(id);
    return (it != m_entities.end()) ? &it->second : nullptr;
}

const Entity* EntityManager::get(EntityId id) const {
    auto it = m_entities.find(id);
    return (it != m_entities.end()) ? &it->second : nullptr;
}

Transform* EntityManager::get_transform(EntityId id) {
    auto it = m_transforms.find(id);
    return (it != m_transforms.end()) ? &it->second : nullptr;
}

const Transform* EntityManager::get_transform(EntityId id) const {
    auto it = m_transforms.find(id);
    return (it != m_transforms.end()) ? &it->second : nullptr;
}

std::vector<EntityId> EntityManager::all_ids() const {
    std::vector<EntityId> ids;
    ids.reserve(m_entities.size());
    for (const auto& [id, ent] : m_entities) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<EntityId> EntityManager::ids_on_team(int team) const {
    std::vector<EntityId> ids;
    for (const auto& [id, ent] : m_entities) {
        if (ent.team == team) ids.push_back(id);
    }
    return ids;
}

std::vector<EntityId> EntityManager::ai_ids() const {
    std::vector<EntityId> ids;
    for (const auto& [id, ent] : m_entities) {
        if (!ent.is_player && ent.alive) ids.push_back(id);
    }
    return ids;
}

// -- Name lookup ------------------------------------------------------------

void EntityManager::set_name(EntityId id, const std::string& name) {
    auto it = m_entities.find(id);
    if (it == m_entities.end()) return;

    // Remove old name mapping.
    if (!it->second.name.empty()) {
        m_name_lookup.erase(it->second.name);
    }

    it->second.name = name;
    if (!name.empty()) {
        m_name_lookup[name] = id;
    }
}

EntityId EntityManager::find_by_name(const std::string& name) const {
    auto it = m_name_lookup.find(name);
    return (it != m_name_lookup.end()) ? it->second : INVALID_ENTITY;
}

const std::string& EntityManager::get_name(EntityId id) const {
    auto it = m_entities.find(id);
    if (it == m_entities.end()) return s_empty_name;
    return it->second.name;
}

// -- Spatial queries --------------------------------------------------------

std::vector<EntityId> EntityManager::find_in_radius(float cx, float cy, float cz,
                                                      float radius) const {
    std::vector<EntityId> result;
    float r2 = radius * radius;

    for (const auto& [id, ent] : m_entities) {
        if (!ent.alive) continue;
        float dx = ent.position[0] - cx;
        float dy = ent.position[1] - cy;
        float dz = ent.position[2] - cz;
        if (dx * dx + dy * dy + dz * dz <= r2) {
            result.push_back(id);
        }
    }
    return result;
}

EntityId EntityManager::find_nearest_on_team(float cx, float cy, float cz,
                                              int team) const {
    EntityId best_id = INVALID_ENTITY;
    float best_dist  = std::numeric_limits<float>::max();

    for (const auto& [id, ent] : m_entities) {
        if (!ent.alive || ent.team != team) continue;
        float dx = ent.position[0] - cx;
        float dy = ent.position[1] - cy;
        float dz = ent.position[2] - cz;
        float d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d < best_dist) {
            best_dist = d;
            best_id   = id;
        }
    }
    return best_id;
}

EntityId EntityManager::find_nearest_enemy(float cx, float cy, float cz,
                                            int my_team) const {
    EntityId best_id = INVALID_ENTITY;
    float best_dist  = std::numeric_limits<float>::max();

    for (const auto& [id, ent] : m_entities) {
        if (!ent.alive || ent.team == my_team || ent.team == 0) continue;
        float dx = ent.position[0] - cx;
        float dy = ent.position[1] - cy;
        float dz = ent.position[2] - cz;
        float d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d < best_dist) {
            best_dist = d;
            best_id   = id;
        }
    }
    return best_id;
}

// -- Position helpers -------------------------------------------------------

bool EntityManager::set_position(EntityId id, float x, float y, float z) {
    auto it = m_entities.find(id);
    if (it == m_entities.end()) return false;

    it->second.position[0] = x;
    it->second.position[1] = y;
    it->second.position[2] = z;

    // Also update the transform.
    auto tr = m_transforms.find(id);
    if (tr != m_transforms.end()) {
        tr->second.position[0] = x;
        tr->second.position[1] = y;
        tr->second.position[2] = z;
    }
    return true;
}

// -- Iteration --------------------------------------------------------------

void EntityManager::for_each(const std::function<void(EntityId, Entity&)>& fn) {
    for (auto& [id, ent] : m_entities) {
        fn(id, ent);
    }
}

// -- Player / HUD helpers ---------------------------------------------------

std::vector<const Entity*> EntityManager::alive_on_team(int team) const {
    std::vector<const Entity*> result;
    for (const auto& [id, e] : m_entities) {
        if (e.team == team && e.alive)
            result.push_back(&e);
    }
    return result;
}

std::vector<const Entity*> EntityManager::on_team(int team) const {
    std::vector<const Entity*> result;
    for (const auto& [id, e] : m_entities) {
        if (e.team == team)
            result.push_back(&e);
    }
    return result;
}

const Entity* EntityManager::local_player() const {
    for (const auto& [id, e] : m_entities) {
        if (e.is_player) return &e;
    }
    return nullptr;
}

Entity* EntityManager::local_player_mut() {
    for (auto& [id, e] : m_entities) {
        if (e.is_player) return &e;
    }
    return nullptr;
}

// -- Reset ------------------------------------------------------------------

void EntityManager::clear() {
    m_entities.clear();
    m_transforms.clear();
    m_name_lookup.clear();
    m_next_id = 1;
    LOG_INFO("EntityManager: cleared all entities");
}

} // namespace swbf
