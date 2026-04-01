#pragma once

#include "core/types.h"
#include "game/command_post_system.h"

#include <functional>

namespace swbf {

// Forward declaration.
class SpawnSystem;

/// Configuration for a Conquest game mode round.
struct ConquestConfig {
    int   initial_tickets    = 200;
    float bleed_rate         = 1.0f;
    float bleed_threshold    = 0.5f;
    int   total_command_posts = 5;
};

/// Callback when a team wins.
using WinCallback = std::function<void(int winner_team)>;

/// Conquest game mode — the primary Battlefront multiplayer mode.
///
/// Teams fight to control command posts.  When one team holds a majority
/// of CPs, the opposing team's reinforcement tickets bleed down.
/// The round ends when one team's tickets hit zero.
///
/// Unified from Lua API, platform+tests, and HUD branches.
class ConquestMode {
public:
    ConquestMode() = default;
    ~ConquestMode() = default;

    /// Initialize conquest mode with a starting ticket count.
    void init(int starting_tickets = 200);

    /// Initialize with full config (platform+tests compatibility).
    void init(const ConquestConfig& config);

    /// Per-frame update — applies ticket bleed based on command post counts.
    void update(float dt, const CommandPostSystem& cps);

    /// Standalone update using set_post_counts data (for tests / config-based mode).
    void update(float dt);

    /// Whether conquest mode is active.
    bool is_active() const { return m_active; }

    /// Set the bleed rate (tickets lost per interval when behind on CPs).
    void set_bleed_rate(float rate);
    float bleed_rate() const { return m_bleed_rate; }

    /// Get ticket count for a team.
    int tickets(int team) const;
    int tickets(Team team) const { return tickets(static_cast<int>(team)); }
    int get_tickets(int team) const { return tickets(team); }

    /// Remove a ticket for a unit kill.
    void remove_ticket(int team);
    void remove_ticket(Team team) { remove_ticket(static_cast<int>(team)); }
    void on_unit_killed(int team) { remove_ticket(team); }

    /// Set command post ownership counts (drives bleed logic).
    void set_post_counts(int team, int posts_owned);

    /// Check if a team is eliminated (0 tickets).
    bool is_eliminated(int team) const;

    /// Starting ticket count (for display as denominator).
    int starting_tickets() const { return m_starting_tickets; }

    /// Check if the round is over (one team has 0 tickets).
    bool is_game_over() const;

    /// Get the winning team (only valid when is_game_over() is true).
    /// Returns 0 (neutral) if nobody has won yet.
    int winner() const;
    int get_winner() const { return winner(); }
    Team winner_team() const { return static_cast<Team>(winner()); }

    /// Set a callback for when a team wins.
    void set_win_callback(WinCallback cb);

    /// Reset for a new round.
    void clear();

private:
    bool  m_active           = false;
    int   m_starting_tickets = 200;
    int   m_tickets_team1    = 200;
    int   m_tickets_team2    = 200;
    float m_bleed_rate       = 1.0f;
    float m_bleed_timer      = 0.0f;
    float m_bleed_threshold  = 0.5f;
    int   m_total_posts      = 5;
    int   m_posts_team1      = 0;
    int   m_posts_team2      = 0;
    bool  m_game_over        = false;

    WinCallback m_win_callback;

    void check_win_condition();

    static constexpr float BLEED_INTERVAL = 3.0f; // seconds between ticks
};

} // namespace swbf
