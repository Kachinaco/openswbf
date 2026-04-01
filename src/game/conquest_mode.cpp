#include "game/conquest_mode.h"
#include "core/log.h"

namespace swbf {

void ConquestMode::init(int starting_tickets) {
    m_active           = true;
    m_game_over        = false;
    m_starting_tickets = starting_tickets;
    m_tickets_team1    = starting_tickets;
    m_tickets_team2    = starting_tickets;
    m_bleed_timer      = 0.0f;
    m_posts_team1      = 0;
    m_posts_team2      = 0;
    LOG_INFO("ConquestMode: started with %d tickets per team", starting_tickets);
}

void ConquestMode::init(const ConquestConfig& config) {
    m_active           = true;
    m_game_over        = false;
    m_starting_tickets = config.initial_tickets;
    m_tickets_team1    = config.initial_tickets;
    m_tickets_team2    = config.initial_tickets;
    m_bleed_rate       = config.bleed_rate;
    m_bleed_threshold  = config.bleed_threshold;
    m_total_posts      = config.total_command_posts;
    m_bleed_timer      = 0.0f;
    m_posts_team1      = 0;
    m_posts_team2      = 0;
    LOG_INFO("ConquestMode: started with %d tickets, bleed_rate=%.1f, threshold=%.1f",
             config.initial_tickets, static_cast<double>(config.bleed_rate),
             static_cast<double>(config.bleed_threshold));
}

void ConquestMode::update(float dt, const CommandPostSystem& cps) {
    if (!m_active || m_game_over) return;

    // Use live CP counts from the system.
    int team1_posts = cps.count_owned_by(TEAM_REPUBLIC);
    int team2_posts = cps.count_owned_by(TEAM_CIS);
    int total       = static_cast<int>(cps.posts().size());

    if (total == 0) return;

    m_bleed_timer += dt;
    if (m_bleed_timer < BLEED_INTERVAL) return;
    m_bleed_timer -= BLEED_INTERVAL;

    if (team1_posts > team2_posts) {
        int bleed = (team1_posts * 3 >= total * 2) ? 2 : 1;
        m_tickets_team2 -= bleed;
        if (m_tickets_team2 < 0) m_tickets_team2 = 0;
    } else if (team2_posts > team1_posts) {
        int bleed = (team2_posts * 3 >= total * 2) ? 2 : 1;
        m_tickets_team1 -= bleed;
        if (m_tickets_team1 < 0) m_tickets_team1 = 0;
    }

    check_win_condition();
}

void ConquestMode::update(float dt) {
    if (!m_active || m_game_over) return;

    int total = m_total_posts;
    if (total == 0) return;

    // Check each team for ticket bleed using pre-set post counts.
    auto check_bleed = [&](int& team_tickets, int team_posts) {
        float fraction = static_cast<float>(team_posts) / static_cast<float>(total);
        if (fraction < m_bleed_threshold) {
            m_bleed_timer += 0.0f; // accumulation done below
        }
    };
    (void)check_bleed;

    m_bleed_timer += dt;
    float bleed_interval = (m_bleed_rate > 0.0f) ? (1.0f / m_bleed_rate) : 999999.0f;

    while (m_bleed_timer >= bleed_interval) {
        m_bleed_timer -= bleed_interval;

        float frac1 = static_cast<float>(m_posts_team1) / static_cast<float>(total);
        float frac2 = static_cast<float>(m_posts_team2) / static_cast<float>(total);

        if (frac1 < m_bleed_threshold && m_tickets_team1 > 0) {
            --m_tickets_team1;
        }
        if (frac2 < m_bleed_threshold && m_tickets_team2 > 0) {
            --m_tickets_team2;
        }
    }

    check_win_condition();
}

void ConquestMode::set_post_counts(int team, int posts_owned) {
    switch (team) {
        case TEAM_REPUBLIC: m_posts_team1 = posts_owned; break;
        case TEAM_CIS:      m_posts_team2 = posts_owned; break;
        default: break;
    }
}

void ConquestMode::set_bleed_rate(float rate) {
    m_bleed_rate = rate;
    LOG_INFO("ConquestMode: bleed rate set to %.2f", static_cast<double>(rate));
}

int ConquestMode::tickets(int team) const {
    switch (team) {
        case TEAM_REPUBLIC: return m_tickets_team1;
        case TEAM_CIS:      return m_tickets_team2;
        default:            return 0;
    }
}

void ConquestMode::remove_ticket(int team) {
    if (m_game_over) return;
    switch (team) {
        case TEAM_REPUBLIC:
            if (m_tickets_team1 > 0) --m_tickets_team1;
            break;
        case TEAM_CIS:
            if (m_tickets_team2 > 0) --m_tickets_team2;
            break;
        default:
            break;
    }
    check_win_condition();
}

bool ConquestMode::is_eliminated(int team) const {
    return tickets(team) <= 0;
}

bool ConquestMode::is_game_over() const {
    if (!m_active) return false;
    return m_game_over;
}

int ConquestMode::winner() const {
    if (m_tickets_team1 <= 0 && m_tickets_team2 > 0) return TEAM_CIS;
    if (m_tickets_team2 <= 0 && m_tickets_team1 > 0) return TEAM_REPUBLIC;
    if (m_tickets_team1 <= 0 && m_tickets_team2 <= 0) return TEAM_NEUTRAL;
    return TEAM_NEUTRAL;
}

void ConquestMode::set_win_callback(WinCallback cb) {
    m_win_callback = std::move(cb);
}

void ConquestMode::check_win_condition() {
    if (m_game_over) return;

    bool t1_alive = m_tickets_team1 > 0;
    bool t2_alive = m_tickets_team2 > 0;

    if (!t1_alive || !t2_alive) {
        m_game_over = true;
        if (m_win_callback) {
            m_win_callback(winner());
        }
    }
}

void ConquestMode::clear() {
    m_active = false;
    m_game_over = false;
    m_starting_tickets = 200;
    m_tickets_team1 = 200;
    m_tickets_team2 = 200;
    m_bleed_rate = 1.0f;
    m_bleed_timer = 0.0f;
    m_bleed_threshold = 0.5f;
    m_total_posts = 5;
    m_posts_team1 = 0;
    m_posts_team2 = 0;
    LOG_INFO("ConquestMode: cleared");
}

} // namespace swbf
