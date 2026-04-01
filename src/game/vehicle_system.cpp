#include "game/vehicle_system.h"

#include "core/log.h"
#include "input/input_system.h"
#include "physics/physics_world.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace swbf {

// ============================================================================
// Helpers
// ============================================================================

namespace {

/// Clamp a float to [lo, hi].
float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/// Linear interpolation.
float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

/// Move @p current toward @p target by at most @p max_delta.
float move_toward(float current, float target, float max_delta) {
    if (target > current) {
        return (current + max_delta > target) ? target : current + max_delta;
    }
    return (current - max_delta < target) ? target : current - max_delta;
}

/// Distance squared between two 3D points.
float dist_sq(const float a[3], const float b[3]) {
    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    float dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}

} // anonymous namespace

// ============================================================================
// Lifecycle
// ============================================================================

void VehicleSystem::init() {
    m_next_id = 1;
    m_definitions.clear();
    m_vehicles.clear();
    m_projectiles.clear();
    m_spawn_pads.clear();
    m_player_vehicle_idx = -1;

    LOG_INFO("VehicleSystem initialised");
}

void VehicleSystem::shutdown() {
    m_vehicles.clear();
    m_projectiles.clear();
    m_spawn_pads.clear();
    m_definitions.clear();
    m_player_vehicle_idx = -1;

    LOG_INFO("VehicleSystem shut down");
}

// ============================================================================
// Definitions
// ============================================================================

int VehicleSystem::add_definition(const VehicleDefinition& def) {
    int idx = static_cast<int>(m_definitions.size());
    m_definitions.push_back(def);
    LOG_INFO("VehicleSystem: registered definition '%s' (index %d, %zu seats, %zu weapons)",
             def.name.c_str(), idx, def.seats.size(), def.weapons.size());
    return idx;
}

// ============================================================================
// Spawning
// ============================================================================

u32 VehicleSystem::spawn_vehicle(int def_index, float x, float y, float z,
                                 float heading) {
    if (def_index < 0 || def_index >= static_cast<int>(m_definitions.size())) {
        LOG_WARN("VehicleSystem::spawn_vehicle — invalid def_index %d", def_index);
        return 0;
    }

    const auto& def = m_definitions[static_cast<size_t>(def_index)];

    VehicleInstance v;
    v.id        = m_next_id++;
    v.def_index = def_index;
    v.health    = def.max_health;
    v.alive     = true;
    v.destroyed = false;
    v.yaw       = heading;
    v.pitch     = 0.0f;
    v.speed     = 0.0f;

    v.transform.position[0] = x;
    v.transform.position[1] = y;
    v.transform.position[2] = z;

    // Zero out velocity.
    v.velocity[0] = v.velocity[1] = v.velocity[2] = 0.0f;

    // Clear occupant slots.
    std::memset(v.occupants, 0, sizeof(v.occupants));

    // Initialise weapon cooldown state.
    v.weapon_states.resize(def.weapons.size());

    LOG_INFO("VehicleSystem: spawned '%s' id=%u at (%.1f, %.1f, %.1f) heading=%.2f",
             def.name.c_str(), v.id,
             static_cast<double>(x), static_cast<double>(y),
             static_cast<double>(z), static_cast<double>(heading));

    m_vehicles.push_back(std::move(v));
    return m_vehicles.back().id;
}

void VehicleSystem::add_spawn_pad(const VehicleSpawnPad& pad) {
    m_spawn_pads.push_back(pad);

    // Immediately spawn the first vehicle at this pad.
    auto& p = m_spawn_pads.back();
    u32 vid = spawn_vehicle(p.def_index,
                            p.position[0], p.position[1], p.position[2],
                            p.heading);
    if (vid != 0) {
        p.vehicle_alive = true;
        p.vehicle_id    = vid;
        p.respawn_timer = 0.0f;

        // Tag the vehicle with its spawn pad index.
        VehicleInstance* vi = find_vehicle(vid);
        if (vi) {
            vi->spawn_pad_idx = static_cast<int>(m_spawn_pads.size() - 1);
        }
    }
}

// ============================================================================
// Mount / Dismount
// ============================================================================

bool VehicleSystem::try_mount(u32 occupant_id, float wx, float wy, float wz,
                              u32* out_vehicle, int* out_seat) {
    // Don't allow if already in a vehicle.
    {
        u32 dummy_v;
        int dummy_s;
        if (get_occupant_vehicle(occupant_id, &dummy_v, &dummy_s)) {
            return false;
        }
    }

    float query_pos[3] = {wx, wy, wz};

    // Find nearest alive vehicle within enter_radius.
    VehicleInstance* best    = nullptr;
    float            best_d2 = 1e30f;

    for (auto& v : m_vehicles) {
        if (!v.alive) continue;
        if (v.def_index < 0) continue;

        const auto& def = m_definitions[static_cast<size_t>(v.def_index)];
        float d2 = dist_sq(query_pos, v.transform.position);
        float r  = def.enter_radius;

        if (d2 < r * r && d2 < best_d2) {
            best    = &v;
            best_d2 = d2;
        }
    }

    if (!best) return false;

    const auto& def = m_definitions[static_cast<size_t>(best->def_index)];

    // Try to seat into the first available seat, preferring DRIVER first.
    // Two passes: first look for empty DRIVER seats, then any empty seat.
    int seat_count = static_cast<int>(def.seats.size());
    if (seat_count > VehicleInstance::MAX_SEATS) {
        seat_count = VehicleInstance::MAX_SEATS;
    }

    // Pass 1: driver.
    for (int i = 0; i < seat_count; ++i) {
        if (def.seats[static_cast<size_t>(i)].role == SeatRole::DRIVER &&
            best->occupants[i] == 0) {
            best->occupants[i] = occupant_id;
            if (out_vehicle) *out_vehicle = best->id;
            if (out_seat)    *out_seat    = i;

            LOG_INFO("VehicleSystem: occupant %u mounted vehicle %u seat %d (DRIVER)",
                     occupant_id, best->id, i);
            return true;
        }
    }

    // Pass 2: any empty seat.
    for (int i = 0; i < seat_count; ++i) {
        if (best->occupants[i] == 0) {
            best->occupants[i] = occupant_id;
            if (out_vehicle) *out_vehicle = best->id;
            if (out_seat)    *out_seat    = i;

            LOG_INFO("VehicleSystem: occupant %u mounted vehicle %u seat %d",
                     occupant_id, best->id, i);
            return true;
        }
    }

    // All seats full.
    return false;
}

bool VehicleSystem::dismount(u32 occupant_id, float* out_pos) {
    for (auto& v : m_vehicles) {
        const auto& def = m_definitions[static_cast<size_t>(v.def_index)];
        int seat_count = static_cast<int>(def.seats.size());
        if (seat_count > VehicleInstance::MAX_SEATS) {
            seat_count = VehicleInstance::MAX_SEATS;
        }

        for (int i = 0; i < seat_count; ++i) {
            if (v.occupants[i] == occupant_id) {
                v.occupants[i] = 0;

                // Place the dismounting entity to the side of the vehicle.
                if (out_pos) {
                    float side_offset[3] = {
                        def.bounding_radius + 1.5f,
                        0.0f,
                        0.0f
                    };
                    local_to_world(v, side_offset, out_pos);
                }

                LOG_INFO("VehicleSystem: occupant %u dismounted vehicle %u seat %d",
                         occupant_id, v.id, i);
                return true;
            }
        }
    }
    return false;
}

bool VehicleSystem::get_occupant_vehicle(u32 occupant_id,
                                         u32* out_vehicle, int* out_seat) const {
    for (const auto& v : m_vehicles) {
        if (v.def_index < 0) continue;
        const auto& def = m_definitions[static_cast<size_t>(v.def_index)];
        int seat_count = static_cast<int>(def.seats.size());
        if (seat_count > VehicleInstance::MAX_SEATS) {
            seat_count = VehicleInstance::MAX_SEATS;
        }

        for (int i = 0; i < seat_count; ++i) {
            if (v.occupants[i] == occupant_id) {
                if (out_vehicle) *out_vehicle = v.id;
                if (out_seat)    *out_seat    = i;
                return true;
            }
        }
    }
    return false;
}

// ============================================================================
// Damage
// ============================================================================

void VehicleSystem::damage_vehicle(u32 vehicle_id, float amount) {
    VehicleInstance* v = find_vehicle(vehicle_id);
    if (!v || !v->alive) return;

    v->health -= amount;
    if (v->health <= 0.0f) {
        v->health = 0.0f;
        destroy_vehicle(*v);
    }
}

void VehicleSystem::destroy_vehicle(VehicleInstance& v) {
    if (v.destroyed) return;

    v.alive     = false;
    v.destroyed = true;
    v.speed     = 0.0f;
    v.velocity[0] = v.velocity[1] = v.velocity[2] = 0.0f;

    const auto& def = m_definitions[static_cast<size_t>(v.def_index)];

    // Eject all occupants.
    int seat_count = static_cast<int>(def.seats.size());
    if (seat_count > VehicleInstance::MAX_SEATS) {
        seat_count = VehicleInstance::MAX_SEATS;
    }

    for (int i = 0; i < seat_count; ++i) {
        if (v.occupants[i] != 0) {
            LOG_INFO("VehicleSystem: ejecting occupant %u from destroyed vehicle %u",
                     v.occupants[i], v.id);
            v.occupants[i] = 0;
        }
    }

    LOG_INFO("VehicleSystem: vehicle %u ('%s') destroyed",
             v.id, def.name.c_str());

    // If this vehicle came from a spawn pad, start the respawn timer.
    if (v.spawn_pad_idx >= 0 &&
        v.spawn_pad_idx < static_cast<int>(m_spawn_pads.size())) {
        auto& pad = m_spawn_pads[static_cast<size_t>(v.spawn_pad_idx)];
        pad.vehicle_alive = false;
        pad.vehicle_id    = 0;
        pad.respawn_timer = def.respawn_time;
    }
}

// ============================================================================
// Accessors
// ============================================================================

VehicleInstance* VehicleSystem::find_vehicle(u32 id) {
    for (auto& v : m_vehicles) {
        if (v.id == id) return &v;
    }
    return nullptr;
}

const VehicleInstance* VehicleSystem::find_vehicle(u32 id) const {
    for (const auto& v : m_vehicles) {
        if (v.id == id) return &v;
    }
    return nullptr;
}

int VehicleSystem::alive_count() const {
    int n = 0;
    for (const auto& v : m_vehicles) {
        if (v.alive) ++n;
    }
    return n;
}

// ============================================================================
// Transform helpers
// ============================================================================

void VehicleSystem::get_forward(const VehicleInstance& v, float out[3]) const {
    // Forward is along -Z in our right-handed system, rotated by yaw and pitch.
    float cy = std::cos(v.yaw);
    float sy = std::sin(v.yaw);
    float cp = std::cos(v.pitch);
    float sp = std::sin(v.pitch);

    out[0] = -sy * cp;     // x
    out[1] =  sp;           // y (pitch up = positive)
    out[2] = -cy * cp;     // z
}

void VehicleSystem::local_to_world(const VehicleInstance& v,
                                   const float local[3],
                                   float world[3]) const {
    // Simple yaw-only rotation for ground vehicles; yaw+pitch for flyers.
    float cy = std::cos(v.yaw);
    float sy = std::sin(v.yaw);

    // Right vector (perpendicular to forward on XZ plane).
    float rx =  cy;
    float rz = -sy;

    // Forward vector on XZ plane.
    float fx = -sy;
    float fz = -cy;

    world[0] = v.transform.position[0] + local[0] * rx + local[2] * fx;
    world[1] = v.transform.position[1] + local[1];
    world[2] = v.transform.position[2] + local[0] * rz + local[2] * fz;
}

// ============================================================================
// Update — main entry point
// ============================================================================

void VehicleSystem::update(float dt, const PhysicsWorld* physics,
                           const InputSystem* input) {
    if (dt <= 0.0f) return;

    // -- Determine which vehicle the player is driving -----------------------
    m_player_vehicle_idx = -1;
    // For now, the "player" is occupant id 1 (the convention used by the
    // rest of the codebase).  A proper entity system would replace this.
    constexpr u32 PLAYER_ID = 1;

    for (int i = 0; i < static_cast<int>(m_vehicles.size()); ++i) {
        auto& v = m_vehicles[static_cast<size_t>(i)];
        if (!v.alive || v.def_index < 0) continue;

        const auto& def = m_definitions[static_cast<size_t>(v.def_index)];
        int seat_count = static_cast<int>(def.seats.size());
        if (seat_count > VehicleInstance::MAX_SEATS) {
            seat_count = VehicleInstance::MAX_SEATS;
        }

        for (int s = 0; s < seat_count; ++s) {
            if (v.occupants[s] == PLAYER_ID &&
                def.seats[static_cast<size_t>(s)].role == SeatRole::DRIVER) {
                m_player_vehicle_idx = i;
                break;
            }
        }
        if (m_player_vehicle_idx >= 0) break;
    }

    // -- Update each vehicle -------------------------------------------------
    for (auto& v : m_vehicles) {
        if (!v.alive) continue;
        if (v.def_index < 0) continue;

        const auto& def = m_definitions[static_cast<size_t>(v.def_index)];

        // Only pass input to the player-driven vehicle.
        const InputSystem* vi = nullptr;
        if (m_player_vehicle_idx >= 0 &&
            &v == &m_vehicles[static_cast<size_t>(m_player_vehicle_idx)]) {
            vi = input;
        }

        switch (def.locomotion) {
        case VehicleLocomotion::GROUND:
            update_ground_vehicle(v, def, dt, physics, vi);
            break;
        case VehicleLocomotion::FLYER:
            update_flyer(v, def, dt, physics, vi);
            break;
        }

        update_weapons(v, def, dt, vi);

        // Update the transform's rotation quaternion from yaw/pitch.
        // Quaternion from Euler: q = qYaw * qPitch
        float half_yaw   = v.yaw   * 0.5f;
        float half_pitch  = v.pitch * 0.5f;
        float cy = std::cos(half_yaw);
        float sy = std::sin(half_yaw);
        float cp = std::cos(half_pitch);
        float sp = std::sin(half_pitch);

        // qYaw   = (0, sin(y/2), 0, cos(y/2))
        // qPitch = (sin(p/2), 0, 0, cos(p/2))
        // Combined = qYaw * qPitch
        v.transform.rotation[0] = cy * sp;            // x
        v.transform.rotation[1] = sy * cp;            // y
        v.transform.rotation[2] = -sy * sp;           // z
        v.transform.rotation[3] = cy * cp;            // w
    }

    // -- Update projectiles --------------------------------------------------
    update_projectiles(dt, physics);

    // -- Update spawn pads ---------------------------------------------------
    update_spawn_pads(dt);

    // -- Remove dead vehicles that have been destroyed for a while -----------
    // (keep them around briefly so renderers can show explosion effects)
    // For now just leave them; a proper cleanup would remove after a delay.
}

// ============================================================================
// Ground vehicle movement
// ============================================================================

void VehicleSystem::update_ground_vehicle(VehicleInstance& v,
                                          const VehicleDefinition& def,
                                          float dt,
                                          const PhysicsWorld* physics,
                                          const InputSystem* input) {
    // -- Read controls (if player-driven) ------------------------------------
    float throttle_input = 0.0f;     // -1 to +1
    float steer_input    = 0.0f;     // -1 (left) to +1 (right)
    bool  braking        = false;

    if (input) {
        if (input->key_down(SDL_SCANCODE_W)) throttle_input += 1.0f;
        if (input->key_down(SDL_SCANCODE_S)) throttle_input -= 1.0f;
        if (input->key_down(SDL_SCANCODE_A)) steer_input    -= 1.0f;
        if (input->key_down(SDL_SCANCODE_D)) steer_input    += 1.0f;
        braking = input->key_down(SDL_SCANCODE_SPACE);
    }

    // -- Acceleration / deceleration -----------------------------------------
    float target_speed = throttle_input * def.max_speed;

    if (braking) {
        v.speed = move_toward(v.speed, 0.0f, def.brake_decel * dt);
    } else if (std::fabs(throttle_input) > 0.01f) {
        v.speed = move_toward(v.speed, target_speed, def.acceleration * dt);
    } else {
        // Friction: slow down when no input.
        v.speed = move_toward(v.speed, 0.0f, def.acceleration * 0.5f * dt);
    }

    // -- Steering (only while moving) ----------------------------------------
    if (std::fabs(v.speed) > 0.5f) {
        // Scale turn rate by speed fraction so slow vehicles turn slowly.
        float speed_frac = clampf(std::fabs(v.speed) / def.max_speed, 0.2f, 1.0f);
        v.yaw += steer_input * def.turn_rate * speed_frac * dt;
    }

    // -- Compute forward direction on XZ plane ------------------------------
    float fwd[3];
    fwd[0] = -std::sin(v.yaw);
    fwd[1] = 0.0f;
    fwd[2] = -std::cos(v.yaw);

    // -- Move position -------------------------------------------------------
    v.transform.position[0] += fwd[0] * v.speed * dt;
    v.transform.position[2] += fwd[2] * v.speed * dt;

    // -- Terrain following ---------------------------------------------------
    if (physics) {
        float terrain_y = physics->get_terrain_height(v.transform.position[0],
                                                      v.transform.position[2]);
        float target_y = terrain_y + def.hover_height;

        // Smoothly interpolate to target height.
        v.transform.position[1] = lerpf(v.transform.position[1], target_y,
                                        clampf(dt * 8.0f, 0.0f, 1.0f));
    }

    // -- Store velocity for external queries ---------------------------------
    v.velocity[0] = fwd[0] * v.speed;
    v.velocity[1] = 0.0f;
    v.velocity[2] = fwd[2] * v.speed;
}

// ============================================================================
// Flyer movement
// ============================================================================

void VehicleSystem::update_flyer(VehicleInstance& v,
                                 const VehicleDefinition& def,
                                 float dt,
                                 const PhysicsWorld* physics,
                                 const InputSystem* input) {
    // -- Read controls -------------------------------------------------------
    float throttle_input = 0.0f;
    float yaw_input      = 0.0f;
    float pitch_input    = 0.0f;
    float lift_input     = 0.0f;

    if (input) {
        if (input->key_down(SDL_SCANCODE_W)) throttle_input += 1.0f;
        if (input->key_down(SDL_SCANCODE_S)) throttle_input -= 1.0f;
        if (input->key_down(SDL_SCANCODE_A)) yaw_input      -= 1.0f;
        if (input->key_down(SDL_SCANCODE_D)) yaw_input      += 1.0f;
        if (input->key_down(SDL_SCANCODE_SPACE))  lift_input += 1.0f;
        if (input->key_down(SDL_SCANCODE_LSHIFT) ||
            input->key_down(SDL_SCANCODE_RSHIFT)) lift_input -= 1.0f;

        // Mouse-look for pitch/yaw adjustment.
        if (input->pointer_locked()) {
            pitch_input += input->mouse_dy() * 0.003f;
            yaw_input   += input->mouse_dx() * 0.003f;
        }
    }

    // -- Yaw -----------------------------------------------------------------
    v.yaw += yaw_input * def.max_yaw_rate * dt;

    // -- Pitch ---------------------------------------------------------------
    v.pitch += pitch_input * def.max_pitch_rate * dt;
    v.pitch = clampf(v.pitch, -1.2f, 1.2f);  // ~69 degrees max pitch

    // Auto-level pitch slowly when no input.
    if (std::fabs(pitch_input) < 0.01f) {
        v.pitch = move_toward(v.pitch, 0.0f, 0.3f * dt);
    }

    // -- Speed (throttle) ----------------------------------------------------
    float target_speed = throttle_input * def.max_speed;
    if (std::fabs(throttle_input) > 0.01f) {
        v.speed = move_toward(v.speed, target_speed, def.acceleration * dt);
    } else {
        // Air friction.
        v.speed = move_toward(v.speed, 0.0f, def.acceleration * 0.3f * dt);
    }

    // -- Forward direction (yaw + pitch) -------------------------------------
    float fwd[3];
    get_forward(v, fwd);

    // -- Move position along forward -----------------------------------------
    v.transform.position[0] += fwd[0] * v.speed * dt;
    v.transform.position[1] += fwd[1] * v.speed * dt;
    v.transform.position[2] += fwd[2] * v.speed * dt;

    // -- Lift (vertical thrust independent of pitch) -------------------------
    v.transform.position[1] += lift_input * def.lift_speed * dt;

    // -- Altitude clamping ---------------------------------------------------
    float min_y = def.min_altitude;
    if (physics) {
        float terrain_y = physics->get_terrain_height(v.transform.position[0],
                                                      v.transform.position[2]);
        min_y = terrain_y + def.min_altitude;
    }
    if (v.transform.position[1] < min_y) {
        v.transform.position[1] = min_y;
        // If flying into the ground, zero out downward speed component.
        if (v.speed < 0.0f && v.pitch > 0.3f) {
            v.speed *= 0.5f;
        }
    }
    if (v.transform.position[1] > def.max_altitude) {
        v.transform.position[1] = def.max_altitude;
    }

    // -- Store velocity ------------------------------------------------------
    v.velocity[0] = fwd[0] * v.speed;
    v.velocity[1] = fwd[1] * v.speed + lift_input * def.lift_speed;
    v.velocity[2] = fwd[2] * v.speed;
}

// ============================================================================
// Vehicle weapons
// ============================================================================

void VehicleSystem::update_weapons(VehicleInstance& v,
                                   const VehicleDefinition& def,
                                   float dt,
                                   const InputSystem* input) {
    // Tick cooldowns.
    for (auto& ws : v.weapon_states) {
        ws.cooldown_remaining -= dt;
        if (ws.cooldown_remaining < 0.0f) {
            ws.cooldown_remaining = 0.0f;
        }
    }

    if (!input) return;

    // Determine fire inputs.
    bool fire_primary   = input->mouse_button(SDL_BUTTON_LEFT);
    bool fire_secondary = input->mouse_button(SDL_BUTTON_RIGHT);

    for (size_t wi = 0; wi < def.weapons.size(); ++wi) {
        const auto& wdef = def.weapons[wi];
        auto&       ws   = v.weapon_states[wi];

        bool should_fire = wdef.is_secondary ? fire_secondary : fire_primary;
        if (!should_fire) continue;
        if (ws.cooldown_remaining > 0.0f) continue;

        // Fire!
        ws.cooldown_remaining = 1.0f / wdef.fire_rate;

        // Compute world-space fire origin.
        float fire_origin[3];
        local_to_world(v, wdef.offset, fire_origin);

        // Compute fire direction (vehicle forward).
        float fwd[3];
        get_forward(v, fwd);

        VehicleProjectile proj;
        proj.position[0] = fire_origin[0];
        proj.position[1] = fire_origin[1];
        proj.position[2] = fire_origin[2];
        proj.velocity[0] = fwd[0] * wdef.projectile_speed + v.velocity[0];
        proj.velocity[1] = fwd[1] * wdef.projectile_speed + v.velocity[1];
        proj.velocity[2] = fwd[2] * wdef.projectile_speed + v.velocity[2];
        proj.damage       = wdef.damage;
        proj.life_remaining = wdef.range / wdef.projectile_speed;
        proj.owner_vehicle  = v.id;
        proj.alive          = true;

        m_projectiles.push_back(proj);
    }
}

// ============================================================================
// Projectile update
// ============================================================================

void VehicleSystem::update_projectiles(float dt, const PhysicsWorld* physics) {
    for (auto& p : m_projectiles) {
        if (!p.alive) continue;

        // Move.
        p.position[0] += p.velocity[0] * dt;
        p.position[1] += p.velocity[1] * dt;
        p.position[2] += p.velocity[2] * dt;

        p.life_remaining -= dt;
        if (p.life_remaining <= 0.0f) {
            p.alive = false;
            continue;
        }

        // Ground collision.
        if (physics) {
            float terrain_y = physics->get_terrain_height(p.position[0],
                                                          p.position[2]);
            if (p.position[1] <= terrain_y) {
                p.alive = false;
                continue;
            }
        }

        // Vehicle collision: check against all vehicles (except owner).
        for (auto& v : m_vehicles) {
            if (!v.alive) continue;
            if (v.id == p.owner_vehicle) continue;
            if (v.def_index < 0) continue;

            const auto& def = m_definitions[static_cast<size_t>(v.def_index)];
            float d2 = dist_sq(p.position, v.transform.position);
            float r  = def.bounding_radius;

            if (d2 < r * r) {
                // Hit!
                damage_vehicle(v.id, p.damage);
                p.alive = false;
                LOG_DEBUG("VehicleSystem: projectile hit vehicle %u for %.0f damage",
                          v.id, static_cast<double>(p.damage));
                break;
            }
        }
    }

    // Remove dead projectiles (swap-and-pop).
    m_projectiles.erase(
        std::remove_if(m_projectiles.begin(), m_projectiles.end(),
                       [](const VehicleProjectile& p) { return !p.alive; }),
        m_projectiles.end());
}

// ============================================================================
// Spawn pad respawn logic
// ============================================================================

void VehicleSystem::update_spawn_pads(float dt) {
    for (auto& pad : m_spawn_pads) {
        if (pad.vehicle_alive) continue;

        // Vehicle is dead — count down.
        pad.respawn_timer -= dt;
        if (pad.respawn_timer <= 0.0f) {
            pad.respawn_timer = 0.0f;

            u32 vid = spawn_vehicle(pad.def_index,
                                    pad.position[0], pad.position[1],
                                    pad.position[2], pad.heading);
            if (vid != 0) {
                pad.vehicle_alive = true;
                pad.vehicle_id    = vid;

                VehicleInstance* vi = find_vehicle(vid);
                if (vi) {
                    int pad_idx = static_cast<int>(&pad - m_spawn_pads.data());
                    vi->spawn_pad_idx = pad_idx;
                }

                LOG_INFO("VehicleSystem: respawned vehicle at pad (%.0f, %.0f, %.0f)",
                         static_cast<double>(pad.position[0]),
                         static_cast<double>(pad.position[1]),
                         static_cast<double>(pad.position[2]));
            }
        }
    }
}

} // namespace swbf
