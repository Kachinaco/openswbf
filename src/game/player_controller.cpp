#include "game/player_controller.h"
#include "game/entity_manager.h"
#include "game/health_system.h"
#include "game/spawn_system.h"
#include "game/weapon_system.h"
#include "renderer/camera.h"
#include "renderer/particle_system.h"
#include "renderer/particle_effects.h"
#include "input/input_system.h"
#include "physics/physics_world.h"
#include "core/log.h"

#include <SDL.h>
#include <cmath>
#include <algorithm>

namespace swbf {

void PlayerController::init(EntityManager& entities,
                            HealthSystem& health,
                            PhysicsWorld& physics,
                            float spawn_x, float spawn_y, float spawn_z) {
    // Create the player entity.
    m_entity_id = entities.create("player", /*team=*/1, /*is_ai=*/false);
    Entity* ent = entities.get(m_entity_id);
    if (ent) {
        ent->is_player = true;
        ent->max_health = MAX_HEALTH;
        ent->health = MAX_HEALTH;
    }

    // Register with health system.
    health.add(m_entity_id, MAX_HEALTH);
    health.set_health(MAX_HEALTH);
    health.set_max_health(MAX_HEALTH);

    // Set initial position, grounded to terrain.
    float terrain_y = physics.get_terrain_height(spawn_x, spawn_z);
    m_position[0] = spawn_x;
    m_position[1] = (spawn_y > terrain_y) ? spawn_y : terrain_y;
    m_position[2] = spawn_z;

    entities.set_position(m_entity_id, m_position[0], m_position[1], m_position[2]);

    // Default weapon.
    m_weapon.name = "DC-15A Blaster";
    m_weapon.damage = 25.0f;
    m_weapon.fire_rate = FIRE_COOLDOWN;
    m_weapon.projectile_speed = 150.0f;
    m_weapon.max_range = 300.0f;

    m_state = PlayerState::Alive;
    m_on_ground = true;
    m_vertical_velocity = 0.0f;
    m_yaw = 0.0f;
    m_pitch = 0.0f;

    LOG_INFO("PlayerController: spawned at (%.1f, %.1f, %.1f)",
             static_cast<double>(m_position[0]),
             static_cast<double>(m_position[1]),
             static_cast<double>(m_position[2]));
}

float PlayerController::eye_height() const {
    return m_crouching ? CROUCH_HEIGHT : STAND_HEIGHT;
}

void PlayerController::respawn_at(float x, float y, float z,
                                   HealthSystem& health) {
    m_position[0] = x;
    m_position[1] = y;
    m_position[2] = z;
    m_vertical_velocity = 0.0f;
    m_on_ground = true;
    m_sprinting = false;
    m_crouching = false;
    m_state = PlayerState::Alive;
    m_fire_timer = 0.0f;

    health.heal(m_entity_id, MAX_HEALTH);
    health.set_health(MAX_HEALTH);

    LOG_INFO("PlayerController: respawned at (%.1f, %.1f, %.1f)",
             static_cast<double>(x), static_cast<double>(y), static_cast<double>(z));
}

void PlayerController::update(float dt,
                               InputSystem& input,
                               Camera& camera,
                               PhysicsWorld& physics,
                               EntityManager& entities,
                               HealthSystem& health,
                               WeaponSystem& weapons,
                               SpawnSystem& spawns,
                               ParticleSystem* particles) {
    // Clear per-frame event flags.
    m_fired_this_frame = false;
    m_died_this_frame  = false;

    // Handle death/respawn state.
    if (m_state != PlayerState::Alive) {
        handle_death(dt, health, spawns, entities, physics);
        handle_camera(camera, physics);
        return;
    }

    // Check if player died this frame.
    if (health.is_dead(m_entity_id) || health.get_health(m_entity_id) <= 0.0f) {
        m_state = PlayerState::Dead;
        m_died_this_frame = true;
        m_respawn_timer = RESPAWN_DELAY;
        LOG_INFO("PlayerController: player died");
        handle_camera(camera, physics);
        return;
    }

    // Toggle camera mode with V key.
    if (input.key_pressed(SDL_SCANCODE_V)) {
        m_camera_mode = (m_camera_mode == CameraMode::FirstPerson)
            ? CameraMode::ThirdPerson
            : CameraMode::FirstPerson;
        LOG_DEBUG("Camera mode: %s",
                  m_camera_mode == CameraMode::FirstPerson ? "first-person" : "third-person");
    }

    // Process input.
    handle_look(input);
    handle_movement(dt, input, physics);
    handle_firing(dt, input, camera, physics, entities, health, weapons, particles);

    // Sync entity position.
    entities.set_position(m_entity_id, m_position[0], m_position[1], m_position[2]);
    Entity* ent = entities.get(m_entity_id);
    if (ent) {
        ent->yaw = m_yaw;
    }

    // Sync player health to HUD display.
    float hp = health.get_health(m_entity_id);
    if (hp >= 0.0f) {
        health.set_health(hp);
    }

    // Update camera last (after position is finalized).
    handle_camera(camera, physics);
}

void PlayerController::handle_look(InputSystem& input) {
    if (!input.pointer_locked()) return;

    float dx = input.mouse_dx();
    float dy = input.mouse_dy();

    m_yaw   += dx * MOUSE_SENS;
    m_pitch -= dy * MOUSE_SENS;

    // Clamp pitch to avoid flipping.
    constexpr float MAX_PITCH = 1.5533f; // ~89 degrees
    if (m_pitch >  MAX_PITCH) m_pitch =  MAX_PITCH;
    if (m_pitch < -MAX_PITCH) m_pitch = -MAX_PITCH;
}

void PlayerController::handle_movement(float dt, InputSystem& input,
                                        PhysicsWorld& physics) {
    // Determine movement speed.
    m_sprinting = input.key_down(SDL_SCANCODE_LSHIFT) ||
                  input.key_down(SDL_SCANCODE_RSHIFT);
    m_crouching = input.key_down(SDL_SCANCODE_LCTRL) ||
                  input.key_down(SDL_SCANCODE_RCTRL);

    float speed = WALK_SPEED;
    if (m_sprinting && !m_crouching) speed = SPRINT_SPEED;
    if (m_crouching) speed = CROUCH_SPEED;

    // Calculate forward and right vectors on the XZ plane (from yaw only).
    float fwd_x =  std::sin(m_yaw);
    float fwd_z = -std::cos(m_yaw);
    float rgt_x =  std::cos(m_yaw);
    float rgt_z =  std::sin(m_yaw);

    // Accumulate horizontal movement.
    float move_x = 0.0f;
    float move_z = 0.0f;

    if (input.key_down(SDL_SCANCODE_W)) {
        move_x += fwd_x;
        move_z += fwd_z;
    }
    if (input.key_down(SDL_SCANCODE_S)) {
        move_x -= fwd_x;
        move_z -= fwd_z;
    }
    if (input.key_down(SDL_SCANCODE_A)) {
        move_x -= rgt_x;
        move_z -= rgt_z;
    }
    if (input.key_down(SDL_SCANCODE_D)) {
        move_x += rgt_x;
        move_z += rgt_z;
    }

    // Normalize diagonal movement so you don't go faster diagonally.
    float move_len = std::sqrt(move_x * move_x + move_z * move_z);
    if (move_len > 1e-4f) {
        move_x = (move_x / move_len) * speed * dt;
        move_z = (move_z / move_len) * speed * dt;
    } else {
        move_x = 0.0f;
        move_z = 0.0f;
    }

    // Apply horizontal movement.
    m_position[0] += move_x;
    m_position[2] += move_z;

    // Jump.
    if (input.key_pressed(SDL_SCANCODE_SPACE) && m_on_ground) {
        m_vertical_velocity = JUMP_VELOCITY;
        m_on_ground = false;
    }

    // Apply gravity.
    m_vertical_velocity -= GRAVITY * dt;
    m_position[1] += m_vertical_velocity * dt;

    // Ground the player to terrain.
    float terrain_y = physics.get_terrain_height(m_position[0], m_position[2]);
    if (m_position[1] <= terrain_y) {
        m_position[1] = terrain_y;
        m_vertical_velocity = 0.0f;
        m_on_ground = true;
    }
}

void PlayerController::handle_camera(Camera& camera, PhysicsWorld& physics) {
    float eye_y = m_position[1] + eye_height();

    if (m_camera_mode == CameraMode::FirstPerson) {
        camera.set_position(m_position[0], eye_y, m_position[2]);
        camera.set_rotation(m_pitch, m_yaw);
    } else {
        // Third-person: camera behind and above the player.
        // Compute the "behind" direction from yaw and pitch.
        float cp = std::cos(m_pitch);
        float sp = std::sin(m_pitch);
        float cy = std::cos(m_yaw);
        float sy = std::sin(m_yaw);

        // Direction the camera looks (forward).
        float fwd_x =  sy * cp;
        float fwd_y =  sp;
        float fwd_z = -cy * cp;

        // Camera is placed behind the player.
        float cam_x = m_position[0] - fwd_x * TP_DISTANCE;
        float cam_y = eye_y + TP_HEIGHT - fwd_y * TP_DISTANCE;
        float cam_z = m_position[2] - fwd_z * TP_DISTANCE;

        // Don't let camera go below terrain.
        float cam_terrain = physics.get_terrain_height(cam_x, cam_z);
        if (cam_y < cam_terrain + 0.5f) {
            cam_y = cam_terrain + 0.5f;
        }

        camera.set_position(cam_x, cam_y, cam_z);
        camera.set_rotation(m_pitch, m_yaw);
    }
}

void PlayerController::handle_firing(float dt, InputSystem& input,
                                      Camera& /*camera*/, PhysicsWorld& physics,
                                      EntityManager& entities, HealthSystem& health,
                                      WeaponSystem& weapons,
                                      ParticleSystem* particles) {
    // Tick down fire cooldown.
    if (m_fire_timer > 0.0f) {
        m_fire_timer -= dt;
    }

    // Fire on left click (only when pointer is locked and alive).
    if (!input.pointer_locked()) return;
    if (!input.mouse_button(SDL_BUTTON_LEFT)) return;
    if (m_fire_timer > 0.0f) return;

    // Try to consume ammo.
    if (!weapons.fire_ammo()) return;

    m_fire_timer = m_weapon.fire_rate;
    m_fired_this_frame = true;

    // Compute firing direction from camera orientation.
    float cp = std::cos(m_pitch);
    float sp = std::sin(m_pitch);
    float cy = std::cos(m_yaw);
    float sy = std::sin(m_yaw);

    float dir_x =  sy * cp;
    float dir_y =  sp;
    float dir_z = -cy * cp;

    // Fire origin: from the player's eye position.
    float eye_y = m_position[1] + eye_height();
    float origin_x = m_position[0];
    float origin_y = eye_y;
    float origin_z = m_position[2];

    // Spawn the projectile.
    weapons.fire(m_weapon, m_entity_id,
                 origin_x, origin_y, origin_z,
                 dir_x, dir_y, dir_z);

    // Muzzle flash particle effect.
    if (particles) {
        // Offset slightly forward from the player.
        float flash_x = origin_x + dir_x * 1.0f;
        float flash_y = origin_y + dir_y * 1.0f - 0.2f;
        float flash_z = origin_z + dir_z * 1.0f;
        effects::muzzle_flash(*particles, flash_x, flash_y, flash_z,
                              dir_x, dir_y, dir_z);
    }

    // Raycast for instant hit detection (hitscan on top of projectile).
    float hit_pos[3] = {0, 0, 0};
    float hit_normal[3] = {0, 1, 0};

    // Check entities first (sphere check).
    EntityId hit_entity = INVALID_ENTITY;
    float best_dist = m_weapon.max_range;

    auto all_ids = entities.all_ids();
    for (EntityId id : all_ids) {
        if (id == m_entity_id) continue;
        const Entity* ent = entities.get(id);
        if (!ent || !ent->alive) continue;

        // Simple sphere intersection.
        float to_x = ent->position[0] - origin_x;
        float to_y = ent->position[1] - origin_y;
        float to_z = ent->position[2] - origin_z;

        // Project onto ray direction.
        float along = to_x * dir_x + to_y * dir_y + to_z * dir_z;
        if (along < 0.0f || along > best_dist) continue;

        // Perpendicular distance from ray to entity center.
        float proj_x = origin_x + dir_x * along;
        float proj_y = origin_y + dir_y * along;
        float proj_z = origin_z + dir_z * along;

        float dx = proj_x - ent->position[0];
        float dy = proj_y - (ent->position[1] + 0.9f); // center mass
        float dz = proj_z - ent->position[2];
        float perp_dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        constexpr float HIT_RADIUS = 1.0f;
        if (perp_dist < HIT_RADIUS && along < best_dist) {
            hit_entity = id;
            best_dist = along;
            hit_pos[0] = proj_x;
            hit_pos[1] = proj_y;
            hit_pos[2] = proj_z;
        }
    }

    if (hit_entity != INVALID_ENTITY) {
        // Apply damage to hit entity.
        health.damage(hit_entity, m_weapon.damage);

        if (particles) {
            effects::sparks(*particles, hit_pos[0], hit_pos[1], hit_pos[2],
                            0.0f, 1.0f, 0.0f);
        }
        return;
    }

    // If no entity hit, check terrain.
    if (physics.raycast(origin_x, origin_y, origin_z,
                        dir_x, dir_y, dir_z,
                        m_weapon.max_range,
                        hit_pos, hit_normal)) {
        if (particles) {
            effects::sparks(*particles,
                            hit_pos[0], hit_pos[1], hit_pos[2],
                            hit_normal[0], hit_normal[1], hit_normal[2]);
        }
    }
}

void PlayerController::handle_death(float dt, HealthSystem& health,
                                     SpawnSystem& spawns,
                                     EntityManager& entities,
                                     PhysicsWorld& physics) {
    if (m_state == PlayerState::Dead) {
        m_respawn_timer -= dt;
        if (m_respawn_timer <= 0.0f) {
            m_state = PlayerState::Respawning;
        }
        return;
    }

    if (m_state == PlayerState::Respawning) {
        // Find a spawn point for the player's team.
        auto team_spawns = spawns.get_team_spawns(1); // player team
        float sp_x = 0.0f, sp_y = 0.0f, sp_z = 0.0f;

        if (!team_spawns.empty()) {
            const SpawnPoint* sp = team_spawns[0];
            sp_x = sp->position[0];
            sp_y = sp->position[1];
            sp_z = sp->position[2];
        }

        // Ground to terrain.
        float terrain_y = physics.get_terrain_height(sp_x, sp_z);
        if (sp_y < terrain_y) sp_y = terrain_y;

        // Reset entity state.
        Entity* ent = entities.get(m_entity_id);
        if (ent) {
            ent->alive = true;
            ent->health = MAX_HEALTH;
        }

        // Re-register with health system (in case it was removed on death).
        health.remove(m_entity_id);
        health.add(m_entity_id, MAX_HEALTH);

        respawn_at(sp_x, sp_y, sp_z, health);
    }
}

} // namespace swbf
