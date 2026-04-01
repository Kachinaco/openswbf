#pragma once

#include "renderer/ui_renderer.h"
#include "game/command_post_system.h"
#include "game/entity_manager.h"
#include "core/types.h"

namespace swbf {

/// Renders a top-down minimap in the corner of the screen.
///
/// Shows:
/// - A circular background representing the terrain
/// - Command post icons colored by team ownership
/// - Friendly unit dots
/// - Player position and facing direction indicator
///
/// The minimap is centered on the player and shows a configurable radius
/// of the game world.
class MiniMapRenderer {
public:
    /// Set the position and size of the minimap on screen.
    void set_position(float x, float y, float size);

    /// Set the world-space radius shown in the minimap.
    void set_world_radius(float radius) { m_world_radius = radius; }

    /// Render the minimap.
    /// @p ui            The UIRenderer to draw into (must have begin() called).
    /// @p player_pos    Player world position [3].
    /// @p player_yaw    Player facing direction in radians.
    /// @p player_team   Which team the player is on.
    /// @p cps           Command post system for post locations/ownership.
    /// @p entities      Entity manager for friendly unit positions.
    void render(UIRenderer& ui,
                const float* player_pos, float player_yaw,
                Team player_team,
                const CommandPostSystem& cps,
                const EntityManager& entities);

private:
    /// Convert a world position to minimap screen coordinates relative to
    /// the player.  Returns false if the position is outside the minimap.
    bool world_to_minimap(const float* world_pos, const float* player_pos,
                          float player_yaw, float& out_sx, float& out_sy) const;

    float m_screen_x    = 10.0f;
    float m_screen_y    = 10.0f;
    float m_size        = 160.0f;
    float m_world_radius = 120.0f;
};

} // namespace swbf
