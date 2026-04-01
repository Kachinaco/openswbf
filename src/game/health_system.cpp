#include "game/health_system.h"
#include "core/log.h"

#include <algorithm>

namespace swbf {

// -- Per-entity health ------------------------------------------------------

void HealthSystem::add(EntityId id, float max_health) {
    HealthData hd;
    hd.current = max_health;
    hd.max     = max_health;
    hd.dead    = false;
    m_health[id] = hd;
}

void HealthSystem::remove(EntityId id) {
    m_health.erase(id);
}

float HealthSystem::damage(EntityId id, float amount) {
    auto it = m_health.find(id);
    if (it == m_health.end()) return 0.0f;

    HealthData& hd = it->second;
    if (hd.dead) return 0.0f;
    if (amount < 0.0f) amount = 0.0f;

    float actual = std::min(amount, hd.current);
    hd.current -= actual;

    if (hd.current <= 0.0f) {
        hd.current = 0.0f;
        hd.dead = true;
        if (m_death_callback) {
            m_death_callback(id);
        }
    }

    return actual;
}

float HealthSystem::heal(EntityId id, float amount) {
    auto it = m_health.find(id);
    if (it == m_health.end()) return 0.0f;

    HealthData& hd = it->second;
    if (hd.dead) return 0.0f;
    if (amount < 0.0f) amount = 0.0f;

    float space = hd.max - hd.current;
    float actual = std::min(amount, space);
    hd.current += actual;

    return actual;
}

float HealthSystem::get_health(EntityId id) const {
    auto it = m_health.find(id);
    if (it == m_health.end()) return -1.0f;
    return it->second.current;
}

float HealthSystem::get_max_health(EntityId id) const {
    auto it = m_health.find(id);
    if (it == m_health.end()) return -1.0f;
    return it->second.max;
}

bool HealthSystem::is_dead(EntityId id) const {
    auto it = m_health.find(id);
    if (it == m_health.end()) return false;
    return it->second.dead;
}

void HealthSystem::set_death_callback(DeathCallback cb) {
    m_death_callback = std::move(cb);
}

size_t HealthSystem::count() const {
    return m_health.size();
}

// -- Simple player health ---------------------------------------------------

void HealthSystem::take_damage(float amount) {
    m_player_health -= amount;
    if (m_player_health < 0.0f) m_player_health = 0.0f;
}

void HealthSystem::heal_player(float amount) {
    m_player_health += amount;
    if (m_player_health > m_player_max_health) m_player_health = m_player_max_health;
}

// -- Reset ------------------------------------------------------------------

void HealthSystem::clear() {
    m_health.clear();
    m_death_callback = nullptr;
    m_player_health = 100.0f;
    m_player_max_health = 100.0f;
    LOG_INFO("HealthSystem: cleared");
}

} // namespace swbf
