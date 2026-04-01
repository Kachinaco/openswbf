#pragma once

#include <cstdint>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

namespace swbf {

class InputSystem;
class UIRenderer;

// ---------------------------------------------------------------------------
// TouchInput -- virtual touch controls for mobile WebAssembly builds.
//
// Maps on-screen touch zones to InputSystem key/mouse state so the rest of
// the codebase does not need to know about touch at all.
//
// Layout (portrait / landscape):
//   Left side  -- virtual joystick  (WASD movement)
//   Right side -- look area          (mouse-look delta)
//   Bottom-right buttons: Fire, Jump, Crouch, Action
//
// On non-touch devices init() is a no-op and update()/render() return
// immediately.
// ---------------------------------------------------------------------------

class TouchInput {
public:
    TouchInput() = default;

    /// Detect touch support and register Emscripten callbacks.
    /// Pass a pointer to the InputSystem whose key/mouse state we will drive.
    void init(InputSystem* input);

    /// Call once per frame, after InputSystem::update().
    /// Translates active touches into key_down / mouse_dx/dy values.
    void update();

    /// Draw semi-transparent overlay controls.
    void render(UIRenderer& ui, int screen_w, int screen_h);

    /// True if the device has touch support.
    bool is_touch_device() const { return m_touch_available; }

private:
    // Touch-point tracking (up to 5 simultaneous fingers)
    static constexpr int MAX_TOUCHES = 5;

    struct TouchPoint {
        bool   active   = false;
        long   id       = -1;    // Emscripten touch identifier
        float  start_x  = 0.0f;
        float  start_y  = 0.0f;
        float  current_x = 0.0f;
        float  current_y = 0.0f;
    };

    TouchPoint m_touches[MAX_TOUCHES] = {};

    // Which touch is assigned to which zone
    int m_joystick_touch = -1;  // index into m_touches
    int m_look_touch     = -1;

    // Joystick output (normalized -1..+1)
    float m_joy_x = 0.0f;
    float m_joy_y = 0.0f;

    // Look delta (pixels this frame)
    float m_look_dx = 0.0f;
    float m_look_dy = 0.0f;

    // Button state
    bool m_fire_down    = false;
    bool m_jump_down    = false;
    bool m_crouch_down  = false;
    bool m_action_down  = false;

    // Crouch is a toggle -- track the last raw state to detect edges
    bool m_crouch_toggled = false;
    bool m_crouch_touch_prev = false;

    bool m_touch_available = false;

    InputSystem* m_input = nullptr;

    // Screen dimensions (cached from last render call)
    int m_screen_w = 1280;
    int m_screen_h = 720;

    // Layout constants -- computed from screen size
    float joystick_cx() const;
    float joystick_cy() const;
    float joystick_radius() const;

    // Button hit-test helpers
    struct ButtonRect {
        float x, y, w, h;
    };

    ButtonRect fire_rect() const;
    ButtonRect jump_rect() const;
    ButtonRect crouch_rect() const;
    ButtonRect action_rect() const;

    bool point_in_rect(float px, float py, const ButtonRect& r) const;
    bool point_in_circle(float px, float py, float cx, float cy, float radius) const;

    // Map current touch state to InputSystem keys/mouse
    void apply_to_input();

    // Emscripten touch callbacks (static, forward to instance)
#ifdef __EMSCRIPTEN__
    static bool on_touchstart(int type, const EmscriptenTouchEvent* event, void* user_data);
    static bool on_touchmove(int type, const EmscriptenTouchEvent* event, void* user_data);
    static bool on_touchend(int type, const EmscriptenTouchEvent* event, void* user_data);
    static bool on_touchcancel(int type, const EmscriptenTouchEvent* event, void* user_data);
#endif

    int find_touch(long id) const;
    int alloc_touch(long id);
    void release_touch(long id);
    void classify_touch(int idx);
    void handle_touch_start(long id, float x, float y);
    void handle_touch_move(long id, float x, float y);
    void handle_touch_end(long id);
};

} // namespace swbf
