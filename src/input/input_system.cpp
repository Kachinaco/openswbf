#include "input/input_system.h"
#include "core/log.h"

#include <SDL.h>
#include <algorithm>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace swbf {

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void InputSystem::init() {
    std::memset(m_keys,               0, sizeof(m_keys));
    std::memset(m_keys_prev,          0, sizeof(m_keys_prev));
    std::memset(m_mouse_buttons,      0, sizeof(m_mouse_buttons));
    std::memset(m_mouse_buttons_prev, 0, sizeof(m_mouse_buttons_prev));

    m_mouse_dx       = 0.0f;
    m_mouse_dy       = 0.0f;
    m_quit           = false;
    m_pointer_locked = false;
    m_window_w       = 1280;
    m_window_h       = 720;
    m_resized        = false;

    LOG_INFO("InputSystem initialised");
}

void InputSystem::update() {
    // Snapshot current state into "previous" arrays so that pressed/released
    // detection works correctly for this frame.
    std::memcpy(m_keys_prev,          m_keys,          sizeof(m_keys));
    std::memcpy(m_mouse_buttons_prev, m_mouse_buttons, sizeof(m_mouse_buttons));

    // Reset per-frame accumulators.
    m_mouse_dx = 0.0f;
    m_mouse_dy = 0.0f;
    m_resized  = false;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {

        // -- Keyboard -----------------------------------------------------
        case SDL_KEYDOWN:
            if (!ev.key.repeat) {
                int sc = static_cast<int>(ev.key.keysym.scancode);
                if (sc >= 0 && sc < MAX_SCANCODES) {
                    m_keys[sc] = true;
                }
            }
            break;

        case SDL_KEYUP: {
            int sc = static_cast<int>(ev.key.keysym.scancode);
            if (sc >= 0 && sc < MAX_SCANCODES) {
                m_keys[sc] = false;
            }
            break;
        }

        // -- Mouse motion -------------------------------------------------
        case SDL_MOUSEMOTION:
            m_mouse_dx += static_cast<float>(ev.motion.xrel);
            m_mouse_dy += static_cast<float>(ev.motion.yrel);
            break;

        // -- Mouse buttons ------------------------------------------------
        case SDL_MOUSEBUTTONDOWN: {
            int btn = static_cast<int>(ev.button.button);
            if (btn >= 0 && btn < MAX_MOUSE_BUTTONS) {
                m_mouse_buttons[btn] = true;
            }
            break;
        }

        case SDL_MOUSEBUTTONUP: {
            int btn = static_cast<int>(ev.button.button);
            if (btn >= 0 && btn < MAX_MOUSE_BUTTONS) {
                m_mouse_buttons[btn] = false;
            }
            break;
        }

        // -- Window -------------------------------------------------------
        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                m_window_w = ev.window.data1;
                m_window_h = ev.window.data2;
                m_resized  = true;
                LOG_DEBUG("Window resized to %d x %d", m_window_w, m_window_h);
            }
            break;

        // -- Quit ---------------------------------------------------------
        case SDL_QUIT:
            m_quit = true;
            break;

        default:
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Keyboard queries
// ---------------------------------------------------------------------------

bool InputSystem::key_down(int scancode) const {
    if (scancode < 0 || scancode >= MAX_SCANCODES) return false;
    return m_keys[scancode];
}

bool InputSystem::key_pressed(int scancode) const {
    if (scancode < 0 || scancode >= MAX_SCANCODES) return false;
    return m_keys[scancode] && !m_keys_prev[scancode];
}

bool InputSystem::key_released(int scancode) const {
    if (scancode < 0 || scancode >= MAX_SCANCODES) return false;
    return !m_keys[scancode] && m_keys_prev[scancode];
}

// ---------------------------------------------------------------------------
// Mouse queries
// ---------------------------------------------------------------------------

float InputSystem::mouse_dx() const { return m_mouse_dx; }
float InputSystem::mouse_dy() const { return m_mouse_dy; }

bool InputSystem::mouse_button(int button) const {
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return false;
    return m_mouse_buttons[button];
}

bool InputSystem::mouse_clicked(int button) const {
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return false;
    return m_mouse_buttons[button] && !m_mouse_buttons_prev[button];
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

bool InputSystem::quit_requested() const { return m_quit; }

bool InputSystem::pointer_locked() const { return m_pointer_locked; }

void InputSystem::request_pointer_lock() {
    if (m_pointer_locked) return;

#ifdef __EMSCRIPTEN__
    // The Emscripten Pointer Lock API requires a target element.
    // "#canvas" is the conventional id for the main WebGL canvas.
    EmscriptenPointerlockChangeEvent state;
    if (emscripten_get_pointerlock_status(&state) == EMSCRIPTEN_RESULT_SUCCESS
        && state.isActive) {
        m_pointer_locked = true;
        return;
    }
    EMSCRIPTEN_RESULT res = emscripten_request_pointerlock("#canvas", /*deferUntilInEventHandler=*/EM_TRUE);
    if (res == EMSCRIPTEN_RESULT_SUCCESS || res == EMSCRIPTEN_RESULT_DEFERRED) {
        m_pointer_locked = true;
        LOG_DEBUG("Emscripten pointer lock requested");
    } else {
        LOG_WARN("Emscripten pointer lock request failed (%d)", static_cast<int>(res));
    }
#else
    if (SDL_SetRelativeMouseMode(SDL_TRUE) == 0) {
        m_pointer_locked = true;
        LOG_DEBUG("Pointer lock enabled (SDL relative mouse mode)");
    } else {
        LOG_WARN("Failed to enable relative mouse mode: %s", SDL_GetError());
    }
#endif
}

void InputSystem::release_pointer_lock() {
    if (!m_pointer_locked) return;

#ifdef __EMSCRIPTEN__
    emscripten_exit_pointerlock();
    m_pointer_locked = false;
    LOG_DEBUG("Emscripten pointer lock released");
#else
    SDL_SetRelativeMouseMode(SDL_FALSE);
    m_pointer_locked = false;
    LOG_DEBUG("Pointer lock released");
#endif
}

// ---------------------------------------------------------------------------
// Window queries
// ---------------------------------------------------------------------------

int  InputSystem::window_width()   const { return m_window_w; }
int  InputSystem::window_height()  const { return m_window_h; }
bool InputSystem::window_resized() const { return m_resized; }

} // namespace swbf
