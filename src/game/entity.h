#pragma once

#include <cstdint>
#include <string>

namespace swbf {

/// Unique identifier for an entity.
using EntityId = uint32_t;
constexpr EntityId INVALID_ENTITY = 0;

/// An entity in the game world.
///
/// Entities are anything that exists in the scene: soldiers, vehicles,
/// props, command posts, projectiles, etc.  Each entity has a transform
/// (position + rotation) and optionally references a GPU model for rendering.
struct Entity {
    EntityId    id       = INVALID_ENTITY;
    std::string name;            // e.g. "rep_inf_ep3_rifleman"
    std::string class_label;     // ODF class: "soldier", "hover", "prop", etc.

    // Transform
    float position[3] = {0.0f, 0.0f, 0.0f};
    float yaw         = 0.0f;   // rotation around Y axis (radians)

    // Model reference -- index into WorldLoader::gpu_models(), or -1 if none.
    int model_index = -1;

    // Gameplay state
    float health     = 100.0f;
    float max_health = 100.0f;
    float speed      = 6.0f;    // movement speed (units/sec)
    int   team       = 0;       // 0 = neutral, 1 = team 1, 2 = team 2
    bool  alive      = true;
    bool  visible    = true;

    // Is this entity the player?
    bool is_player = false;

    // Scoreboard stats.
    int kills  = 0;
    int deaths = 0;
    int score  = 0;
};

} // namespace swbf
