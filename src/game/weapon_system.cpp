#include "game/weapon_system.h"
#include "game/entity_manager.h"
#include "core/log.h"

#include <cmath>

namespace swbf {

// -- ODF weapon registry ----------------------------------------------------

void WeaponSystem::register_weapon(const std::string& odf_name) {
    if (m_weapons.count(odf_name)) {
        LOG_DEBUG("WeaponSystem: weapon \"%s\" already registered",
                  odf_name.c_str());
        return;
    }
    int idx = static_cast<int>(m_weapons.size());
    m_weapons[odf_name] = idx;
    LOG_INFO("WeaponSystem: registered weapon \"%s\" (index=%d)",
             odf_name.c_str(), idx);
}

bool WeaponSystem::has_weapon(const std::string& odf_name) const {
    return m_weapons.count(odf_name) > 0;
}

// -- Ammo tracking ----------------------------------------------------------

void WeaponSystem::set_ammo(int current, int max) {
    m_ammo     = current;
    m_max_ammo = max;
}

void WeaponSystem::set_weapon_name(const std::string& name) {
    m_weapon_name = name;
}

bool WeaponSystem::fire_ammo() {
    if (m_ammo <= 0) return false;
    --m_ammo;
    return true;
}

void WeaponSystem::reload() {
    m_ammo = m_max_ammo;
}

// -- Projectile system ------------------------------------------------------

size_t WeaponSystem::fire(const WeaponConfig& weapon,
                          EntityId owner,
                          float ox, float oy, float oz,
                          float dx, float dy, float dz) {
    // Normalise direction.
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len > 1e-6f) {
        dx /= len;
        dy /= len;
        dz /= len;
    }

    Projectile proj;
    proj.position[0] = ox;
    proj.position[1] = oy;
    proj.position[2] = oz;
    proj.direction[0] = dx;
    proj.direction[1] = dy;
    proj.direction[2] = dz;
    proj.speed               = weapon.projectile_speed;
    proj.damage              = weapon.damage;
    proj.max_range           = weapon.max_range;
    proj.owner               = owner;
    proj.distance_travelled  = 0.0f;
    proj.active              = true;

    m_projectiles.push_back(proj);
    return m_projectiles.size() - 1;
}

std::vector<ProjectileHit> WeaponSystem::update(float dt,
                                                 EntityManager& entities,
                                                 float hit_radius) {
    std::vector<ProjectileHit> hits;

    for (auto& proj : m_projectiles) {
        if (!proj.active) continue;

        // Move projectile.
        float move = proj.speed * dt;
        proj.position[0] += proj.direction[0] * move;
        proj.position[1] += proj.direction[1] * move;
        proj.position[2] += proj.direction[2] * move;
        proj.distance_travelled += move;

        // Check range.
        if (proj.distance_travelled >= proj.max_range) {
            proj.active = false;
            continue;
        }

        // Check collision against entities.
        float hr_sq = hit_radius * hit_radius;
        entities.for_each([&](EntityId id, Entity& ent) {
            if (!proj.active) return;
            if (id == proj.owner) return;
            if (!ent.alive) return;

            float dx = proj.position[0] - ent.position[0];
            float dy = proj.position[1] - ent.position[1];
            float dz = proj.position[2] - ent.position[2];
            float dist_sq = dx * dx + dy * dy + dz * dz;

            if (dist_sq <= hr_sq) {
                ProjectileHit hit;
                hit.target     = id;
                hit.hit_pos[0] = proj.position[0];
                hit.hit_pos[1] = proj.position[1];
                hit.hit_pos[2] = proj.position[2];
                hit.damage     = proj.damage;
                hits.push_back(hit);
                proj.active = false;
            }
        });
    }

    return hits;
}

size_t WeaponSystem::active_count() const {
    size_t n = 0;
    for (const auto& p : m_projectiles) {
        if (p.active) ++n;
    }
    return n;
}

size_t WeaponSystem::total_count() const {
    return m_projectiles.size();
}

const Projectile* WeaponSystem::get_projectile(size_t index) const {
    if (index >= m_projectiles.size()) return nullptr;
    return &m_projectiles[index];
}

// -- Reset ------------------------------------------------------------------

void WeaponSystem::clear() {
    m_weapons.clear();
    m_projectiles.clear();
    m_ammo     = 50;
    m_max_ammo = 50;
    m_weapon_name = "DC-15A Blaster";
    LOG_INFO("WeaponSystem: cleared");
}

} // namespace swbf
