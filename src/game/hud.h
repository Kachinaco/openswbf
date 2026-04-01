#pragma once

#include "renderer/ui_renderer.h"
#include "renderer/minimap_renderer.h"
#include "game/health_system.h"
#include "game/weapon_system.h"
#include "game/conquest_mode.h"
#include "game/command_post_system.h"
#include "game/entity_manager.h"
#include "core/types.h"

#include <string>
#include <vector>

namespace swbf {

/// A kill feed entry that slowly fades out.
struct KillFeedEntry {
    std::string killer;
    std::string victim;
    Team killer_team = Team::NEUTRAL;
    float timer = 5.0f; // seconds remaining before fade
};

/// The in-game heads-up display.
///
/// Renders all gameplay overlay elements:
/// - Health bar (bottom left)
/// - Ammo counter (bottom right)
/// - Crosshair / reticle (center)
/// - Team ticket counts (top center)
/// - Minimap (top right corner)
/// - Kill feed (top right, below minimap)
/// - Capture progress indicator (center, when near a command post)
class HUD {
public:
    /// Initialize the HUD (call after GL context is ready).
    bool init();

    /// Release resources.
    void destroy();

    /// Update timers (kill feed fade, etc.).
    void update(float dt);

    /// Render the full HUD.
    /// @p screen_w / screen_h  Current viewport size.
    void render(int screen_w, int screen_h,
                const HealthSystem& health,
                const WeaponSystem& weapon,
                const ConquestMode& conquest,
                const CommandPostSystem& cps,
                const EntityManager& entities,
                const float* player_pos, float player_yaw,
                Team player_team);

    /// Add a kill to the kill feed.
    void add_kill(const std::string& killer, const std::string& victim,
                  Team killer_team);

    /// Access the UIRenderer (used by Scoreboard and Menu which share it).
    UIRenderer&       ui_renderer()       { return m_ui; }
    const UIRenderer& ui_renderer() const { return m_ui; }

    /// Access the minimap renderer for configuration.
    MiniMapRenderer&       minimap()       { return m_minimap; }
    const MiniMapRenderer& minimap() const { return m_minimap; }

private:
    UIRenderer      m_ui;
    MiniMapRenderer m_minimap;

    std::vector<KillFeedEntry> m_kill_feed;

    void render_health_bar(int screen_w, int screen_h,
                           const HealthSystem& health);
    void render_ammo(int screen_w, int screen_h,
                     const WeaponSystem& weapon);
    void render_crosshair(int screen_w, int screen_h);
    void render_tickets(int screen_w, int screen_h,
                        const ConquestMode& conquest);
    void render_kill_feed(int screen_w, int screen_h);
    void render_capture_progress(int screen_w, int screen_h,
                                 const CommandPostSystem& cps,
                                 const float* player_pos);
};

} // namespace swbf
