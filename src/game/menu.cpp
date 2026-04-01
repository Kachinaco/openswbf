#include "game/menu.h"
#include "core/log.h"

#include <SDL.h>
#include <cmath>
#include <cstdio>

namespace swbf {

// =========================================================================
// Initialization
// =========================================================================

void MenuSystem::init() {
    build_menus();
    m_screen = MenuScreen::MAIN_MENU;
    m_cursor = 0;
    m_quit   = false;
    m_game_started = false;
    LOG_INFO("MenuSystem initialised");
}

void MenuSystem::build_menus() {
    // -- Main menu --
    m_main_items.clear();
    m_main_items.push_back({"Play",     true, 1});
    m_main_items.push_back({"Settings", true, 2});
    m_main_items.push_back({"Quit",     true, 3});

    // -- Pause menu --
    m_pause_items.clear();
    m_pause_items.push_back({"Resume",   true, 1});
    m_pause_items.push_back({"Settings", true, 2});
    m_pause_items.push_back({"Quit to Menu", true, 3});

    // -- Map selection --
    m_map_items.clear();
    m_map_items.push_back({"Naboo: Plains",        true, 0});
    m_map_items.push_back({"Geonosis: Spire",      true, 1});
    m_map_items.push_back({"Hoth: Echo Base",       true, 2});
    m_map_items.push_back({"Endor: Bunker",         true, 3});
    m_map_items.push_back({"Tatooine: Dune Sea",    true, 4});
    m_map_items.push_back({"Kashyyyk: Docks",       true, 5});
    m_map_items.push_back({"Kamino: Tipoca City",   true, 6});
    m_map_items.push_back({"Bespin: Cloud City",    true, 7});
    m_map_items.push_back({"Yavin 4: Temple",       true, 8});
    m_map_items.push_back({"Rhen Var: Harbor",      true, 9});

    // -- Team/class selection --
    m_team_items.clear();
    m_team_items.push_back({"Republic - Soldier",        true, 0});
    m_team_items.push_back({"Republic - Heavy Trooper",  true, 1});
    m_team_items.push_back({"Republic - Sniper",         true, 2});
    m_team_items.push_back({"Republic - Engineer",       true, 3});
    m_team_items.push_back({"CIS - Super Battle Droid",  true, 4});
    m_team_items.push_back({"CIS - Assault Droid",       true, 5});
    m_team_items.push_back({"CIS - Assassin Droid",      true, 6});
    m_team_items.push_back({"CIS - Engineer Droid",      true, 7});

    // -- Settings --
    m_settings_items.clear();
    m_settings_items.push_back({"Resolution",  true, 1});
    m_settings_items.push_back({"Volume",      true, 2});
    m_settings_items.push_back({"Back",        true, 3});
}

void MenuSystem::show_screen(MenuScreen screen) {
    m_prev_screen = m_screen;
    m_screen = screen;
    m_cursor = 0;
}

// =========================================================================
// Update (input handling)
// =========================================================================

bool MenuSystem::update(const InputSystem& input, float dt) {
    m_cursor_blink += dt * 3.0f;
    if (m_cursor_blink > 6.28f) m_cursor_blink -= 6.28f;

    if (m_screen == MenuScreen::NONE) {
        // Check for Escape to open pause menu.
        if (input.key_pressed(SDL_SCANCODE_ESCAPE)) {
            show_screen(MenuScreen::PAUSE);
            return true;
        }
        return false;
    }

    // Navigation.
    if (input.key_pressed(SDL_SCANCODE_UP) ||
        input.key_pressed(SDL_SCANCODE_W)) {
        move_cursor(-1);
        return true;
    }
    if (input.key_pressed(SDL_SCANCODE_DOWN) ||
        input.key_pressed(SDL_SCANCODE_S)) {
        move_cursor(1);
        return true;
    }

    // Selection.
    if (input.key_pressed(SDL_SCANCODE_RETURN) ||
        input.key_pressed(SDL_SCANCODE_SPACE)) {
        select_current();
        return true;
    }

    // Back.
    if (input.key_pressed(SDL_SCANCODE_ESCAPE)) {
        switch (m_screen) {
            case MenuScreen::PAUSE:
                m_screen = MenuScreen::NONE;
                break;
            case MenuScreen::MAP_SELECT:
                m_screen = MenuScreen::MAIN_MENU;
                m_cursor = 0;
                break;
            case MenuScreen::TEAM_SELECT:
                m_screen = MenuScreen::MAP_SELECT;
                m_cursor = 0;
                break;
            case MenuScreen::SETTINGS:
                m_screen = m_prev_screen;
                m_cursor = 0;
                break;
            default:
                break;
        }
        return true;
    }

    // Settings-specific: left/right to change values.
    if (m_screen == MenuScreen::SETTINGS) {
        bool left  = input.key_pressed(SDL_SCANCODE_LEFT) ||
                     input.key_pressed(SDL_SCANCODE_A);
        bool right = input.key_pressed(SDL_SCANCODE_RIGHT) ||
                     input.key_pressed(SDL_SCANCODE_D);

        if (m_cursor == 0 && (left || right)) {
            // Resolution.
            if (left && m_resolution_idx > 0) --m_resolution_idx;
            if (right && m_resolution_idx < 4) ++m_resolution_idx;
            return true;
        }
        if (m_cursor == 1 && (left || right)) {
            // Volume.
            if (left && m_volume_pct > 0) m_volume_pct -= 10;
            if (right && m_volume_pct < 100) m_volume_pct += 10;
            return true;
        }
    }

    return true; // consume all input while menu is active
}

void MenuSystem::move_cursor(int delta) {
    const std::vector<MenuItem>* items = nullptr;
    switch (m_screen) {
        case MenuScreen::MAIN_MENU:   items = &m_main_items; break;
        case MenuScreen::PAUSE:       items = &m_pause_items; break;
        case MenuScreen::MAP_SELECT:  items = &m_map_items; break;
        case MenuScreen::TEAM_SELECT: items = &m_team_items; break;
        case MenuScreen::SETTINGS:    items = &m_settings_items; break;
        default: return;
    }
    if (!items || items->empty()) return;

    m_cursor += delta;
    if (m_cursor < 0) m_cursor = static_cast<int>(items->size()) - 1;
    if (m_cursor >= static_cast<int>(items->size())) m_cursor = 0;

    // Skip disabled items.
    int attempts = static_cast<int>(items->size());
    while (!(*items)[static_cast<size_t>(m_cursor)].enabled && attempts > 0) {
        m_cursor += delta > 0 ? 1 : -1;
        if (m_cursor < 0) m_cursor = static_cast<int>(items->size()) - 1;
        if (m_cursor >= static_cast<int>(items->size())) m_cursor = 0;
        --attempts;
    }
}

void MenuSystem::select_current() {
    switch (m_screen) {
        case MenuScreen::MAIN_MENU:
            switch (m_main_items[static_cast<size_t>(m_cursor)].action_id) {
                case 1: // Play
                    show_screen(MenuScreen::MAP_SELECT);
                    break;
                case 2: // Settings
                    m_prev_screen = MenuScreen::MAIN_MENU;
                    show_screen(MenuScreen::SETTINGS);
                    break;
                case 3: // Quit
                    m_quit = true;
                    break;
            }
            break;

        case MenuScreen::MAP_SELECT:
            m_selected_map = m_map_items[static_cast<size_t>(m_cursor)].action_id;
            show_screen(MenuScreen::TEAM_SELECT);
            break;

        case MenuScreen::TEAM_SELECT: {
            int idx = m_team_items[static_cast<size_t>(m_cursor)].action_id;
            m_selected_team  = (idx < 4) ? 1 : 2;
            m_selected_class = idx % 4;
            m_game_started   = true;
            m_screen = MenuScreen::NONE;
            LOG_INFO("Game starting: map=%d team=%d class=%d",
                     m_selected_map, m_selected_team, m_selected_class);
            break;
        }

        case MenuScreen::PAUSE:
            switch (m_pause_items[static_cast<size_t>(m_cursor)].action_id) {
                case 1: // Resume
                    m_screen = MenuScreen::NONE;
                    break;
                case 2: // Settings
                    m_prev_screen = MenuScreen::PAUSE;
                    show_screen(MenuScreen::SETTINGS);
                    break;
                case 3: // Quit to Menu
                    show_screen(MenuScreen::MAIN_MENU);
                    break;
            }
            break;

        case MenuScreen::SETTINGS:
            if (m_settings_items[static_cast<size_t>(m_cursor)].action_id == 3) {
                // Back
                m_screen = m_prev_screen;
                m_cursor = 0;
            }
            break;

        default:
            break;
    }
}

// =========================================================================
// Rendering
// =========================================================================

void MenuSystem::render(UIRenderer& ui, int screen_w, int screen_h) {
    if (m_screen == MenuScreen::NONE) return;

    render_background(ui, screen_w, screen_h);

    switch (m_screen) {
        case MenuScreen::MAIN_MENU:
            render_menu_list(ui, screen_w, screen_h,
                             "STAR WARS BATTLEFRONT", m_main_items);
            break;
        case MenuScreen::MAP_SELECT:
            render_menu_list(ui, screen_w, screen_h,
                             "SELECT MAP", m_map_items);
            break;
        case MenuScreen::TEAM_SELECT:
            render_menu_list(ui, screen_w, screen_h,
                             "SELECT TEAM & CLASS", m_team_items);
            break;
        case MenuScreen::PAUSE:
            render_menu_list(ui, screen_w, screen_h,
                             "PAUSED", m_pause_items);
            break;
        case MenuScreen::SETTINGS:
            render_settings(ui, screen_w, screen_h);
            break;
        default:
            break;
    }
}

void MenuSystem::render_background(UIRenderer& ui, int screen_w, int screen_h) {
    float sw = static_cast<float>(screen_w);
    float sh = static_cast<float>(screen_h);

    // Full-screen dark overlay.
    ui.draw_rect(0.0f, 0.0f, sw, sh, 0.0f, 0.02f, 0.05f, 0.85f);

    // Subtle top/bottom bars for a cinematic feel.
    ui.draw_rect(0.0f, 0.0f, sw, 4.0f, 0.2f, 0.4f, 0.8f, 0.4f);
    ui.draw_rect(0.0f, sh - 4.0f, sw, 4.0f, 0.2f, 0.4f, 0.8f, 0.4f);
}

void MenuSystem::render_menu_list(UIRenderer& ui, int screen_w, int screen_h,
                                   const char* title,
                                   const std::vector<MenuItem>& items) {
    float sw = static_cast<float>(screen_w);
    float sh = static_cast<float>(screen_h);

    // Title.
    float title_scale = 3.5f;
    float title_w = ui.text_width(title, title_scale);
    float title_x = (sw - title_w) * 0.5f;
    float title_y = sh * 0.15f;

    ui.draw_text_shadow(title, title_x, title_y,
                        0.8f, 0.85f, 1.0f, 1.0f, title_scale);

    // Decorative line under title.
    float line_w = title_w + 40.0f;
    float line_x = (sw - line_w) * 0.5f;
    float line_y = title_y + title_scale * 9.0f + 8.0f;
    ui.draw_rect(line_x, line_y, line_w, 2.0f, 0.3f, 0.5f, 0.8f, 0.5f);

    // Menu items.
    float item_scale = 2.5f;
    float line_h = 9.0f * item_scale + 8.0f;
    float items_start_y = line_y + 20.0f;

    for (size_t i = 0; i < items.size(); ++i) {
        float item_w = ui.text_width(items[i].label.c_str(), item_scale);
        float item_x = (sw - item_w) * 0.5f;
        float item_y = items_start_y + static_cast<float>(i) * line_h;

        bool selected = (static_cast<int>(i) == m_cursor);
        float ir, ig, ib, ia;

        if (!items[i].enabled) {
            ir = 0.3f; ig = 0.3f; ib = 0.3f; ia = 0.5f;
        } else if (selected) {
            float pulse = 0.8f + 0.2f * std::sin(m_cursor_blink);
            ir = 1.0f; ig = 0.9f; ib = 0.5f; ia = pulse;
        } else {
            ir = 0.7f; ig = 0.7f; ib = 0.8f; ia = 0.9f;
        }

        ui.draw_text_shadow(items[i].label.c_str(), item_x, item_y,
                            ir, ig, ib, ia, item_scale);

        // Selection indicator (arrow).
        if (selected) {
            float arrow_x = item_x - 25.0f;
            float arrow_cy = item_y + item_scale * 3.5f;
            ui.draw_triangle(arrow_x, arrow_cy,
                             arrow_x - 10.0f, arrow_cy - 7.0f,
                             arrow_x - 10.0f, arrow_cy + 7.0f,
                             1.0f, 0.9f, 0.4f, ia);
        }
    }

    // Navigation hints at the bottom.
    const char* nav_hint = "UP/DOWN: Navigate   ENTER: Select   ESC: Back";
    float hint_w = ui.text_width(nav_hint, 1.5f);
    ui.draw_text(nav_hint, (sw - hint_w) * 0.5f, sh - 40.0f,
                 0.5f, 0.5f, 0.6f, 0.5f, 1.5f);
}

void MenuSystem::render_settings(UIRenderer& ui, int screen_w, int screen_h) {
    float sw = static_cast<float>(screen_w);
    float sh = static_cast<float>(screen_h);

    // Title.
    float title_scale = 3.5f;
    const char* title = "SETTINGS";
    float title_w = ui.text_width(title, title_scale);
    ui.draw_text_shadow(title, (sw - title_w) * 0.5f, sh * 0.15f,
                        0.8f, 0.85f, 1.0f, 1.0f, title_scale);

    float item_scale = 2.5f;
    float line_h = 9.0f * item_scale + 12.0f;
    float start_y = sh * 0.30f;
    float label_x = sw * 0.25f;
    float value_x = sw * 0.55f;

    // Resolution list.
    const char* resolutions[] = {
        "800x600", "1024x768", "1280x720", "1920x1080", "2560x1440"
    };

    // -- Resolution row --
    {
        bool selected = (m_cursor == 0);
        float pulse = selected ? 0.8f + 0.2f * std::sin(m_cursor_blink) : 0.9f;
        float cr = selected ? 1.0f : 0.7f;
        float cg = selected ? 0.9f : 0.7f;
        float cb = selected ? 0.5f : 0.8f;

        ui.draw_text_shadow("Resolution:", label_x, start_y,
                            cr, cg, cb, pulse, item_scale);

        // Left/right arrows around the value.
        if (selected) {
            ui.draw_text("<", value_x - 20.0f, start_y,
                         1.0f, 0.9f, 0.4f, pulse, item_scale);
        }
        ui.draw_text_shadow(resolutions[m_resolution_idx], value_x, start_y,
                            0.9f, 0.9f, 0.9f, pulse, item_scale);
        if (selected) {
            float rw = ui.text_width(resolutions[m_resolution_idx], item_scale);
            ui.draw_text(">", value_x + rw + 10.0f, start_y,
                         1.0f, 0.9f, 0.4f, pulse, item_scale);
        }

        if (selected) {
            ui.draw_triangle(label_x - 25.0f, start_y + item_scale * 3.5f,
                             label_x - 35.0f, start_y + item_scale * 3.5f - 7.0f,
                             label_x - 35.0f, start_y + item_scale * 3.5f + 7.0f,
                             1.0f, 0.9f, 0.4f, pulse);
        }
    }

    // -- Volume row --
    {
        float row_y = start_y + line_h;
        bool selected = (m_cursor == 1);
        float pulse = selected ? 0.8f + 0.2f * std::sin(m_cursor_blink) : 0.9f;
        float cr = selected ? 1.0f : 0.7f;
        float cg = selected ? 0.9f : 0.7f;
        float cb = selected ? 0.5f : 0.8f;

        ui.draw_text_shadow("Volume:", label_x, row_y,
                            cr, cg, cb, pulse, item_scale);

        // Volume bar.
        float bar_w = 150.0f;
        float bar_h = 14.0f;
        float bar_y = row_y + item_scale * 2.0f;

        ui.draw_rect(value_x, bar_y, bar_w, bar_h,
                     0.15f, 0.15f, 0.2f, 0.7f);
        float fill = bar_w * static_cast<float>(m_volume_pct) / 100.0f;
        ui.draw_rect(value_x, bar_y, fill, bar_h,
                     0.3f, 0.6f, 0.9f, 0.8f);
        ui.draw_rect_outline(value_x, bar_y, bar_w, bar_h,
                             0.4f, 0.4f, 0.5f, 0.6f, 1.0f);

        char vol_buf[8];
        std::snprintf(vol_buf, sizeof(vol_buf), "%d%%", m_volume_pct);
        ui.draw_text(vol_buf, value_x + bar_w + 10.0f, row_y,
                     0.9f, 0.9f, 0.9f, pulse, item_scale);

        if (selected) {
            ui.draw_triangle(label_x - 25.0f, row_y + item_scale * 3.5f,
                             label_x - 35.0f, row_y + item_scale * 3.5f - 7.0f,
                             label_x - 35.0f, row_y + item_scale * 3.5f + 7.0f,
                             1.0f, 0.9f, 0.4f, pulse);
        }
    }

    // -- Back button --
    {
        float row_y = start_y + line_h * 2.5f;
        bool selected = (m_cursor == 2);
        float pulse = selected ? 0.8f + 0.2f * std::sin(m_cursor_blink) : 0.9f;
        float cr = selected ? 1.0f : 0.7f;
        float cg = selected ? 0.9f : 0.7f;
        float cb = selected ? 0.5f : 0.8f;

        float back_w = ui.text_width("Back", item_scale);
        float back_x = (sw - back_w) * 0.5f;
        ui.draw_text_shadow("Back", back_x, row_y,
                            cr, cg, cb, pulse, item_scale);

        if (selected) {
            ui.draw_triangle(back_x - 25.0f, row_y + item_scale * 3.5f,
                             back_x - 35.0f, row_y + item_scale * 3.5f - 7.0f,
                             back_x - 35.0f, row_y + item_scale * 3.5f + 7.0f,
                             1.0f, 0.9f, 0.4f, pulse);
        }
    }

    // Navigation hints.
    const char* hints = "UP/DOWN: Navigate   LEFT/RIGHT: Change   ENTER: Select   ESC: Back";
    float hint_w = ui.text_width(hints, 1.5f);
    ui.draw_text(hints, (sw - hint_w) * 0.5f, sh - 40.0f,
                 0.5f, 0.5f, 0.6f, 0.5f, 1.5f);
}

} // namespace swbf
