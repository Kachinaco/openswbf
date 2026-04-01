#pragma once

#include "core/types.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace swbf {

/// Team identifier constants.  0 = neutral / uncaptured.
static constexpr int TEAM_NEUTRAL  = 0;
static constexpr int TEAM_REPUBLIC = 1;
static constexpr int TEAM_CIS      = 2;

/// Represents which team currently controls a command post (for HUD display).
enum class Team : u8 {
    NEUTRAL  = 0,
    REPUBLIC = 1,   // Team 1 — blue
    CIS      = 2,   // Team 2 — red
};

/// Unique command post identifier.
using CommandPostId = u32;

/// State of a command post.
enum class CommandPostState : u8 {
    Neutral,     ///< Not owned by any team.
    Capturing,   ///< Being captured (ownership in transition).
    Owned,       ///< Fully owned by a team.
    Contested    ///< Multiple teams have soldiers in the capture zone.
};

/// A single command post in the game world.
///
/// Combines features from all branches: spatial data for pathfinding,
/// capture mechanics for gameplay, team data for HUD rendering.
struct CommandPost {
    CommandPostId    id            = 0;
    std::string      name;
    float            position[3]  = {0.0f, 0.0f, 0.0f};
    float            capture_radius = 10.0f;

    int              owner_team      = TEAM_NEUTRAL;
    CommandPostState state            = CommandPostState::Neutral;
    float            capture_progress = 0.0f;  ///< 0..1
    int              capturing_team   = TEAM_NEUTRAL;
    float            capture_rate     = 0.25f; ///< progress per second per unit

    /// Helper: get owner as a Team enum for HUD rendering.
    Team owner() const { return static_cast<Team>(owner_team); }
};

/// Callback when a command post changes ownership.
using PostCapturedCallback = std::function<void(CommandPostId post_id,
                                                int old_team, int new_team)>;

/// Manages all command posts on the current map.
///
/// Unified from all branches:
///   - Name-based add/find/set_team  (Lua API)
///   - ID-based add/get/remove       (platform+tests)
///   - Spatial queries (nearest)     (pathfinding)
///   - Capture mechanics             (platform+tests, HUD)
///   - Team counting                 (all)
///   - Player proximity update       (HUD)
class CommandPostSystem {
public:
    bool init();
    void shutdown();

    // -- Construction -----------------------------------------------------------

    /// Add a command post.  Returns its id.
    CommandPostId add_post(const std::string& name,
                           float x, float y, float z,
                           int initial_owner = TEAM_NEUTRAL,
                           float capture_radius = 10.0f);

    /// Convenience: add a pre-built CommandPost struct.
    void add_post(const CommandPost& post);

    /// Remove a command post by id.
    void remove_post(CommandPostId id);

    // -- Queries ----------------------------------------------------------------

    const CommandPost* get_post(CommandPostId id) const;
    CommandPost*       get_post(CommandPostId id);

    /// Find a command post by name.  Returns nullptr if not found.
    CommandPost*       find(const std::string& name);
    const CommandPost* find(const std::string& name) const;

    /// Get all command posts.
    const std::vector<CommandPost>& posts() const { return m_posts; }
    std::vector<CommandPost>&       posts()       { return m_posts; }

    /// Find the nearest command post to a position.
    CommandPostId find_nearest(float x, float y, float z) const;

    /// Find the nearest command post NOT owned by @p team.
    CommandPostId find_nearest_enemy_post(float x, float y, float z,
                                          int team) const;

    /// Find the nearest command post owned by @p team.
    CommandPostId find_nearest_friendly_post(float x, float y, float z,
                                             int team) const;

    /// Find the CP the player is near (within capture radius), or nullptr.
    const CommandPost* post_near_player(const float* player_pos) const;

    // -- Team operations --------------------------------------------------------

    /// Change the owning team of a command post (by name, for Lua API).
    void set_team(const std::string& name, int team);

    /// Count posts owned by a given team.
    int count_owned_by(int team) const;

    /// Count posts owned by a Team enum (HUD convenience).
    int count_owned_by(Team team) const { return count_owned_by(static_cast<int>(team)); }

    /// Total post count.
    size_t count() const { return m_posts.size(); }
    size_t total_posts() const { return m_posts.size(); }

    /// Get the owner of a specific post by id.
    int get_owner(CommandPostId post_id) const;

    // -- Capture mechanics ------------------------------------------------------

    /// Begin capturing a post for the given team.
    void begin_capture(CommandPostId post_id, int team);

    /// Advance capture progress.  Returns true if the post flipped this frame.
    bool update_capture(CommandPostId post_id, float dt, int num_capturers = 1);

    /// Per-frame update for player-proximity capture (HUD branch style).
    void update(float dt, const float* player_pos = nullptr, int player_team = 0);

    /// Set a callback for when a post is captured.
    void set_captured_callback(PostCapturedCallback cb);

    // -- Reset ------------------------------------------------------------------

    void clear();

private:
    std::vector<CommandPost> m_posts;
    CommandPostId            m_next_id = 1;
    PostCapturedCallback     m_captured_callback;
};

} // namespace swbf
