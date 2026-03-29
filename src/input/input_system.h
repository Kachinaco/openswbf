#pragma once

#include <SDL.h>

namespace swbf {

/// Handles keyboard, mouse, and window events via SDL2.
/// Call update() once per frame before reading any state.
class InputSystem {
public:
    /// Initialize SDL event subsystem state. Call once at startup.
    void init();

    /// Poll all pending SDL events and update internal state.
    /// Must be called exactly once per frame, before any queries.
    void update();

    // -- Keyboard --------------------------------------------------------

    /// True while the key is held down.
    bool key_down(int scancode) const;

    /// True only on the frame the key was first pressed.
    bool key_pressed(int scancode) const;

    /// True only on the frame the key was released.
    bool key_released(int scancode) const;

    // -- Mouse -----------------------------------------------------------

    /// Horizontal mouse movement since the last frame (pixels).
    float mouse_dx() const;

    /// Vertical mouse movement since the last frame (pixels).
    float mouse_dy() const;

    /// True while the given mouse button is held (SDL_BUTTON_LEFT, etc.).
    bool mouse_button(int button) const;

    /// True only on the frame the button was first clicked.
    bool mouse_clicked(int button) const;

    // -- State -----------------------------------------------------------

    /// True after an SDL_QUIT event has been received.
    bool quit_requested() const;

    /// True if the pointer (mouse cursor) is currently locked / captured.
    bool pointer_locked() const;

    /// Lock the pointer to the window and enable relative mouse mode.
    /// On Emscripten this requests the Pointer Lock API; on native it
    /// uses SDL_SetRelativeMouseMode.
    void request_pointer_lock();

    /// Release the pointer lock / relative mouse mode.
    void release_pointer_lock();

    // -- Window ----------------------------------------------------------

    /// Current window width in pixels.
    int window_width() const;

    /// Current window height in pixels.
    int window_height() const;

    /// True only on the frame the window was resized.
    bool window_resized() const;

private:
    static constexpr int MAX_SCANCODES   = 512;
    static constexpr int MAX_MOUSE_BUTTONS = 8;

    bool  m_keys[MAX_SCANCODES]              = {};
    bool  m_keys_prev[MAX_SCANCODES]         = {};

    bool  m_mouse_buttons[MAX_MOUSE_BUTTONS]      = {};
    bool  m_mouse_buttons_prev[MAX_MOUSE_BUTTONS] = {};

    float m_mouse_dx = 0.0f;
    float m_mouse_dy = 0.0f;

    bool  m_quit           = false;
    bool  m_pointer_locked = false;

    int   m_window_w = 1280;
    int   m_window_h = 720;
    bool  m_resized  = false;
};

} // namespace swbf
