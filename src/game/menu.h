#pragma once

#include "renderer/ui_renderer.h"
#include "input/input_system.h"
#include "core/types.h"

#include <string>
#include <vector>
#include <functional>

namespace swbf {

/// Identifies which menu screen is currently active.
enum class MenuScreen {
    NONE,           // No menu — game is running
    MAIN_MENU,      // Play, Settings, Quit
    MAP_SELECT,     // Map selection
    TEAM_SELECT,    // Team / class selection
    PAUSE,          // Pause menu (Resume, Settings, Quit)
    SETTINGS,       // Resolution, controls, audio
};

/// A single selectable item in a menu.
struct MenuItem {
    std::string label;
    bool        enabled = true;
    /// Tag for identifying what action to take.
    int         action_id = 0;
};

/// The game's menu system.
///
/// Handles the main menu, pause menu, map selection, team/class selection,
/// and settings.  Renders using UIRenderer and processes keyboard/mouse
/// input for navigation.
///
/// Menu flow:
///   MAIN_MENU -> MAP_SELECT -> TEAM_SELECT -> NONE (game starts)
///   In-game:  Escape -> PAUSE -> (Resume | Settings | Quit)
class MenuSystem {
public:
    /// Initialize the menu system and show the main menu.
    void init();

    /// Process input and update menu state.  Returns true if the menu
    /// consumed the input (game should not process it).
    bool update(const InputSystem& input, float dt);

    /// Render the current menu screen.
    void render(UIRenderer& ui, int screen_w, int screen_h);

    /// Get the current menu screen.
    MenuScreen current_screen() const { return m_screen; }

    /// Check if any menu is active (game should pause input).
    bool is_active() const { return m_screen != MenuScreen::NONE; }

    /// Force a specific screen (e.g., show pause menu on Escape).
    void show_screen(MenuScreen screen);

    /// Close all menus and return to gameplay.
    void close() { m_screen = MenuScreen::NONE; }

    // ----- Settings accessors (read by the game loop) --------------------

    int  resolution_index() const { return m_resolution_idx; }
    int  volume_percent()   const { return m_volume_pct; }
    bool quit_requested()   const { return m_quit; }

    /// Which map was selected (index into the maps list).
    int  selected_map() const { return m_selected_map; }

    /// Which team was selected (1 = Republic, 2 = CIS).
    int  selected_team() const { return m_selected_team; }

    /// Which class was selected (0-3).
    int  selected_class() const { return m_selected_class; }

    /// True after the player has completed the menu flow and is ready to play.
    bool game_started() const { return m_game_started; }

    /// Reset the "game started" flag (after the game has acted on it).
    void clear_game_started() { m_game_started = false; }

private:
    MenuScreen m_screen = MenuScreen::MAIN_MENU;
    int m_cursor = 0;

    // Animation.
    float m_cursor_blink = 0.0f;

    // Settings state.
    int m_resolution_idx = 2; // index into resolution list
    int m_volume_pct     = 80;
    bool m_quit          = false;

    // Selection state.
    int m_selected_map   = 0;
    int m_selected_team  = 1;
    int m_selected_class = 0;
    bool m_game_started  = false;

    // Menu data.
    std::vector<MenuItem> m_main_items;
    std::vector<MenuItem> m_pause_items;
    std::vector<MenuItem> m_map_items;
    std::vector<MenuItem> m_team_items;
    std::vector<MenuItem> m_settings_items;

    // Previous screen (for "Back" navigation).
    MenuScreen m_prev_screen = MenuScreen::MAIN_MENU;

    void build_menus();

    // Navigation helpers.
    void move_cursor(int delta);
    void select_current();

    // Render helpers.
    void render_menu_list(UIRenderer& ui, int screen_w, int screen_h,
                          const char* title,
                          const std::vector<MenuItem>& items);
    void render_settings(UIRenderer& ui, int screen_w, int screen_h);
    void render_background(UIRenderer& ui, int screen_w, int screen_h);
};

} // namespace swbf
