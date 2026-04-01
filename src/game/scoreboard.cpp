#include "game/scoreboard.h"

#include <algorithm>
#include <cstdio>

namespace swbf {

void Scoreboard::render(UIRenderer& ui, int screen_w, int screen_h,
                        const ConquestMode& conquest,
                        const EntityManager& entities) {
    if (!m_visible) return;

    float sw = static_cast<float>(screen_w);
    float sh = static_cast<float>(screen_h);

    // Scoreboard dimensions.
    float board_w = sw * 0.7f;
    float board_h = sh * 0.7f;
    float board_x = (sw - board_w) * 0.5f;
    float board_y = (sh - board_h) * 0.5f;

    // Semi-transparent background.
    ui.draw_rect(board_x, board_y, board_w, board_h,
                 0.0f, 0.0f, 0.0f, 0.75f);

    // Border.
    ui.draw_rect_outline(board_x, board_y, board_w, board_h,
                         0.4f, 0.5f, 0.4f, 0.8f, 2.0f);

    // Title.
    const char* title = "SCOREBOARD";
    float title_w = ui.text_width(title, 3.0f);
    ui.draw_text_shadow(title, board_x + (board_w - title_w) * 0.5f,
                        board_y + 15.0f,
                        1.0f, 1.0f, 0.8f, 1.0f, 3.0f);

    // Divider line under title.
    float div_y = board_y + 50.0f;
    ui.draw_rect(board_x + 10.0f, div_y, board_w - 20.0f, 2.0f,
                 0.4f, 0.5f, 0.4f, 0.6f);

    // Two columns: Republic (left), CIS (right).
    float col_w = (board_w - 30.0f) * 0.5f;
    float col_y = div_y + 10.0f;

    auto rep_players = entities.on_team(TEAM_REPUBLIC);
    auto cis_players = entities.on_team(TEAM_CIS);

    // Sort by score descending.
    auto sort_fn = [](const Entity* a, const Entity* b) {
        return a->score > b->score;
    };
    std::sort(rep_players.begin(), rep_players.end(), sort_fn);
    std::sort(cis_players.begin(), cis_players.end(), sort_fn);

    render_team_column(ui, board_x + 10.0f, col_y, col_w,
                       "REPUBLIC", 0.3f, 0.6f, 1.0f,
                       conquest.tickets(Team::REPUBLIC),
                       rep_players);

    render_team_column(ui, board_x + col_w + 20.0f, col_y, col_w,
                       "CIS", 1.0f, 0.3f, 0.3f,
                       conquest.tickets(Team::CIS),
                       cis_players);
}

void Scoreboard::render_team_column(UIRenderer& ui,
                                     float x, float y, float col_w,
                                     const char* team_name,
                                     float tr, float tg, float tb,
                                     int tickets,
                                     const std::vector<const Entity*>& players) {
    float scale = 2.0f;
    float line_h = 9.0f * scale;

    // Column background.
    float col_h = line_h * static_cast<float>(players.size() + 4) + 10.0f;
    ui.draw_rect(x, y, col_w, col_h, 0.05f, 0.05f, 0.1f, 0.4f);

    // Team name header.
    ui.draw_text_shadow(team_name, x + 5.0f, y + 5.0f,
                        tr, tg, tb, 1.0f, 2.5f);

    // Tickets.
    char ticket_buf[32];
    std::snprintf(ticket_buf, sizeof(ticket_buf), "Tickets: %d", tickets);
    ui.draw_text(ticket_buf, x + col_w - ui.text_width(ticket_buf, scale) - 5.0f,
                 y + 5.0f, 0.8f, 0.8f, 0.8f, 0.9f, scale);

    // Column headers.
    float header_y = y + 5.0f + line_h * 1.5f;
    ui.draw_text("Name", x + 5.0f, header_y,
                 0.7f, 0.7f, 0.7f, 0.8f, scale);
    ui.draw_text("K", x + col_w * 0.55f, header_y,
                 0.7f, 0.7f, 0.7f, 0.8f, scale);
    ui.draw_text("D", x + col_w * 0.68f, header_y,
                 0.7f, 0.7f, 0.7f, 0.8f, scale);
    ui.draw_text("Score", x + col_w * 0.80f, header_y,
                 0.7f, 0.7f, 0.7f, 0.8f, scale);

    // Divider.
    float sep_y = header_y + line_h + 2.0f;
    ui.draw_rect(x + 3.0f, sep_y, col_w - 6.0f, 1.0f,
                 0.3f, 0.3f, 0.3f, 0.5f);

    // Player rows.
    float row_y = sep_y + 4.0f;
    int total_kills = 0, total_deaths = 0, total_score = 0;

    for (const auto* player : players) {
        // Highlight the local player row.
        float row_r = 0.8f, row_g = 0.8f, row_b = 0.8f, row_a = 0.9f;
        if (player->is_player) {
            ui.draw_rect(x + 2.0f, row_y - 1.0f, col_w - 4.0f, line_h,
                         tr * 0.3f, tg * 0.3f, tb * 0.3f, 0.3f);
            row_r = 1.0f; row_g = 1.0f; row_b = 0.8f;
        }

        // Truncate long names.
        char name_buf[20];
        std::snprintf(name_buf, sizeof(name_buf), "%.16s", player->name.c_str());

        char k_buf[8], d_buf[8], s_buf[8];
        std::snprintf(k_buf, sizeof(k_buf), "%d", player->kills);
        std::snprintf(d_buf, sizeof(d_buf), "%d", player->deaths);
        std::snprintf(s_buf, sizeof(s_buf), "%d", player->score);

        ui.draw_text(name_buf, x + 5.0f, row_y,
                     row_r, row_g, row_b, row_a, scale);
        ui.draw_text(k_buf, x + col_w * 0.55f, row_y,
                     row_r, row_g, row_b, row_a, scale);
        ui.draw_text(d_buf, x + col_w * 0.68f, row_y,
                     row_r, row_g, row_b, row_a, scale);
        ui.draw_text(s_buf, x + col_w * 0.80f, row_y,
                     row_r, row_g, row_b, row_a, scale);

        total_kills  += player->kills;
        total_deaths += player->deaths;
        total_score  += player->score;

        row_y += line_h;
    }

    // Team totals row.
    row_y += 4.0f;
    ui.draw_rect(x + 3.0f, row_y - 2.0f, col_w - 6.0f, 1.0f,
                 0.3f, 0.3f, 0.3f, 0.5f);
    row_y += 4.0f;

    char tk_buf[8], td_buf[8], ts_buf[8];
    std::snprintf(tk_buf, sizeof(tk_buf), "%d", total_kills);
    std::snprintf(td_buf, sizeof(td_buf), "%d", total_deaths);
    std::snprintf(ts_buf, sizeof(ts_buf), "%d", total_score);

    ui.draw_text("TOTAL", x + 5.0f, row_y,
                 tr, tg, tb, 0.9f, scale);
    ui.draw_text(tk_buf, x + col_w * 0.55f, row_y,
                 tr, tg, tb, 0.9f, scale);
    ui.draw_text(td_buf, x + col_w * 0.68f, row_y,
                 tr, tg, tb, 0.9f, scale);
    ui.draw_text(ts_buf, x + col_w * 0.80f, row_y,
                 tr, tg, tb, 0.9f, scale);
}

} // namespace swbf
