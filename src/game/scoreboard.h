#pragma once

#include "renderer/ui_renderer.h"
#include "game/conquest_mode.h"
#include "game/entity_manager.h"
#include "core/types.h"

namespace swbf {

/// Renders the in-game scoreboard overlay.
///
/// The scoreboard is toggled with the Tab key and shows a two-column layout
/// with both teams' players listed by score, plus team totals.
class Scoreboard {
public:
    /// Toggle scoreboard visibility.
    void toggle()              { m_visible = !m_visible; }
    void set_visible(bool v)   { m_visible = v; }
    bool is_visible() const    { return m_visible; }

    /// Render the scoreboard.
    /// @p ui            The UIRenderer to draw into (begin() must have been called).
    /// @p screen_w      Screen width in pixels.
    /// @p screen_h      Screen height in pixels.
    /// @p conquest      Conquest mode for ticket counts.
    /// @p entities      Entity manager for player stats.
    void render(UIRenderer& ui, int screen_w, int screen_h,
                const ConquestMode& conquest,
                const EntityManager& entities);

private:
    bool m_visible = false;

    void render_team_column(UIRenderer& ui,
                            float x, float y, float col_w,
                            const char* team_name,
                            float tr, float tg, float tb,
                            int tickets,
                            const std::vector<const Entity*>& players);
};

} // namespace swbf
