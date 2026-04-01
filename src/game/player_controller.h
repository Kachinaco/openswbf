#pragma once

#include "game/entity.h"
#include "game/weapon_system.h"

namespace swbf {

// Forward declarations.
class Camera;
class InputSystem;
class PhysicsWorld;
class EntityManager;
class HealthSystem;
class SpawnSystem;
class ParticleSystem;

/// Camera mode for the player view.
enum class CameraMode : int {
    FirstPerson,
    ThirdPerson
};

/// Player state machine.
enum class PlayerState : int {
    Alive,
    Dead,
    Respawning
};

/// Player controller — the core gameplay component.
///
/// Manages a playable soldier entity with:
///   - WASD movement grounded to terrain via PhysicsWorld height queries
///   - Mouse look with pointer lock
///   - Sprint (Shift), crouch (Ctrl), jump (Space) with gravity
///   - First-person / third-person camera toggle (V key)
///   - Weapon firing via left click (raycast hit detection)
///   - Health tracking and death/respawn cycle
///   - Crosshair rendering
class PlayerController {
public:
    PlayerController() = default;

    /// Initialize the player controller.
    /// Creates the player entity in the EntityManager, registers with
    /// HealthSystem, and sets initial position.
    void init(EntityManager& entities,
              HealthSystem& health,
              PhysicsWorld& physics,
              float spawn_x, float spawn_y, float spawn_z);

    /// Per-frame update: reads input, applies movement with gravity and
    /// terrain grounding, updates camera, handles weapon firing.
    void update(float dt,
                InputSystem& input,
                Camera& camera,
                PhysicsWorld& physics,
                EntityManager& entities,
                HealthSystem& health,
                WeaponSystem& weapons,
                SpawnSystem& spawns,
                ParticleSystem* particles);

    /// Get the player entity ID.
    EntityId entity_id() const { return m_entity_id; }

    /// Get current player position (3 floats).
    const float* position() const { return m_position; }

    /// Get yaw in radians.
    float yaw() const { return m_yaw; }

    /// Get pitch in radians.
    float pitch() const { return m_pitch; }

    /// Current camera mode.
    CameraMode camera_mode() const { return m_camera_mode; }

    /// Current player state.
    PlayerState state() const { return m_state; }

    /// Whether the player is alive.
    bool is_alive() const { return m_state == PlayerState::Alive; }

    /// Eye height above ground (accounts for crouch).
    float eye_height() const;

    /// True if a shot was fired this frame.  Cleared at the start of update().
    bool fired_this_frame() const { return m_fired_this_frame; }

    /// True if the player died this frame (transitioned Alive -> Dead).
    bool died_this_frame() const { return m_died_this_frame; }

    /// Reset the player to a position (e.g., on respawn).
    void respawn_at(float x, float y, float z,
                    HealthSystem& health);

    // -- Configuration -------------------------------------------------------

    static constexpr float WALK_SPEED    = 6.0f;   // units/sec
    static constexpr float SPRINT_SPEED  = 10.0f;
    static constexpr float CROUCH_SPEED  = 3.0f;

    static constexpr float STAND_HEIGHT  = 1.8f;   // eye height when standing
    static constexpr float CROUCH_HEIGHT = 1.0f;   // eye height when crouching

    static constexpr float JUMP_VELOCITY = 7.0f;   // initial upward velocity
    static constexpr float GRAVITY       = 18.0f;  // downward acceleration

    static constexpr float MOUSE_SENS    = 0.002f; // radians per pixel

    static constexpr float MAX_HEALTH    = 100.0f;
    static constexpr float RESPAWN_DELAY = 5.0f;   // seconds

    // Third-person camera offsets.
    static constexpr float TP_DISTANCE   = 5.0f;   // distance behind player
    static constexpr float TP_HEIGHT     = 2.0f;   // height above player head

    static constexpr float FIRE_COOLDOWN = 0.2f;   // seconds between shots

private:
    // Player entity reference.
    EntityId m_entity_id = INVALID_ENTITY;

    // Position and orientation.
    float m_position[3] = {0.0f, 0.0f, 0.0f};
    float m_yaw   = 0.0f;   // radians, around Y axis
    float m_pitch = 0.0f;   // radians, up/down look

    // Vertical velocity for jump/gravity.
    float m_vertical_velocity = 0.0f;
    bool  m_on_ground = true;

    // Movement state.
    bool m_sprinting = false;
    bool m_crouching = false;

    // Camera.
    CameraMode m_camera_mode = CameraMode::FirstPerson;

    // Player state.
    PlayerState m_state = PlayerState::Alive;
    float m_respawn_timer = 0.0f;

    // Weapon firing.
    float m_fire_timer = 0.0f;
    WeaponConfig m_weapon;
    bool m_fired_this_frame = false;
    bool m_died_this_frame  = false;

    // Internal helpers.
    void handle_movement(float dt, InputSystem& input, PhysicsWorld& physics);
    void handle_look(InputSystem& input);
    void handle_camera(Camera& camera, PhysicsWorld& physics);
    void handle_firing(float dt, InputSystem& input,
                       Camera& camera, PhysicsWorld& physics,
                       EntityManager& entities, HealthSystem& health,
                       WeaponSystem& weapons, ParticleSystem* particles);
    void handle_death(float dt, HealthSystem& health, SpawnSystem& spawns,
                      EntityManager& entities, PhysicsWorld& physics);
};

} // namespace swbf
