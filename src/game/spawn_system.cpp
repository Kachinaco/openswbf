#include "game/spawn_system.h"
#include "game/entity_manager.h"
#include "game/command_post_system.h"
#include "core/log.h"

#include <algorithm>

namespace swbf {

// -- Team configuration -----------------------------------------------------

void SpawnSystem::set_team_name(int team, const std::string& name) {
    auto& t = this->team(team);
    t.name = name;
    LOG_INFO("SpawnSystem: team %d name set to \"%s\"", team, name.c_str());
}

void SpawnSystem::add_unit_class(int team, const std::string& odf_name,
                                  int min_count, int max_count) {
    auto& t = this->team(team);
    t.unit_classes.push_back({odf_name, min_count, max_count});
    LOG_INFO("SpawnSystem: team %d added unit class \"%s\" (min=%d, max=%d)",
             team, odf_name.c_str(), min_count, max_count);
}

void SpawnSystem::set_hero_class(int team, const std::string& odf_name) {
    auto& t = this->team(team);
    t.hero_class = odf_name;
    LOG_INFO("SpawnSystem: team %d hero class set to \"%s\"",
             team, odf_name.c_str());
}

void SpawnSystem::set_unit_count(int team, int count) {
    auto& t = this->team(team);
    t.unit_count = count;
    LOG_INFO("SpawnSystem: team %d unit count set to %d", team, count);
}

void SpawnSystem::set_spawn_delay(float seconds) {
    m_spawn_delay = seconds;
    LOG_INFO("SpawnSystem: spawn delay set to %.1f seconds",
             static_cast<double>(seconds));
}

void SpawnSystem::set_sp_hero_rules(bool enabled) {
    m_sp_hero_rules = enabled;
    LOG_INFO("SpawnSystem: SP hero rules %s", enabled ? "enabled" : "disabled");
}

TeamConfig& SpawnSystem::team(int index) {
    auto it = m_teams.find(index);
    if (it == m_teams.end()) {
        TeamConfig tc;
        tc.team_index = index;
        m_teams[index] = tc;
        return m_teams[index];
    }
    return it->second;
}

const TeamConfig* SpawnSystem::team(int index) const {
    auto it = m_teams.find(index);
    return (it != m_teams.end()) ? &it->second : nullptr;
}

// -- Spawn points -----------------------------------------------------------

uint32_t SpawnSystem::add_spawn_point(const std::string& name,
                                       float x, float y, float z,
                                       int owner,
                                       uint32_t linked_post) {
    uint32_t id = m_next_sp_id++;
    SpawnPoint sp;
    sp.id = id;
    sp.name = name;
    sp.position[0] = x;
    sp.position[1] = y;
    sp.position[2] = z;
    sp.owner = owner;
    sp.linked_post = linked_post;
    sp.active = true;
    m_spawn_points[id] = sp;
    return id;
}

void SpawnSystem::remove_spawn_point(uint32_t id) {
    m_spawn_points.erase(id);
}

const SpawnPoint* SpawnSystem::get_spawn_point(uint32_t id) const {
    auto it = m_spawn_points.find(id);
    if (it == m_spawn_points.end()) return nullptr;
    return &it->second;
}

std::vector<const SpawnPoint*> SpawnSystem::get_team_spawns(int team) const {
    std::vector<const SpawnPoint*> result;
    for (const auto& [id, sp] : m_spawn_points) {
        if (sp.owner == team && sp.active) {
            result.push_back(&sp);
        }
    }
    return result;
}

void SpawnSystem::sync_with_posts(const CommandPostSystem& cps) {
    for (auto& [id, sp] : m_spawn_points) {
        if (sp.linked_post != 0) {
            const CommandPost* cp = cps.get_post(sp.linked_post);
            if (cp) {
                sp.owner = cp->owner_team;
            }
        }
    }
}

void SpawnSystem::request_respawn(EntityId entity, int team,
                                   UnitClass unit_class,
                                   float respawn_delay,
                                   uint32_t preferred_spawn) {
    RespawnRequest req;
    req.entity      = entity;
    req.team        = team;
    req.unit_class  = unit_class;
    req.timer       = respawn_delay;
    req.spawn_point = preferred_spawn;
    m_pending.push_back(req);
}

std::vector<EntityId> SpawnSystem::update(float dt, EntityManager& entities) {
    std::vector<EntityId> spawned;

    for (auto it = m_pending.begin(); it != m_pending.end(); ) {
        it->timer -= dt;

        if (it->timer <= 0.0f) {
            const SpawnPoint* sp = nullptr;

            // Try preferred spawn first.
            if (it->spawn_point != 0) {
                auto sp_it = m_spawn_points.find(it->spawn_point);
                if (sp_it != m_spawn_points.end() &&
                    sp_it->second.owner == it->team &&
                    sp_it->second.active) {
                    sp = &sp_it->second;
                }
            }

            // Fall back to any available spawn.
            if (!sp) {
                for (const auto& [sid, spawn] : m_spawn_points) {
                    if (spawn.owner == it->team && spawn.active) {
                        sp = &spawn;
                        break;
                    }
                }
            }

            if (sp) {
                entities.set_position(it->entity,
                                       sp->position[0],
                                       sp->position[1],
                                       sp->position[2]);
                spawned.push_back(it->entity);
            }

            it = m_pending.erase(it);
        } else {
            ++it;
        }
    }

    return spawned;
}

size_t SpawnSystem::spawn_point_count() const {
    return m_spawn_points.size();
}

size_t SpawnSystem::pending_respawn_count() const {
    return m_pending.size();
}

// -- Reset ------------------------------------------------------------------

void SpawnSystem::clear() {
    m_teams.clear();
    m_spawn_delay   = 5.0f;
    m_sp_hero_rules = false;
    m_spawn_points.clear();
    m_pending.clear();
    m_next_sp_id = 1;
    LOG_INFO("SpawnSystem: cleared");
}

} // namespace swbf
