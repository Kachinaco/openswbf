// lua_runtime.cpp — Real Lua 5.4 runtime with Lua 4.0 compatibility shim.
//
// SWBF1 mission scripts were written for Lua 4.0.  Key differences handled:
//   - Global functions: getn, tinsert, tremove, sort, format, strsub, strfind,
//     strlen, gsub, strrep, strlower, strupper, abs, floor, ceil, sqrt,
//     mod, max, min, random, sin, cos, tan, atan2, log, exp, deg, rad
//   - Global constants: PI
//   - Table functions available as globals instead of table.* / string.* etc.

#include "scripting/lua_runtime.h"
#include "scripting/swbf_api.h"
#include "core/log.h"

#include <cmath>
#include <cstring>

namespace swbf {

// ---------------------------------------------------------------------------
// Lua 4.0 compatibility functions
// ---------------------------------------------------------------------------

// getn(table) -> #table
static int compat_getn(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushinteger(L, static_cast<lua_Integer>(lua_rawlen(L, 1)));
    return 1;
}

// tinsert(table, [pos,] value) — like table.insert
static int compat_tinsert(lua_State* L) {
    int nargs = lua_gettop(L);
    luaL_checktype(L, 1, LUA_TTABLE);

    if (nargs == 2) {
        // tinsert(t, value) — append
        lua_Integer len = static_cast<lua_Integer>(lua_rawlen(L, 1));
        lua_rawseti(L, 1, len + 1);
    } else if (nargs >= 3) {
        // tinsert(t, pos, value) — shift elements and insert
        lua_Integer pos = luaL_checkinteger(L, 2);
        lua_Integer len = static_cast<lua_Integer>(lua_rawlen(L, 1));

        // Shift elements up
        for (lua_Integer i = len; i >= pos; --i) {
            lua_rawgeti(L, 1, i);
            lua_rawseti(L, 1, i + 1);
        }
        // Insert the value at pos
        lua_pushvalue(L, 3);
        lua_rawseti(L, 1, pos);
    }
    return 0;
}

// tremove(table [, pos]) — like table.remove
static int compat_tremove(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_Integer len = static_cast<lua_Integer>(lua_rawlen(L, 1));
    lua_Integer pos = luaL_optinteger(L, 2, len);

    // Get the element being removed (return value)
    lua_rawgeti(L, 1, pos);

    // Shift elements down
    for (lua_Integer i = pos; i < len; ++i) {
        lua_rawgeti(L, 1, i + 1);
        lua_rawseti(L, 1, i);
    }
    lua_pushnil(L);
    lua_rawseti(L, 1, len);

    return 1;
}

// sort(table [, comp]) — like table.sort
static int compat_sort(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    // Delegate to table.sort
    lua_getglobal(L, "table");
    lua_getfield(L, -1, "sort");
    lua_remove(L, -2); // remove "table"
    lua_pushvalue(L, 1);
    if (lua_gettop(L) >= 2 && !lua_isnone(L, 2)) {
        lua_pushvalue(L, 2);
        lua_call(L, 2, 0);
    } else {
        lua_call(L, 1, 0);
    }
    return 0;
}

// format(fmt, ...) -> string.format(fmt, ...)
static int compat_format(lua_State* L) {
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "format");
    lua_remove(L, -2);

    int nargs = lua_gettop(L) - 1; // minus the function we just pushed
    for (int i = 1; i <= nargs; ++i) {
        lua_pushvalue(L, i);
    }
    lua_call(L, nargs, 1);
    return 1;
}

// strsub(s, i [, j]) -> string.sub(s, i, j)
static int compat_strsub(lua_State* L) {
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "sub");
    lua_remove(L, -2);

    int nargs = lua_gettop(L) - 1;
    for (int i = 1; i <= nargs; ++i) {
        lua_pushvalue(L, i);
    }
    lua_call(L, nargs, LUA_MULTRET);
    return lua_gettop(L) - nargs;
}

// strfind(s, pattern [, init [, plain]]) -> string.find(...)
static int compat_strfind(lua_State* L) {
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "find");
    lua_remove(L, -2);

    int nargs = lua_gettop(L) - 1;
    for (int i = 1; i <= nargs; ++i) {
        lua_pushvalue(L, i);
    }
    lua_call(L, nargs, LUA_MULTRET);
    return lua_gettop(L) - nargs;
}

// strlen(s) -> #s
static int compat_strlen(lua_State* L) {
    size_t len = 0;
    luaL_checklstring(L, 1, &len);
    lua_pushinteger(L, static_cast<lua_Integer>(len));
    return 1;
}

// gsub(s, pattern, repl [, n]) -> string.gsub(...)
static int compat_gsub(lua_State* L) {
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "gsub");
    lua_remove(L, -2);

    int nargs = lua_gettop(L) - 1;
    for (int i = 1; i <= nargs; ++i) {
        lua_pushvalue(L, i);
    }
    lua_call(L, nargs, LUA_MULTRET);
    return lua_gettop(L) - nargs;
}

// strrep(s, n) -> string.rep(s, n)
static int compat_strrep(lua_State* L) {
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "rep");
    lua_remove(L, -2);

    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    lua_call(L, 2, 1);
    return 1;
}

// strlower(s) -> string.lower(s)
static int compat_strlower(lua_State* L) {
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "lower");
    lua_remove(L, -2);
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);
    return 1;
}

// strupper(s) -> string.upper(s)
static int compat_strupper(lua_State* L) {
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "upper");
    lua_remove(L, -2);
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);
    return 1;
}

// strchar(i, ...) -> string.char(i, ...)
static int compat_strchar(lua_State* L) {
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "char");
    lua_remove(L, -2);

    int nargs = lua_gettop(L) - 1;
    for (int i = 1; i <= nargs; ++i) {
        lua_pushvalue(L, i);
    }
    lua_call(L, nargs, 1);
    return 1;
}

// strbyte(s [, i [, j]]) -> string.byte(...)
static int compat_strbyte(lua_State* L) {
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "byte");
    lua_remove(L, -2);

    int nargs = lua_gettop(L) - 1;
    for (int i = 1; i <= nargs; ++i) {
        lua_pushvalue(L, i);
    }
    lua_call(L, nargs, LUA_MULTRET);
    return lua_gettop(L) - nargs;
}

// ---------------------------------------------------------------------------
// Math compat: expose math.* as globals (Lua 4.0 had them global)
// ---------------------------------------------------------------------------

static int compat_abs(lua_State* L) {
    lua_pushnumber(L, std::fabs(luaL_checknumber(L, 1)));
    return 1;
}

static int compat_floor(lua_State* L) {
    lua_pushnumber(L, std::floor(luaL_checknumber(L, 1)));
    return 1;
}

static int compat_ceil(lua_State* L) {
    lua_pushnumber(L, std::ceil(luaL_checknumber(L, 1)));
    return 1;
}

static int compat_sqrt(lua_State* L) {
    lua_pushnumber(L, std::sqrt(luaL_checknumber(L, 1)));
    return 1;
}

static int compat_mod(lua_State* L) {
    lua_pushnumber(L, std::fmod(luaL_checknumber(L, 1), luaL_checknumber(L, 2)));
    return 1;
}

static int compat_max(lua_State* L) {
    lua_getglobal(L, "math");
    lua_getfield(L, -1, "max");
    lua_remove(L, -2);
    int nargs = lua_gettop(L) - 1;
    for (int i = 1; i <= nargs; ++i) {
        lua_pushvalue(L, i);
    }
    lua_call(L, nargs, 1);
    return 1;
}

static int compat_min(lua_State* L) {
    lua_getglobal(L, "math");
    lua_getfield(L, -1, "min");
    lua_remove(L, -2);
    int nargs = lua_gettop(L) - 1;
    for (int i = 1; i <= nargs; ++i) {
        lua_pushvalue(L, i);
    }
    lua_call(L, nargs, 1);
    return 1;
}

static int compat_random(lua_State* L) {
    lua_getglobal(L, "math");
    lua_getfield(L, -1, "random");
    lua_remove(L, -2);
    int nargs = lua_gettop(L) - 1;
    for (int i = 1; i <= nargs; ++i) {
        lua_pushvalue(L, i);
    }
    lua_call(L, nargs, 1);
    return 1;
}

static int compat_sin(lua_State* L) {
    lua_pushnumber(L, std::sin(luaL_checknumber(L, 1)));
    return 1;
}

static int compat_cos(lua_State* L) {
    lua_pushnumber(L, std::cos(luaL_checknumber(L, 1)));
    return 1;
}

static int compat_tan(lua_State* L) {
    lua_pushnumber(L, std::tan(luaL_checknumber(L, 1)));
    return 1;
}

static int compat_atan2(lua_State* L) {
    lua_pushnumber(L, std::atan2(luaL_checknumber(L, 1), luaL_checknumber(L, 2)));
    return 1;
}

static int compat_log(lua_State* L) {
    lua_pushnumber(L, std::log(luaL_checknumber(L, 1)));
    return 1;
}

static int compat_exp(lua_State* L) {
    lua_pushnumber(L, std::exp(luaL_checknumber(L, 1)));
    return 1;
}

static int compat_deg(lua_State* L) {
    lua_pushnumber(L, luaL_checknumber(L, 1) * (180.0 / 3.14159265358979323846));
    return 1;
}

static int compat_rad(lua_State* L) {
    lua_pushnumber(L, luaL_checknumber(L, 1) * (3.14159265358979323846 / 180.0));
    return 1;
}

// dofile(filename) — Lua 4.0 had dofile as a global
static int compat_dofile(lua_State* L) {
    const char* filename = luaL_optstring(L, 1, nullptr);
    if (luaL_dofile(L, filename) != LUA_OK) {
        return lua_error(L);
    }
    return lua_gettop(L);
}

// ---------------------------------------------------------------------------
// LuaRuntime implementation
// ---------------------------------------------------------------------------

bool LuaRuntime::init() {
    if (m_state) {
        LOG_WARN("LuaRuntime::init — already initialized");
        return true;
    }

    m_state = luaL_newstate();
    if (!m_state) {
        LOG_ERROR("LuaRuntime::init — failed to create Lua state");
        return false;
    }

    // Open standard libraries
    luaL_openlibs(m_state);

    // Install Lua 4.0 compatibility shim
    register_lua4_compat();

    // Register SWBF mission API (27 functions)
    register_swbf_api(*this);

    LOG_INFO("LuaRuntime::init — Lua 5.4 state created with Lua 4.0 compat shim");
    return true;
}

void LuaRuntime::shutdown() {
    if (m_state) {
        lua_close(m_state);
        m_state = nullptr;
        LOG_INFO("LuaRuntime::shutdown — Lua state closed");
    }
}

bool LuaRuntime::execute_string(const char* code) {
    if (!m_state) {
        LOG_ERROR("LuaRuntime::execute_string — not initialized");
        return false;
    }
    if (!code) {
        LOG_WARN("LuaRuntime::execute_string — null code pointer");
        return false;
    }

    if (luaL_dostring(m_state, code) != LUA_OK) {
        const char* err = lua_tostring(m_state, -1);
        LOG_ERROR("LuaRuntime::execute_string — error: %s", err ? err : "(unknown)");
        lua_pop(m_state, 1);
        return false;
    }
    return true;
}

bool LuaRuntime::execute_file(const std::string& path) {
    if (!m_state) {
        LOG_ERROR("LuaRuntime::execute_file — not initialized");
        return false;
    }

    if (luaL_dofile(m_state, path.c_str()) != LUA_OK) {
        const char* err = lua_tostring(m_state, -1);
        LOG_ERROR("LuaRuntime::execute_file — error loading \"%s\": %s",
                  path.c_str(), err ? err : "(unknown)");
        lua_pop(m_state, 1);
        return false;
    }

    LOG_INFO("LuaRuntime::execute_file — executed \"%s\"", path.c_str());
    return true;
}

void LuaRuntime::register_function(const char* name, lua_CFunction func) {
    if (!m_state) {
        LOG_WARN("LuaRuntime::register_function — not initialized, "
                 "ignoring: %s", name ? name : "(null)");
        return;
    }
    if (!name || !func) {
        LOG_WARN("LuaRuntime::register_function — null name or func");
        return;
    }

    lua_register(m_state, name, func);
}

void LuaRuntime::register_lua4_compat() {
    // Table manipulation
    lua_register(m_state, "getn",     compat_getn);
    lua_register(m_state, "tinsert",  compat_tinsert);
    lua_register(m_state, "tremove",  compat_tremove);
    lua_register(m_state, "sort",     compat_sort);

    // String functions as globals
    lua_register(m_state, "format",   compat_format);
    lua_register(m_state, "strsub",   compat_strsub);
    lua_register(m_state, "strfind",  compat_strfind);
    lua_register(m_state, "strlen",   compat_strlen);
    lua_register(m_state, "gsub",     compat_gsub);
    lua_register(m_state, "strrep",   compat_strrep);
    lua_register(m_state, "strlower", compat_strlower);
    lua_register(m_state, "strupper", compat_strupper);
    lua_register(m_state, "strchar",  compat_strchar);
    lua_register(m_state, "strbyte",  compat_strbyte);

    // Math functions as globals
    lua_register(m_state, "abs",      compat_abs);
    lua_register(m_state, "floor",    compat_floor);
    lua_register(m_state, "ceil",     compat_ceil);
    lua_register(m_state, "sqrt",     compat_sqrt);
    lua_register(m_state, "mod",      compat_mod);
    lua_register(m_state, "max",      compat_max);
    lua_register(m_state, "min",      compat_min);
    lua_register(m_state, "random",   compat_random);
    lua_register(m_state, "sin",      compat_sin);
    lua_register(m_state, "cos",      compat_cos);
    lua_register(m_state, "tan",      compat_tan);
    lua_register(m_state, "atan2",    compat_atan2);
    lua_register(m_state, "log",      compat_log);
    lua_register(m_state, "exp",      compat_exp);
    lua_register(m_state, "deg",      compat_deg);
    lua_register(m_state, "rad",      compat_rad);

    // Misc globals
    lua_register(m_state, "dofile",   compat_dofile);

    // PI constant
    lua_pushnumber(m_state, 3.14159265358979323846);
    lua_setglobal(m_state, "PI");

    // Lua 4.0 used %varname for upvalues/globals — 5.4 doesn't need this.
    // Most SWBF scripts use plain global variables, which work as-is.

    LOG_INFO("LuaRuntime — Lua 4.0 compatibility shim installed");
}

} // namespace swbf
