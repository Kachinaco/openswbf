#pragma once

#include <string>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

namespace swbf {

/// Lua scripting runtime for executing SWBF mission scripts.
///
/// Wraps a Lua 5.4 state with a Lua 4.0 compatibility shim so that
/// original SWBF mission scripts can execute unmodified.  On init(),
/// standard libs are opened, compat globals are registered, and the
/// SWBF mission API (27 functions) is wired up.
class LuaRuntime {
public:
    bool init();
    void shutdown();

    /// Execute a Lua code string.  Returns true on success.
    bool execute_string(const char* code);

    /// Execute a Lua script file.  Returns true on success.
    bool execute_file(const std::string& path);

    /// Register a C function callable from Lua scripts.
    void register_function(const char* name, lua_CFunction func);

    /// Access the raw Lua state (for subsystems that need it directly).
    lua_State* state() const { return m_state; }

private:
    /// Install Lua 4.0 compatibility functions and globals.
    void register_lua4_compat();

    lua_State* m_state = nullptr;
};

} // namespace swbf
