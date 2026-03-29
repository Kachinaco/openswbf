#include "scripting/lua_runtime.h"
#include "core/log.h"

#include <cstring>

namespace swbf {

bool LuaRuntime::init() {
    LOG_WARN("LuaRuntime::init — Lua not yet integrated (stub)");
    // m_state remains nullptr until real Lua is linked.
    return true;
}

void LuaRuntime::shutdown() {
    if (m_state) {
        // When Lua is integrated: lua_close(static_cast<lua_State*>(m_state));
        m_state = nullptr;
    }
    LOG_INFO("LuaRuntime::shutdown");
}

bool LuaRuntime::execute_string(const char* code) {
    if (!m_state) {
        LOG_WARN("LuaRuntime::execute_string — Lua not yet integrated, "
                 "ignoring: %.64s%s",
                 code ? code : "(null)",
                 (code && std::strlen(code) > 64) ? "..." : "");
        return false;
    }
    return false;
}

bool LuaRuntime::execute_file(const std::string& path) {
    if (!m_state) {
        LOG_WARN("LuaRuntime::execute_file — Lua not yet integrated, "
                 "ignoring: %s", path.c_str());
        return false;
    }
    return false;
}

void LuaRuntime::register_function(const char* name, void* /*func*/) {
    if (!m_state) {
        LOG_WARN("LuaRuntime::register_function — Lua not yet integrated, "
                 "ignoring: %s", name ? name : "(null)");
        return;
    }
}

} // namespace swbf
