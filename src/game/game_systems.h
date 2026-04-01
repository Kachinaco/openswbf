#pragma once

#include "core/config.h"
#include "core/filesystem.h"

namespace swbf {

// Forward declarations — avoids pulling every system header into every TU.
class EntityManager;
class SpawnSystem;
class CommandPostSystem;
class ConquestMode;
class WeaponSystem;
class AISystem;
class AudioDevice;
class AudioManager;
class Camera;
class LVLLoader;
class HealthSystem;
class SpawnSystem;
class Pathfinder;
class VehicleSystem;

/// GameSystems — a lightweight service locator / context struct.
///
/// Holds non-owning pointers to all game subsystems.  The application
/// (main.cpp) owns the actual system objects; this struct just lets
/// static C callbacks (Lua API bindings) reach them without globals
/// scattered everywhere.
///
/// Usage:
///   GameSystems& sys = GameSystems::instance();
///   sys.spawn_system = &my_spawn_system;
///   // ... later, from a Lua callback:
///   GameSystems::instance().spawn_system->set_team_name(1, "Republic");
struct GameSystems {
    // Subsystem pointers — set by the application at startup.
    EntityManager*      entity_manager      = nullptr;
    SpawnSystem*        spawn_system        = nullptr;
    CommandPostSystem*  command_post_system = nullptr;
    ConquestMode*       conquest_mode       = nullptr;
    WeaponSystem*       weapon_system       = nullptr;
    AISystem*           ai_system           = nullptr;
    AudioDevice*        audio_device        = nullptr;
    AudioManager*       audio_manager       = nullptr;
    Camera*             camera              = nullptr;
    VFS*                vfs                 = nullptr;

    // Gameplay configuration values that don't belong to a specific system.
    Config              config;

    /// Access the singleton instance.
    static GameSystems& instance();

    /// Reset all pointers to nullptr — call during shutdown.
    void clear();

private:
    GameSystems() = default;
};

} // namespace swbf
