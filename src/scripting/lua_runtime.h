#pragma once

#include <string>

namespace swbf {

/// Lua scripting runtime for executing SWBF mission scripts.
///
/// This is a stub implementation.  Real Lua integration (Lua 4.0 compatibility
/// shim over Lua 5.4) will be added in a later milestone.  All execution
/// methods currently log a warning and return false.
class LuaRuntime {
public:
    bool init();
    void shutdown();

    /// Execute a Lua code string.  Returns true on success.
    bool execute_string(const char* code);

    /// Execute a Lua script file.  Returns true on success.
    bool execute_file(const std::string& path);

    /// Register a C function callable from Lua scripts.
    /// @p func  The function pointer (cast to void* for the stub interface).
    void register_function(const char* name, void* func);

private:
    void* m_state = nullptr; // lua_State*, null until Lua is integrated
};

} // namespace swbf
