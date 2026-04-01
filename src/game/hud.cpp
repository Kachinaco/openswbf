#include "game/hud.h"
#include "core/log.h"

#include <algorithm>
#include <cstdio>

namespace swbf {

bool HUD::init() {
    if (!m_ui.init()) {
        LOG_ERROR("HUD: failed to initialise UIRenderer");
        return false;
    }

    // Position the minimap in the top-right corner.
    // (Will be repositioned in render() based on actual screen size.)
    m_minimap.set_position(10.0f, 10.0f, 160.0f);
    m_minimap.set_world_radius(120.0f);

    LOG_INFO("HUD initialised");
    return true;
}

void HUD::destroy() {
    m_ui.destroy();
}

void HUD::update(float dt) {
    // Decay kill feed entries.
    for (auto& entry : m_kill_feed) {
        entry.timer -= dt;
    }
    // Remove expired entries.
    m_kill_feed.erase(
        std::remove_if(m_kill_feed.begin(), m_kill_feed.end(),
                        [](const KillFeedEntry& e) { return e.timer <= 0.0f; }),
        m_kill_feed.end());
}

void HUD::add_kill(const std::string& killer, const std::string& victim,
                   Team killer_team) {
    KillFeedEntry entry;
    entry.killer      = killer;
    entry.victim      = victim;
    entry.killer_team = killer_team;
    entry.timer       = 5.0f;
    m_kill_feed.push_back(entry);

    // Keep at most 5 entries.
    while (m_kill_feed.size() > 5) {
        m_kill_feed.erase(m_kill_feed.begin());
    }
}

void HUD::render(int screen_w, int screen_h,
                 const HealthSystem& health,
                 const WeaponSystem& weapon,
                 const ConquestMode& conquest,
                 const CommandPostSystem& cps,
                 const EntityManager& entities,
                 const float* player_pos, float player_yaw,
                 Team player_team) {
    m_ui.begin(screen_w, screen_h);

    // Position minimap in top-right corner.
    float minimap_size = 160.0f;
    m_minimap.set_position(static_cast<float>(screen_w) - minimap_size - 10.0f,
                           10.0f, minimap_size);

    render_tickets(screen_w, screen_h, conquest);
    render_health_bar(screen_w, screen_h, health);
    render_ammo(screen_w, screen_h, weapon);
    render_crosshair(screen_w, screen_h);
    render_kill_feed(screen_w, screen_h);
    render_capture_progress(screen_w, screen_h, cps, player_pos);

    m_minimap.render(m_ui, player_pos, player_yaw, player_team, cps, entities);

    m_ui.flush();
}

// =========================================================================
// Health bar — bottom left
// =========================================================================

void HUD::render_health_bar(int screen_w, int screen_h,
                             const HealthSystem& health) {
    (void)screen_w;

    float sh = static_cast<float>(screen_h);

    float bar_w = 200.0f;
    float bar_h = 20.0f;
    float bar_x = 20.0f;
    float bar_y = sh - 50.0f;

    // Background.
    m_ui.draw_rect(bar_x, bar_y, bar_w, bar_h,
                   0.1f, 0.1f, 0.1f, 0.7f);

    // Health fill.
    float frac = health.health_fraction();
    float fill_w = bar_w * frac;

    // Color: green when healthy, yellow when hurt, red when critical.
    float hr = 0.2f, hg = 0.8f, hb = 0.2f;
    if (frac < 0.5f) {
        hr = 1.0f; hg = 0.8f * frac * 2.0f; hb = 0.1f;
    }
    if (frac < 0.25f) {
        hr = 1.0f; hg = 0.1f; hb = 0.1f;
    }

    m_ui.draw_rect(bar_x, bar_y, fill_w, bar_h, hr, hg, hb, 0.9f);

    // Border.
    m_ui.draw_rect_outline(bar_x, bar_y, bar_w, bar_h,
                           0.5f, 0.5f, 0.5f, 0.8f, 2.0f);

    // Health text.
    char hp_buf[32];
    std::snprintf(hp_buf, sizeof(hp_buf), "HP: %.0f/%.0f",
                  static_cast<double>(health.health()),
                  static_cast<double>(health.max_health()));
    m_ui.draw_text_shadow(hp_buf, bar_x + 5.0f, bar_y + 3.0f,
                          1.0f, 1.0f, 1.0f, 0.9f, 2.0f);

    // Health icon (a small "+" cross left of the bar).
    float icon_x = bar_x - 22.0f;
    float icon_y = bar_y + bar_h * 0.5f;
    m_ui.draw_rect(icon_x, icon_y - 6.0f, 14.0f, 4.0f,
                   1.0f, 0.2f, 0.2f, 0.9f);
    m_ui.draw_rect(icon_x + 5.0f, icon_y - 11.0f, 4.0f, 14.0f,
                   1.0f, 0.2f, 0.2f, 0.9f);
}

// =========================================================================
// Ammo counter — bottom right
// =========================================================================

void HUD::render_ammo(int screen_w, int screen_h,
                      const WeaponSystem& weapon) {
    float sw = static_cast<float>(screen_w);
    float sh = static_cast<float>(screen_h);

    float text_scale = 2.0f;
    float big_scale  = 3.0f;

    // Ammo count in large text.
    char ammo_buf[32];
    std::snprintf(ammo_buf, sizeof(ammo_buf), "%d/%d",
                  weapon.ammo(), weapon.max_ammo());

    float ammo_w = m_ui.text_width(ammo_buf, big_scale);
    float ammo_x = sw - ammo_w - 30.0f;
    float ammo_y = sh - 55.0f;

    // Background panel.
    float panel_w = ammo_w + 40.0f;
    float panel_h = 50.0f;
    m_ui.draw_rect(ammo_x - 10.0f, ammo_y - 5.0f, panel_w, panel_h,
                   0.0f, 0.0f, 0.0f, 0.5f);
    m_ui.draw_rect_outline(ammo_x - 10.0f, ammo_y - 5.0f, panel_w, panel_h,
                           0.4f, 0.4f, 0.5f, 0.6f, 1.0f);

    // Ammo numbers.
    float ammo_r = 1.0f, ammo_g = 1.0f, ammo_b = 1.0f;
    if (weapon.ammo_fraction() < 0.2f) {
        ammo_r = 1.0f; ammo_g = 0.3f; ammo_b = 0.3f;
    }
    m_ui.draw_text_shadow(ammo_buf, ammo_x, ammo_y,
                          ammo_r, ammo_g, ammo_b, 1.0f, big_scale);

    // Weapon name.
    float name_w = m_ui.text_width(weapon.weapon_name().c_str(), text_scale);
    m_ui.draw_text(weapon.weapon_name().c_str(),
                   ammo_x + (ammo_w - name_w) * 0.5f,
                   ammo_y + big_scale * 9.0f + 2.0f,
                   0.7f, 0.7f, 0.8f, 0.7f, text_scale);
}

// =========================================================================
// Crosshair — center of screen
// =========================================================================

void HUD::render_crosshair(int screen_w, int screen_h) {
    float cx = static_cast<float>(screen_w) * 0.5f;
    float cy = static_cast<float>(screen_h) * 0.5f;

    float gap = 4.0f;
    float len = 12.0f;
    float thick = 2.0f;

    // Four crosshair lines with a central gap.
    // Top
    m_ui.draw_rect(cx - thick * 0.5f, cy - gap - len, thick, len,
                   1.0f, 1.0f, 1.0f, 0.8f);
    // Bottom
    m_ui.draw_rect(cx - thick * 0.5f, cy + gap, thick, len,
                   1.0f, 1.0f, 1.0f, 0.8f);
    // Left
    m_ui.draw_rect(cx - gap - len, cy - thick * 0.5f, len, thick,
                   1.0f, 1.0f, 1.0f, 0.8f);
    // Right
    m_ui.draw_rect(cx + gap, cy - thick * 0.5f, len, thick,
                   1.0f, 1.0f, 1.0f, 0.8f);

    // Center dot.
    m_ui.draw_rect(cx - 1.0f, cy - 1.0f, 2.0f, 2.0f,
                   1.0f, 1.0f, 1.0f, 0.6f);
}

// =========================================================================
// Team tickets — top center
// =========================================================================

void HUD::render_tickets(int screen_w, int /*screen_h*/,
                         const ConquestMode& conquest) {
    float sw = static_cast<float>(screen_w);
    float scale = 2.5f;

    // Panel dimensions.
    float panel_w = 300.0f;
    float panel_h = 40.0f;
    float panel_x = (sw - panel_w) * 0.5f;
    float panel_y = 8.0f;

    // Background.
    m_ui.draw_rect(panel_x, panel_y, panel_w, panel_h,
                   0.0f, 0.0f, 0.0f, 0.6f);
    m_ui.draw_rect_outline(panel_x, panel_y, panel_w, panel_h,
                           0.3f, 0.3f, 0.4f, 0.5f, 1.0f);

    // Republic tickets (left side, blue).
    char rep_buf[16];
    std::snprintf(rep_buf, sizeof(rep_buf), "%d", conquest.tickets(Team::REPUBLIC));
    m_ui.draw_text_shadow(rep_buf, panel_x + 20.0f, panel_y + 9.0f,
                          0.3f, 0.6f, 1.0f, 1.0f, scale);

    // "vs" in the middle.
    m_ui.draw_text("vs", panel_x + panel_w * 0.5f - 8.0f, panel_y + 12.0f,
                   0.6f, 0.6f, 0.6f, 0.7f, 2.0f);

    // CIS tickets (right side, red).
    char cis_buf[16];
    std::snprintf(cis_buf, sizeof(cis_buf), "%d", conquest.tickets(Team::CIS));
    float cis_w = m_ui.text_width(cis_buf, scale);
    m_ui.draw_text_shadow(cis_buf,
                          panel_x + panel_w - cis_w - 20.0f, panel_y + 9.0f,
                          1.0f, 0.3f, 0.3f, 1.0f, scale);

    // Team labels.
    m_ui.draw_text("REP", panel_x + 20.0f, panel_y + 2.0f,
                   0.3f, 0.5f, 0.8f, 0.5f, 1.5f);
    float cis_label_w = m_ui.text_width("CIS", 1.5f);
    m_ui.draw_text("CIS", panel_x + panel_w - cis_label_w - 20.0f, panel_y + 2.0f,
                   0.8f, 0.3f, 0.3f, 0.5f, 1.5f);
}

// =========================================================================
// Kill feed — upper right, below minimap
// =========================================================================

void HUD::render_kill_feed(int screen_w, int /*screen_h*/) {
    if (m_kill_feed.empty()) return;

    float sw = static_cast<float>(screen_w);
    float scale = 1.5f;
    float line_h = 9.0f * scale;
    float feed_x = sw - 300.0f;
    float feed_y = 180.0f; // below minimap

    for (size_t i = 0; i < m_kill_feed.size(); ++i) {
        const auto& entry = m_kill_feed[i];
        float alpha = entry.timer < 1.0f ? entry.timer : 1.0f;

        float y = feed_y + static_cast<float>(i) * (line_h + 2.0f);

        // Background stripe.
        m_ui.draw_rect(feed_x, y - 1.0f, 290.0f, line_h + 2.0f,
                       0.0f, 0.0f, 0.0f, 0.3f * alpha);

        // Killer name (colored by team).
        float kr = 0.8f, kg = 0.8f, kb = 0.8f;
        switch (entry.killer_team) {
            case Team::REPUBLIC: kr = 0.3f; kg = 0.6f; kb = 1.0f; break;
            case Team::CIS:     kr = 1.0f; kg = 0.3f; kb = 0.3f; break;
            default: break;
        }
        m_ui.draw_text(entry.killer.c_str(), feed_x + 5.0f, y,
                       kr, kg, kb, alpha, scale);

        // " killed " text.
        float killer_w = m_ui.text_width(entry.killer.c_str(), scale);
        m_ui.draw_text(" > ", feed_x + 5.0f + killer_w, y,
                       0.7f, 0.7f, 0.7f, alpha * 0.8f, scale);

        // Victim name.
        float arrow_w = m_ui.text_width(" > ", scale);
        m_ui.draw_text(entry.victim.c_str(),
                       feed_x + 5.0f + killer_w + arrow_w, y,
                       0.9f, 0.9f, 0.9f, alpha, scale);
    }
}

// =========================================================================
// Capture progress — center of screen when near a command post
// =========================================================================

void HUD::render_capture_progress(int screen_w, int screen_h,
                                   const CommandPostSystem& cps,
                                   const float* player_pos) {
    const CommandPost* nearby = cps.post_near_player(player_pos);
    if (!nearby) return;
    if (nearby->capture_progress <= 0.0f) return;

    float cx = static_cast<float>(screen_w) * 0.5f;
    float cy = static_cast<float>(screen_h) * 0.5f + 60.0f;

    float bar_w = 200.0f;
    float bar_h = 14.0f;

    // Background.
    m_ui.draw_rect(cx - bar_w * 0.5f, cy, bar_w, bar_h,
                   0.1f, 0.1f, 0.1f, 0.6f);

    // Fill.
    float fill_w = bar_w * nearby->capture_progress;
    float cr = 1.0f, cg = 0.8f, cb = 0.0f;
    m_ui.draw_rect(cx - bar_w * 0.5f, cy, fill_w, bar_h,
                   cr, cg, cb, 0.8f);

    // Border.
    m_ui.draw_rect_outline(cx - bar_w * 0.5f, cy, bar_w, bar_h,
                           0.5f, 0.5f, 0.5f, 0.7f, 1.0f);

    // "Capturing..." text above the bar.
    const char* cap_text = "CAPTURING";
    float tw = m_ui.text_width(cap_text, 2.0f);
    m_ui.draw_text_shadow(cap_text, cx - tw * 0.5f, cy - 18.0f,
                          1.0f, 0.9f, 0.3f, 0.9f, 2.0f);

    // Post name below the bar.
    float nw = m_ui.text_width(nearby->name.c_str(), 1.5f);
    m_ui.draw_text(nearby->name.c_str(), cx - nw * 0.5f, cy + bar_h + 3.0f,
                   0.7f, 0.7f, 0.7f, 0.7f, 1.5f);
}

} // namespace swbf
