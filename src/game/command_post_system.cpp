#include "game/command_post_system.h"
#include "core/log.h"

#include <cmath>
#include <limits>

namespace swbf {

static float distance3(const float* a, const float* b) {
    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    float dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

static float distance_xz(const float* a, const float* b) {
    float dx = a[0] - b[0];
    float dz = a[2] - b[2];
    return std::sqrt(dx * dx + dz * dz);
}

bool CommandPostSystem::init() {
    LOG_INFO("CommandPostSystem::init");
    return true;
}

void CommandPostSystem::shutdown() {
    clear();
    LOG_INFO("CommandPostSystem::shutdown");
}

// -- Construction -----------------------------------------------------------

CommandPostId CommandPostSystem::add_post(const std::string& name,
                                          float x, float y, float z,
                                          int initial_owner,
                                          float capture_radius) {
    // Check for duplicate by name.
    if (!name.empty() && find(name)) {
        LOG_WARN("CommandPostSystem: CP \"%s\" already exists, updating team",
                 name.c_str());
        set_team(name, initial_owner);
        return find(name)->id;
    }

    CommandPost cp;
    cp.id              = m_next_id++;
    cp.name            = name;
    cp.position[0]     = x;
    cp.position[1]     = y;
    cp.position[2]     = z;
    cp.capture_radius  = capture_radius;
    cp.owner_team      = initial_owner;
    cp.capturing_team  = initial_owner;
    cp.state           = (initial_owner != TEAM_NEUTRAL) ? CommandPostState::Owned
                                                         : CommandPostState::Neutral;
    cp.capture_progress = (initial_owner != TEAM_NEUTRAL) ? 1.0f : 0.0f;

    m_posts.push_back(std::move(cp));

    LOG_INFO("CommandPostSystem: added CP \"%s\" (id=%u, team=%d)",
             name.c_str(), m_posts.back().id, initial_owner);
    return m_posts.back().id;
}

void CommandPostSystem::add_post(const CommandPost& post) {
    CommandPost cp = post;
    if (cp.id == 0) {
        cp.id = m_next_id++;
    } else if (cp.id >= m_next_id) {
        m_next_id = cp.id + 1;
    }
    m_posts.push_back(std::move(cp));
}

void CommandPostSystem::remove_post(CommandPostId id) {
    for (auto it = m_posts.begin(); it != m_posts.end(); ++it) {
        if (it->id == id) {
            m_posts.erase(it);
            return;
        }
    }
}

// -- Queries ----------------------------------------------------------------

const CommandPost* CommandPostSystem::get_post(CommandPostId id) const {
    for (const auto& cp : m_posts) {
        if (cp.id == id) return &cp;
    }
    return nullptr;
}

CommandPost* CommandPostSystem::get_post(CommandPostId id) {
    for (auto& cp : m_posts) {
        if (cp.id == id) return &cp;
    }
    return nullptr;
}

CommandPost* CommandPostSystem::find(const std::string& name) {
    for (auto& cp : m_posts) {
        if (cp.name == name) return &cp;
    }
    return nullptr;
}

const CommandPost* CommandPostSystem::find(const std::string& name) const {
    for (const auto& cp : m_posts) {
        if (cp.name == name) return &cp;
    }
    return nullptr;
}

CommandPostId CommandPostSystem::find_nearest(float x, float y, float z) const {
    CommandPostId best_id = 0;
    float best_dist       = std::numeric_limits<float>::max();
    float pos[3] = {x, y, z};

    for (const auto& cp : m_posts) {
        float d = distance3(pos, cp.position);
        if (d < best_dist) {
            best_dist = d;
            best_id   = cp.id;
        }
    }
    return best_id;
}

CommandPostId CommandPostSystem::find_nearest_enemy_post(
    float x, float y, float z, int team) const {
    CommandPostId best_id = 0;
    float best_dist       = std::numeric_limits<float>::max();
    float pos[3] = {x, y, z};

    for (const auto& cp : m_posts) {
        if (cp.owner_team == team) continue;
        float d = distance3(pos, cp.position);
        if (d < best_dist) {
            best_dist = d;
            best_id   = cp.id;
        }
    }
    return best_id;
}

CommandPostId CommandPostSystem::find_nearest_friendly_post(
    float x, float y, float z, int team) const {
    CommandPostId best_id = 0;
    float best_dist       = std::numeric_limits<float>::max();
    float pos[3] = {x, y, z};

    for (const auto& cp : m_posts) {
        if (cp.owner_team != team) continue;
        float d = distance3(pos, cp.position);
        if (d < best_dist) {
            best_dist = d;
            best_id   = cp.id;
        }
    }
    return best_id;
}

const CommandPost* CommandPostSystem::post_near_player(const float* player_pos) const {
    for (const auto& post : m_posts) {
        float dist = distance_xz(player_pos, post.position);
        if (dist <= post.capture_radius) return &post;
    }
    return nullptr;
}

// -- Team operations --------------------------------------------------------

void CommandPostSystem::set_team(const std::string& name, int team) {
    CommandPost* cp = find(name);
    if (!cp) {
        LOG_WARN("CommandPostSystem: SetCommandPostTeam — \"%s\" not found",
                 name.c_str());
        return;
    }
    int old_team = cp->owner_team;
    cp->owner_team = team;
    cp->state = (team != TEAM_NEUTRAL) ? CommandPostState::Owned
                                        : CommandPostState::Neutral;
    cp->capture_progress = (team != TEAM_NEUTRAL) ? 1.0f : 0.0f;
    cp->capturing_team = team;
    LOG_INFO("CommandPostSystem: CP \"%s\" team changed %d -> %d",
             name.c_str(), old_team, team);
}

int CommandPostSystem::count_owned_by(int team) const {
    int n = 0;
    for (const auto& cp : m_posts) {
        if (cp.owner_team == team) ++n;
    }
    return n;
}

int CommandPostSystem::get_owner(CommandPostId post_id) const {
    const CommandPost* cp = get_post(post_id);
    return cp ? cp->owner_team : TEAM_NEUTRAL;
}

// -- Capture mechanics ------------------------------------------------------

void CommandPostSystem::begin_capture(CommandPostId post_id, int team) {
    CommandPost* cp = get_post(post_id);
    if (!cp) return;

    if (cp->capturing_team != team) {
        cp->capture_progress = 0.0f;
        cp->capturing_team = team;
    }
}

bool CommandPostSystem::update_capture(CommandPostId post_id, float dt, int num_capturers) {
    CommandPost* cp = get_post(post_id);
    if (!cp) return false;

    if (cp->capturing_team == cp->owner_team && cp->owner_team != TEAM_NEUTRAL) {
        return false;
    }

    cp->capture_progress += cp->capture_rate * static_cast<float>(num_capturers) * dt;

    if (cp->capture_progress >= 1.0f) {
        cp->capture_progress = 1.0f;
        int old_owner = cp->owner_team;
        cp->owner_team = cp->capturing_team;
        cp->state = CommandPostState::Owned;

        if (m_captured_callback && old_owner != cp->owner_team) {
            m_captured_callback(post_id, old_owner, cp->owner_team);
        }
        return true;
    }

    cp->state = CommandPostState::Capturing;
    return false;
}

void CommandPostSystem::update(float dt, const float* player_pos, int player_team) {
    if (!player_pos) return;

    constexpr float CAPTURE_SPEED = 0.25f;

    for (auto& post : m_posts) {
        float dist = distance_xz(player_pos, post.position);

        if (dist <= post.capture_radius && player_team != TEAM_NEUTRAL) {
            if (post.owner_team == player_team) {
                // Friendly — reverse any enemy capture.
                if (post.capture_progress > 0.0f && post.capturing_team != player_team) {
                    post.capture_progress -= CAPTURE_SPEED * dt;
                    if (post.capture_progress <= 0.0f) {
                        post.capture_progress = 0.0f;
                        post.capturing_team = TEAM_NEUTRAL;
                        post.state = CommandPostState::Owned;
                    }
                }
            } else {
                post.capturing_team = player_team;
                post.capture_progress += CAPTURE_SPEED * dt;
                post.state = CommandPostState::Capturing;
                if (post.capture_progress >= 1.0f) {
                    int old_owner = post.owner_team;
                    post.capture_progress = 0.0f;
                    post.owner_team = player_team;
                    post.capturing_team = TEAM_NEUTRAL;
                    post.state = CommandPostState::Owned;
                    if (m_captured_callback) {
                        m_captured_callback(post.id, old_owner, post.owner_team);
                    }
                }
            }
        } else {
            // Nobody near — slowly decay capture progress.
            if (post.capture_progress > 0.0f && post.state == CommandPostState::Capturing) {
                post.capture_progress -= CAPTURE_SPEED * 0.5f * dt;
                if (post.capture_progress <= 0.0f) {
                    post.capture_progress = 0.0f;
                    post.capturing_team = TEAM_NEUTRAL;
                    post.state = (post.owner_team != TEAM_NEUTRAL)
                        ? CommandPostState::Owned : CommandPostState::Neutral;
                }
            }
        }
    }
}

void CommandPostSystem::set_captured_callback(PostCapturedCallback cb) {
    m_captured_callback = std::move(cb);
}

// -- Reset ------------------------------------------------------------------

void CommandPostSystem::clear() {
    m_posts.clear();
    m_next_id = 1;
    LOG_INFO("CommandPostSystem: cleared");
}

} // namespace swbf
