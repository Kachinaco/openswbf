#include "input/touch_input.h"
#include "input/input_system.h"
#include "renderer/ui_renderer.h"
#include "core/log.h"

#include <SDL.h>
#include <cmath>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace swbf {

// ---------------------------------------------------------------------------
// Layout helpers -- positions scale with screen size
// ---------------------------------------------------------------------------

float TouchInput::joystick_cx() const {
    return static_cast<float>(m_screen_w) * 0.15f;
}

float TouchInput::joystick_cy() const {
    return static_cast<float>(m_screen_h) * 0.70f;
}

float TouchInput::joystick_radius() const {
    float smaller = static_cast<float>(std::min(m_screen_w, m_screen_h));
    return smaller * 0.12f;
}

TouchInput::ButtonRect TouchInput::fire_rect() const {
    float sw = static_cast<float>(m_screen_w);
    float sh = static_cast<float>(m_screen_h);
    float size = std::min(sw, sh) * 0.12f;
    return { sw - size * 1.5f, sh - size * 1.5f, size, size };
}

TouchInput::ButtonRect TouchInput::jump_rect() const {
    float sw = static_cast<float>(m_screen_w);
    float sh = static_cast<float>(m_screen_h);
    float size = std::min(sw, sh) * 0.10f;
    ButtonRect fire = fire_rect();
    return { fire.x - size * 1.3f, fire.y - size * 0.2f, size, size };
}

TouchInput::ButtonRect TouchInput::crouch_rect() const {
    float sw = static_cast<float>(m_screen_w);
    float sh = static_cast<float>(m_screen_h);
    float size = std::min(sw, sh) * 0.09f;
    ButtonRect fire = fire_rect();
    return { fire.x, fire.y - size * 1.5f, size, size };
}

TouchInput::ButtonRect TouchInput::action_rect() const {
    float sw = static_cast<float>(m_screen_w);
    float sh = static_cast<float>(m_screen_h);
    float size = std::min(sw, sh) * 0.09f;
    ButtonRect jump = jump_rect();
    return { jump.x, jump.y - size * 1.5f, size, size };
}

bool TouchInput::point_in_rect(float px, float py, const ButtonRect& r) const {
    return px >= r.x && px <= r.x + r.w && py >= r.y && py <= r.y + r.h;
}

bool TouchInput::point_in_circle(float px, float py,
                                  float cx, float cy, float radius) const {
    float dx = px - cx;
    float dy = py - cy;
    return (dx * dx + dy * dy) <= (radius * radius);
}

// ---------------------------------------------------------------------------
// Touch tracking
// ---------------------------------------------------------------------------

int TouchInput::find_touch(long id) const {
    for (int i = 0; i < MAX_TOUCHES; ++i) {
        if (m_touches[i].active && m_touches[i].id == id) return i;
    }
    return -1;
}

int TouchInput::alloc_touch(long id) {
    for (int i = 0; i < MAX_TOUCHES; ++i) {
        if (!m_touches[i].active) {
            m_touches[i].active = true;
            m_touches[i].id = id;
            return i;
        }
    }
    return -1;
}

void TouchInput::release_touch(long id) {
    int idx = find_touch(id);
    if (idx < 0) return;

    if (m_joystick_touch == idx) m_joystick_touch = -1;
    if (m_look_touch == idx)     m_look_touch = -1;

    m_touches[idx].active = false;
    m_touches[idx].id = -1;
}

void TouchInput::classify_touch(int idx) {
    float x = m_touches[idx].start_x;
    float y = m_touches[idx].start_y;
    float half_w = static_cast<float>(m_screen_w) * 0.5f;

    // Check buttons first (they are on the right side)
    if (point_in_rect(x, y, fire_rect()))   return; // handled as button
    if (point_in_rect(x, y, jump_rect()))   return;
    if (point_in_rect(x, y, crouch_rect())) return;
    if (point_in_rect(x, y, action_rect())) return;

    // Left side of screen: joystick
    if (x < half_w && m_joystick_touch < 0) {
        m_joystick_touch = idx;
        return;
    }

    // Right side of screen: look
    if (x >= half_w && m_look_touch < 0) {
        m_look_touch = idx;
        return;
    }
}

void TouchInput::handle_touch_start(long id, float x, float y) {
    int idx = alloc_touch(id);
    if (idx < 0) return;

    m_touches[idx].start_x   = x;
    m_touches[idx].start_y   = y;
    m_touches[idx].current_x = x;
    m_touches[idx].current_y = y;

    // Check buttons
    if (point_in_rect(x, y, fire_rect()))   { m_fire_down = true;   return; }
    if (point_in_rect(x, y, jump_rect()))   { m_jump_down = true;   return; }
    if (point_in_rect(x, y, action_rect())) { m_action_down = true; return; }
    if (point_in_rect(x, y, crouch_rect())) {
        // Toggle crouch on touch-down edge
        if (!m_crouch_touch_prev) {
            m_crouch_toggled = !m_crouch_toggled;
        }
        m_crouch_touch_prev = true;
        return;
    }

    classify_touch(idx);
}

void TouchInput::handle_touch_move(long id, float x, float y) {
    int idx = find_touch(id);
    if (idx < 0) return;

    float prev_x = m_touches[idx].current_x;
    float prev_y = m_touches[idx].current_y;
    m_touches[idx].current_x = x;
    m_touches[idx].current_y = y;

    if (idx == m_joystick_touch) {
        float dx = x - m_touches[idx].start_x;
        float dy = y - m_touches[idx].start_y;
        float radius = joystick_radius();
        float len = std::sqrt(dx * dx + dy * dy);
        if (len > radius) {
            dx = dx / len * radius;
            dy = dy / len * radius;
        }
        m_joy_x = dx / radius;
        m_joy_y = dy / radius;
    }

    if (idx == m_look_touch) {
        m_look_dx += (x - prev_x);
        m_look_dy += (y - prev_y);
    }
}

void TouchInput::handle_touch_end(long id) {
    int idx = find_touch(id);
    if (idx < 0) return;

    // Release buttons
    if (point_in_rect(m_touches[idx].start_x, m_touches[idx].start_y, fire_rect())) {
        m_fire_down = false;
    }
    if (point_in_rect(m_touches[idx].start_x, m_touches[idx].start_y, jump_rect())) {
        m_jump_down = false;
    }
    if (point_in_rect(m_touches[idx].start_x, m_touches[idx].start_y, action_rect())) {
        m_action_down = false;
    }
    if (point_in_rect(m_touches[idx].start_x, m_touches[idx].start_y, crouch_rect())) {
        m_crouch_touch_prev = false;
    }

    if (idx == m_joystick_touch) {
        m_joy_x = 0.0f;
        m_joy_y = 0.0f;
    }

    release_touch(id);
}

// ---------------------------------------------------------------------------
// Emscripten callbacks
// ---------------------------------------------------------------------------

#ifdef __EMSCRIPTEN__

static float em_touch_x(const EmscriptenTouchPoint& tp) {
    return static_cast<float>(tp.targetX);
}

static float em_touch_y(const EmscriptenTouchPoint& tp) {
    return static_cast<float>(tp.targetY);
}

bool TouchInput::on_touchstart(int /*type*/, const EmscriptenTouchEvent* te, void* user_data) {
    auto* self = static_cast<TouchInput*>(user_data);
    for (int i = 0; i < te->numTouches; ++i) {
        if (te->touches[i].isChanged) {
            self->handle_touch_start(te->touches[i].identifier,
                                     em_touch_x(te->touches[i]),
                                     em_touch_y(te->touches[i]));
        }
    }
    return true; // prevent default
}

bool TouchInput::on_touchmove(int /*type*/, const EmscriptenTouchEvent* te, void* user_data) {
    auto* self = static_cast<TouchInput*>(user_data);
    for (int i = 0; i < te->numTouches; ++i) {
        if (te->touches[i].isChanged) {
            self->handle_touch_move(te->touches[i].identifier,
                                    em_touch_x(te->touches[i]),
                                    em_touch_y(te->touches[i]));
        }
    }
    return true;
}

bool TouchInput::on_touchend(int /*type*/, const EmscriptenTouchEvent* te, void* user_data) {
    auto* self = static_cast<TouchInput*>(user_data);
    for (int i = 0; i < te->numTouches; ++i) {
        if (te->touches[i].isChanged) {
            self->handle_touch_end(te->touches[i].identifier);
        }
    }
    return true;
}

bool TouchInput::on_touchcancel(int type, const EmscriptenTouchEvent* te, void* user_data) {
    return on_touchend(type, te, user_data);
}

#endif // __EMSCRIPTEN__

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------

void TouchInput::init(InputSystem* input) {
    m_input = input;
    m_touch_available = false;

#ifdef __EMSCRIPTEN__
    // Detect touch support via JavaScript
    m_touch_available = EM_ASM_INT({
        return (('ontouchstart' in window) ||
                (navigator.maxTouchPoints > 0)) ? 1 : 0;
    }) != 0;

    if (m_touch_available) {
        emscripten_set_touchstart_callback("#canvas", this, true, on_touchstart);
        emscripten_set_touchmove_callback("#canvas", this, true, on_touchmove);
        emscripten_set_touchend_callback("#canvas", this, true, on_touchend);
        emscripten_set_touchcancel_callback("#canvas", this, true, on_touchcancel);

        LOG_INFO("TouchInput: touch controls enabled (mobile device detected)");
    } else {
        LOG_INFO("TouchInput: no touch support detected, controls disabled");
    }
#else
    LOG_DEBUG("TouchInput: native build, touch controls disabled");
#endif
}

// ---------------------------------------------------------------------------
// apply_to_input -- drive InputSystem key/mouse state from touch
// ---------------------------------------------------------------------------

void TouchInput::apply_to_input() {
    if (!m_input) return;

    // The InputSystem exposes m_keys and m_mouse_dx/dy as private members.
    // We inject synthetic key states via SDL_PushEvent so they flow through
    // the normal InputSystem::update() pipeline next frame. However, for
    // immediate responsiveness, we push SDL events that will be picked up
    // on the *next* frame's SDL_PollEvent loop.

    // Movement: joystick -> WASD scancodes
    // We send key-down events when the axis exceeds a deadzone and key-up
    // when it returns. This is done by injecting synthetic SDL keyboard
    // events into the event queue.

    static constexpr float DEADZONE = 0.2f;

    auto push_key = [](SDL_Scancode sc, bool down) {
        SDL_Event ev;
        ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
        ev.key.keysym.scancode = sc;
        ev.key.keysym.sym = SDL_GetKeyFromScancode(sc);
        ev.key.keysym.mod = 0;
        ev.key.repeat = 0;
        ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
        ev.key.timestamp = SDL_GetTicks();
        ev.key.windowID = 0;
        SDL_PushEvent(&ev);
    };

    // Forward/Back
    push_key(SDL_SCANCODE_W, m_joy_y < -DEADZONE);
    push_key(SDL_SCANCODE_S, m_joy_y >  DEADZONE);

    // Strafe Left/Right
    push_key(SDL_SCANCODE_A, m_joy_x < -DEADZONE);
    push_key(SDL_SCANCODE_D, m_joy_x >  DEADZONE);

    // Fire -> left mouse button
    {
        SDL_Event ev;
        ev.type = m_fire_down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        ev.button.button = SDL_BUTTON_LEFT;
        ev.button.state = m_fire_down ? SDL_PRESSED : SDL_RELEASED;
        ev.button.clicks = 1;
        ev.button.x = 0;
        ev.button.y = 0;
        ev.button.timestamp = SDL_GetTicks();
        ev.button.windowID = 0;
        ev.button.which = 0;
        SDL_PushEvent(&ev);
    }

    // Jump -> Space
    push_key(SDL_SCANCODE_SPACE, m_jump_down);

    // Crouch -> C (toggle)
    push_key(SDL_SCANCODE_C, m_crouch_toggled);

    // Action -> E
    push_key(SDL_SCANCODE_E, m_action_down);

    // Look -> inject mouse motion
    if (std::fabs(m_look_dx) > 0.5f || std::fabs(m_look_dy) > 0.5f) {
        SDL_Event ev;
        ev.type = SDL_MOUSEMOTION;
        ev.motion.xrel = static_cast<int>(m_look_dx);
        ev.motion.yrel = static_cast<int>(m_look_dy);
        ev.motion.x = 0;
        ev.motion.y = 0;
        ev.motion.state = 0;
        ev.motion.timestamp = SDL_GetTicks();
        ev.motion.windowID = 0;
        ev.motion.which = 0;
        SDL_PushEvent(&ev);
    }

    // Reset per-frame look delta
    m_look_dx = 0.0f;
    m_look_dy = 0.0f;
}

// ---------------------------------------------------------------------------
// update
// ---------------------------------------------------------------------------

void TouchInput::update() {
    if (!m_touch_available) return;
    apply_to_input();
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------

void TouchInput::render(UIRenderer& ui, int screen_w, int screen_h) {
    if (!m_touch_available) return;

    m_screen_w = screen_w;
    m_screen_h = screen_h;

    constexpr float BG_A = 0.18f;  // background alpha
    constexpr float FG_A = 0.35f;  // foreground / active alpha

    // -- Virtual joystick (left side) --
    float jcx = joystick_cx();
    float jcy = joystick_cy();
    float jr  = joystick_radius();

    // Outer ring
    ui.draw_circle(jcx, jcy, jr, 1.0f, 1.0f, 1.0f, BG_A, 32);

    // Inner nub showing current stick position
    float nub_x = jcx + m_joy_x * jr * 0.7f;
    float nub_y = jcy + m_joy_y * jr * 0.7f;
    float nub_r = jr * 0.35f;
    ui.draw_circle(nub_x, nub_y, nub_r, 1.0f, 1.0f, 1.0f, FG_A, 16);

    // -- Buttons (right side) --
    auto draw_button = [&](const ButtonRect& r, const char* label,
                           bool pressed) {
        float alpha = pressed ? FG_A : BG_A;
        ui.draw_rect(r.x, r.y, r.w, r.h, 1.0f, 1.0f, 1.0f, alpha);
        // Center the label
        float tw = ui.text_width(label, 1.5f);
        float th = ui.text_height(1.5f);
        ui.draw_text_shadow(label,
                            r.x + (r.w - tw) * 0.5f,
                            r.y + (r.h - th) * 0.5f,
                            1.0f, 1.0f, 1.0f, 0.8f, 1.5f);
    };

    draw_button(fire_rect(),   "FIRE",   m_fire_down);
    draw_button(jump_rect(),   "JUMP",   m_jump_down);
    draw_button(crouch_rect(), "DUCK",   m_crouch_toggled);
    draw_button(action_rect(), "USE",    m_action_down);
}

} // namespace swbf
