#pragma once

#include "game/components/transform.h"
#include "core/types.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

// Forward declarations.
class InputSystem;
class PhysicsWorld;

// ============================================================================
// Vehicle type identifiers
// ============================================================================

/// Broad locomotion category.
enum class VehicleLocomotion : u8 {
    GROUND,     // tanks, speeders — follows terrain
    FLYER,      // starfighters, gunships — free flight with altitude limits
};

// ============================================================================
// Vehicle seat
// ============================================================================

/// Role a seat occupant plays.
enum class SeatRole : u8 {
    DRIVER,     // controls movement + primary/secondary weapons
    GUNNER,     // controls a turret weapon
    PASSENGER,  // can fire personal weapons; no vehicle control
};

/// Static definition of one seat inside a vehicle.
struct SeatDefinition {
    SeatRole role       = SeatRole::PASSENGER;
    float    offset[3]  = {0.0f, 0.0f, 0.0f};   // local-space offset from vehicle origin
    int      weapon_idx = -1;                     // index into VehicleDefinition::weapons (-1 = none)
};

// ============================================================================
// Vehicle weapon hardpoint
// ============================================================================

/// Static definition of one weapon hardpoint attached to the vehicle.
struct VehicleWeaponDef {
    std::string name;                            // e.g. "laser_cannon"
    float  offset[3]  = {0.0f, 0.0f, 0.0f};     // local-space fire origin
    float  damage      = 25.0f;                  // damage per hit
    float  fire_rate   = 4.0f;                   // rounds per second
    float  projectile_speed = 200.0f;            // world units per second
    float  range       = 500.0f;                 // max distance
    bool   is_secondary = false;                 // true = fired with secondary fire
};

// ============================================================================
// Vehicle definition (template)
// ============================================================================

/// Immutable blueprint describing a vehicle class (e.g. "AAT", "X-Wing").
/// Multiple VehicleInstance objects can share one VehicleDefinition.
struct VehicleDefinition {
    std::string         name;                    // human-readable name
    VehicleLocomotion   locomotion = VehicleLocomotion::GROUND;

    // -- Physics / movement --------------------------------------------------
    float max_speed      = 20.0f;                // world units per second
    float acceleration   = 12.0f;                // units/s^2
    float turn_rate      = 2.0f;                 // radians per second (ground)
    float brake_decel    = 30.0f;                // deceleration when braking

    // Flyer-specific
    float max_pitch_rate = 1.5f;                 // rad/s
    float max_yaw_rate   = 1.5f;                 // rad/s
    float max_altitude   = 120.0f;               // hard ceiling
    float min_altitude   = 5.0f;                 // floor above terrain
    float hover_height   = 1.5f;                 // ground vehicles: height above terrain
    float lift_speed     = 15.0f;                // vertical climb speed (flyers)

    // -- Durability ----------------------------------------------------------
    float max_health     = 500.0f;

    // -- Interaction ---------------------------------------------------------
    float enter_radius   = 5.0f;                 // how close an entity must be to mount

    // -- Spawn / respawn -----------------------------------------------------
    float respawn_time   = 30.0f;                // seconds after destruction

    // -- Seats & weapons -----------------------------------------------------
    std::vector<SeatDefinition>   seats;
    std::vector<VehicleWeaponDef> weapons;

    // -- Visual --------------------------------------------------------------
    float bounding_radius = 4.0f;                // rough collision sphere radius
};

// ============================================================================
// Vehicle weapon runtime state
// ============================================================================

struct VehicleWeaponState {
    float cooldown_remaining = 0.0f;             // seconds until next shot
};

// ============================================================================
// Projectile
// ============================================================================

struct VehicleProjectile {
    float position[3]  = {};
    float velocity[3]  = {};
    float damage        = 0.0f;
    float life_remaining = 0.0f;                 // seconds
    u32   owner_vehicle = 0;                     // VehicleInstance id
    bool  alive         = true;
};

// ============================================================================
// Vehicle instance (runtime state)
// ============================================================================

/// A single spawned vehicle in the world.
struct VehicleInstance {
    u32             id            = 0;
    int             def_index     = -1;          // index into VehicleSystem::m_definitions

    Transform       transform;
    float           velocity[3]   = {};
    float           speed         = 0.0f;        // signed scalar along forward axis
    float           yaw           = 0.0f;        // ground: heading. flyer: heading
    float           pitch         = 0.0f;        // flyer only

    float           health        = 0.0f;
    bool            alive         = true;
    bool            destroyed     = false;       // true once death sequence starts

    // Occupant tracking: each element corresponds to a SeatDefinition.
    // Value is an occupant id (0 = empty).  We use a simple integer id to
    // stay decoupled from any specific entity system.
    static constexpr int MAX_SEATS = 8;
    u32             occupants[MAX_SEATS] = {};

    // Per-weapon cooldown state.
    std::vector<VehicleWeaponState> weapon_states;

    // Spawn pad this vehicle came from (index into spawn pads, -1 = none).
    int             spawn_pad_idx = -1;
};

// ============================================================================
// Spawn pad
// ============================================================================

/// A location where vehicles appear and reappear after destruction.
struct VehicleSpawnPad {
    float position[3]  = {};
    float heading       = 0.0f;                  // initial yaw (radians)
    int   def_index     = -1;                    // which VehicleDefinition to spawn

    // Respawn timer state.
    bool  vehicle_alive = false;
    u32   vehicle_id    = 0;                     // id of the spawned vehicle (0 = none)
    float respawn_timer = 0.0f;                  // counts down to zero
};

// ============================================================================
// VehicleSystem
// ============================================================================

/// Manages all vehicle definitions, instances, projectiles, and spawn pads.
///
/// The system is intentionally decoupled from rendering — it produces world-
/// space transforms and state flags that a renderer can query each frame.
class VehicleSystem {
public:
    // -- Lifecycle -----------------------------------------------------------

    /// Initialise the vehicle system.  Call once at startup.
    void init();

    /// Advance all vehicles, projectiles, and respawn timers by @p dt seconds.
    /// @p physics  Used for terrain-height queries (may be null).
    /// @p input    Used for reading player controls (may be null for AI-only).
    void update(float dt, const PhysicsWorld* physics, const InputSystem* input);

    /// Release all resources.
    void shutdown();

    // -- Definitions ---------------------------------------------------------

    /// Register a vehicle blueprint.  Returns its index.
    int add_definition(const VehicleDefinition& def);

    /// Read-only access to all definitions.
    const std::vector<VehicleDefinition>& definitions() const { return m_definitions; }

    // -- Spawning ------------------------------------------------------------

    /// Manually spawn a vehicle of the given definition at a world position.
    /// Returns the new vehicle's id (0 on failure).
    u32 spawn_vehicle(int def_index, float x, float y, float z, float heading);

    /// Register a spawn pad.  The system will automatically spawn and respawn
    /// vehicles at pads according to their definition's respawn_time.
    void add_spawn_pad(const VehicleSpawnPad& pad);

    // -- Mount / dismount ----------------------------------------------------

    /// Attempt to mount @p occupant_id into the nearest vehicle within
    /// interaction range of (@p wx, @p wy, @p wz).
    /// Returns true if successful; sets @p out_vehicle and @p out_seat.
    bool try_mount(u32 occupant_id, float wx, float wy, float wz,
                   u32* out_vehicle = nullptr, int* out_seat = nullptr);

    /// Dismount @p occupant_id from whatever vehicle they occupy.
    /// @p out_pos receives the world-space dismount position (3 floats).
    /// Returns true if the occupant was in a vehicle.
    bool dismount(u32 occupant_id, float* out_pos = nullptr);

    /// Query which vehicle and seat an occupant is in.
    /// Returns true if found; sets *out_vehicle and *out_seat.
    bool get_occupant_vehicle(u32 occupant_id, u32* out_vehicle, int* out_seat) const;

    // -- Damage --------------------------------------------------------------

    /// Apply @p amount points of damage to vehicle @p vehicle_id.
    void damage_vehicle(u32 vehicle_id, float amount);

    // -- Accessors -----------------------------------------------------------

    /// All live vehicles.
    const std::vector<VehicleInstance>& vehicles() const { return m_vehicles; }

    /// All live projectiles.
    const std::vector<VehicleProjectile>& projectiles() const { return m_projectiles; }

    /// Retrieve a vehicle by id (nullptr if not found).
    VehicleInstance*       find_vehicle(u32 id);
    const VehicleInstance* find_vehicle(u32 id) const;

    /// Number of vehicles currently alive (not destroyed).
    int alive_count() const;

private:
    // Internal helpers.
    void update_ground_vehicle(VehicleInstance& v, const VehicleDefinition& def,
                               float dt, const PhysicsWorld* physics,
                               const InputSystem* input);
    void update_flyer(VehicleInstance& v, const VehicleDefinition& def,
                      float dt, const PhysicsWorld* physics,
                      const InputSystem* input);
    void update_weapons(VehicleInstance& v, const VehicleDefinition& def,
                        float dt, const InputSystem* input);
    void update_projectiles(float dt, const PhysicsWorld* physics);
    void update_spawn_pads(float dt);
    void destroy_vehicle(VehicleInstance& v);

    // Convert vehicle-local offset to world position.
    void local_to_world(const VehicleInstance& v,
                        const float local[3], float world[3]) const;
    // Get forward direction from yaw/pitch.
    void get_forward(const VehicleInstance& v, float out[3]) const;

    u32 m_next_id = 1;

    std::vector<VehicleDefinition> m_definitions;
    std::vector<VehicleInstance>   m_vehicles;
    std::vector<VehicleProjectile> m_projectiles;
    std::vector<VehicleSpawnPad>   m_spawn_pads;

    // The player-controlled vehicle (first occupied vehicle with a DRIVER seat).
    // -1 means no player vehicle.
    int m_player_vehicle_idx = -1;
};

} // namespace swbf
