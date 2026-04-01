// swbf_api.cpp — SWBF mission script API: 27 Lua-callable functions.
//
// Each function follows the Lua C API signature:  int func(lua_State* L)
// They access game systems through the GameSystems singleton.

#include "scripting/swbf_api.h"
#include "scripting/lua_runtime.h"

#include "game/game_systems.h"
#include "game/entity_manager.h"
#include "game/spawn_system.h"
#include "game/command_post_system.h"
#include "game/conquest_mode.h"
#include "game/weapon_system.h"
#include "game/ai_system.h"
#include "audio/audio_device.h"
#include "renderer/camera.h"
#include "assets/lvl/lvl_loader.h"
#include "core/filesystem.h"
#include "core/log.h"

#include <cmath>
#include <cstring>
#include <string>

namespace {

using swbf::GameSystems;

// Helper — safely read Lua string args, returning "" if missing/nil.
static std::string get_string(lua_State* L, int idx) {
    const char* s = lua_tostring(L, idx);
    return s ? s : "";
}

static int get_int(lua_State* L, int idx) {
    return static_cast<int>(lua_tonumber(L, idx));
}

static float get_float(lua_State* L, int idx) {
    return static_cast<float>(lua_tonumber(L, idx));
}

// =========================================================================
// 1. ReadDataFile(path)
// =========================================================================
static int api_ReadDataFile(lua_State* L) {
    std::string path = get_string(L, 1);
    LOG_INFO("SWBF API: ReadDataFile(\"%s\")", path.c_str());

    auto& sys = GameSystems::instance();
    if (!sys.vfs) {
        LOG_WARN("  ReadDataFile: VFS not available");
        return 0;
    }

    swbf::LVLLoader loader;
    if (loader.load(path, *sys.vfs)) {
        LOG_INFO("  ReadDataFile: loaded %zu assets from \"%s\"",
                 loader.asset_count(), path.c_str());
    } else {
        LOG_WARN("  ReadDataFile: failed to load \"%s\"", path.c_str());
    }
    return 0;
}

// =========================================================================
// 2. ReadDataFileInGame(path)
// =========================================================================
static int api_ReadDataFileInGame(lua_State* L) {
    std::string path = get_string(L, 1);
    LOG_INFO("SWBF API: ReadDataFileInGame(\"%s\")", path.c_str());

    auto& sys = GameSystems::instance();
    if (!sys.vfs) {
        LOG_WARN("  ReadDataFileInGame: VFS not available");
        return 0;
    }

    swbf::LVLLoader loader;
    if (loader.load(path, *sys.vfs)) {
        LOG_INFO("  ReadDataFileInGame: loaded %zu assets from \"%s\"",
                 loader.asset_count(), path.c_str());
    } else {
        LOG_WARN("  ReadDataFileInGame: failed to load \"%s\"", path.c_str());
    }
    return 0;
}

// =========================================================================
// 3. SetTeamName(team, name)
// =========================================================================
static int api_SetTeamName(lua_State* L) {
    int         team = get_int(L, 1);
    std::string name = get_string(L, 2);
    LOG_INFO("SWBF API: SetTeamName(%d, \"%s\")", team, name.c_str());

    auto& sys = GameSystems::instance();
    if (sys.spawn_system) {
        sys.spawn_system->set_team_name(team, name);
    }
    return 0;
}

// =========================================================================
// 4. AddUnitClass(team, odf_name, min_count, max_count)
// =========================================================================
static int api_AddUnitClass(lua_State* L) {
    int         team      = get_int(L, 1);
    std::string odf       = get_string(L, 2);
    int         min_count = get_int(L, 3);
    int         max_count = (lua_gettop(L) >= 4) ? get_int(L, 4) : min_count;
    LOG_INFO("SWBF API: AddUnitClass(%d, \"%s\", %d, %d)",
             team, odf.c_str(), min_count, max_count);

    auto& sys = GameSystems::instance();
    if (sys.spawn_system) {
        sys.spawn_system->add_unit_class(team, odf, min_count, max_count);
    }
    return 0;
}

// =========================================================================
// 5. SetHeroClass(team, odf_name)
// =========================================================================
static int api_SetHeroClass(lua_State* L) {
    int         team = get_int(L, 1);
    std::string odf  = get_string(L, 2);
    LOG_INFO("SWBF API: SetHeroClass(%d, \"%s\")", team, odf.c_str());

    auto& sys = GameSystems::instance();
    if (sys.spawn_system) {
        sys.spawn_system->set_hero_class(team, odf);
    }
    return 0;
}

// =========================================================================
// 6. SetUnitCount(team, count)
// =========================================================================
static int api_SetUnitCount(lua_State* L) {
    int team  = get_int(L, 1);
    int count = get_int(L, 2);
    LOG_INFO("SWBF API: SetUnitCount(%d, %d)", team, count);

    auto& sys = GameSystems::instance();
    if (sys.spawn_system) {
        sys.spawn_system->set_unit_count(team, count);
    }
    return 0;
}

// =========================================================================
// 7. AddCommandPost(name, team)
// =========================================================================
static int api_AddCommandPost(lua_State* L) {
    std::string name = get_string(L, 1);
    int         team = get_int(L, 2);
    LOG_INFO("SWBF API: AddCommandPost(\"%s\", %d)", name.c_str(), team);

    auto& sys = GameSystems::instance();
    if (sys.command_post_system) {
        sys.command_post_system->add_post(name, 0.0f, 0.0f, 0.0f, team);
    }
    return 0;
}

// =========================================================================
// 8. SetCommandPostTeam(name, team)
// =========================================================================
static int api_SetCommandPostTeam(lua_State* L) {
    std::string name = get_string(L, 1);
    int         team = get_int(L, 2);
    LOG_INFO("SWBF API: SetCommandPostTeam(\"%s\", %d)", name.c_str(), team);

    auto& sys = GameSystems::instance();
    if (sys.command_post_system) {
        sys.command_post_system->set_team(name, team);
    }
    return 0;
}

// =========================================================================
// 9. SetMapNorthAngle(angle_degrees)
// =========================================================================
static int api_SetMapNorthAngle(lua_State* L) {
    float angle = get_float(L, 1);
    LOG_INFO("SWBF API: SetMapNorthAngle(%.1f)", static_cast<double>(angle));

    auto& sys = GameSystems::instance();
    sys.config.set("map_north_angle", std::to_string(angle));

    if (sys.camera) {
        float radians = angle * (3.14159265f / 180.0f);
        sys.config.set("map_north_angle_rad", std::to_string(radians));
    }
    return 0;
}

// =========================================================================
// 10. SetMemoryPoolSize(pool_name, size)
// =========================================================================
static int api_SetMemoryPoolSize(lua_State* L) {
    std::string pool_name = get_string(L, 1);
    int         size      = get_int(L, 2);
    LOG_INFO("SWBF API: SetMemoryPoolSize(\"%s\", %d)",
             pool_name.c_str(), size);

    auto& sys = GameSystems::instance();
    sys.config.set("pool_" + pool_name, std::to_string(size));
    return 0;
}

// =========================================================================
// 11. SetAIViewMultiplier(multiplier)
// =========================================================================
static int api_SetAIViewMultiplier(lua_State* L) {
    float mult = get_float(L, 1);
    LOG_INFO("SWBF API: SetAIViewMultiplier(%.2f)", static_cast<double>(mult));

    auto& sys = GameSystems::instance();
    if (sys.ai_system) {
        sys.ai_system->set_view_multiplier(mult);
    }
    return 0;
}

// =========================================================================
// 12. OpenAudioStream(stream_path)
// =========================================================================
static int api_OpenAudioStream(lua_State* L) {
    std::string path = get_string(L, 1);
    LOG_INFO("SWBF API: OpenAudioStream(\"%s\")", path.c_str());

    auto& sys = GameSystems::instance();
    sys.config.set("audio_stream", path);
    return 0;
}

// =========================================================================
// 13. SetAmbientMusic(segment_min, segment_max, ...)
// =========================================================================
static int api_SetAmbientMusic(lua_State* L) {
    int nargs = lua_gettop(L);

    std::string args_str;
    for (int i = 1; i <= nargs; ++i) {
        if (i > 1) args_str += ",";
        args_str += get_string(L, i);
    }
    LOG_INFO("SWBF API: SetAmbientMusic(%s)", args_str.c_str());

    auto& sys = GameSystems::instance();
    sys.config.set("ambient_music", args_str);
    return 0;
}

// =========================================================================
// 14. SetBleedRate(rate)
// =========================================================================
static int api_SetBleedRate(lua_State* L) {
    float rate = get_float(L, 1);
    LOG_INFO("SWBF API: SetBleedRate(%.2f)", static_cast<double>(rate));

    auto& sys = GameSystems::instance();
    if (sys.conquest_mode) {
        sys.conquest_mode->set_bleed_rate(rate);
    }
    return 0;
}

// =========================================================================
// 15. SetSoundEffect(effect_name, sound_path)
// =========================================================================
static int api_SetSoundEffect(lua_State* L) {
    std::string effect = get_string(L, 1);
    std::string path   = get_string(L, 2);
    LOG_INFO("SWBF API: SetSoundEffect(\"%s\", \"%s\")",
             effect.c_str(), path.c_str());

    auto& sys = GameSystems::instance();
    sys.config.set("sfx_" + effect, path);
    return 0;
}

// =========================================================================
// 16. SetConquestMode(enabled)
// =========================================================================
static int api_SetConquestMode(lua_State* L) {
    std::string arg = get_string(L, 1);
    bool enable = (arg == "on" || arg == "1" || arg == "true"
                   || lua_toboolean(L, 1));

    LOG_INFO("SWBF API: SetConquestMode(%s)", enable ? "on" : "off");

    auto& sys = GameSystems::instance();
    if (enable) {
        if (sys.conquest_mode) {
            sys.conquest_mode->init(200);
        }
    } else {
        if (sys.conquest_mode) {
            sys.conquest_mode->clear();
        }
    }
    return 0;
}

// =========================================================================
// 17. SetSpawnDelay(seconds)
// =========================================================================
static int api_SetSpawnDelay(lua_State* L) {
    float seconds = get_float(L, 1);
    LOG_INFO("SWBF API: SetSpawnDelay(%.1f)", static_cast<double>(seconds));

    auto& sys = GameSystems::instance();
    if (sys.spawn_system) {
        sys.spawn_system->set_spawn_delay(seconds);
    }
    return 0;
}

// =========================================================================
// 18. EnableSPHeroRules()
// =========================================================================
static int api_EnableSPHeroRules(lua_State* /*L*/) {
    LOG_INFO("SWBF API: EnableSPHeroRules()");

    auto& sys = GameSystems::instance();
    if (sys.spawn_system) {
        sys.spawn_system->set_sp_hero_rules(true);
    }
    return 0;
}

// =========================================================================
// 19. SetMaxFlyHeight(height)
// =========================================================================
static int api_SetMaxFlyHeight(lua_State* L) {
    float height = get_float(L, 1);
    LOG_INFO("SWBF API: SetMaxFlyHeight(%.1f)", static_cast<double>(height));

    auto& sys = GameSystems::instance();
    sys.config.set("max_fly_height", std::to_string(height));
    return 0;
}

// =========================================================================
// 20. SetAttackingTeam(team)
// =========================================================================
static int api_SetAttackingTeam(lua_State* L) {
    int team = get_int(L, 1);
    LOG_INFO("SWBF API: SetAttackingTeam(%d)", team);
    GameSystems::instance().config.set("attacking_team", std::to_string(team));
    return 0;
}

// =========================================================================
// 21. SetDefendingTeam(team)
// =========================================================================
static int api_SetDefendingTeam(lua_State* L) {
    int team = get_int(L, 1);
    LOG_INFO("SWBF API: SetDefendingTeam(%d)", team);
    GameSystems::instance().config.set("defending_team", std::to_string(team));
    return 0;
}

// =========================================================================
// 22. AllowAISpawn(team, enabled)
// =========================================================================
static int api_AllowAISpawn(lua_State* L) {
    int team = get_int(L, 1);
    int enabled = lua_toboolean(L, 2);
    LOG_INFO("SWBF API: AllowAISpawn(%d, %s)", team, enabled ? "true" : "false");
    std::string key = "allow_ai_spawn_" + std::to_string(team);
    GameSystems::instance().config.set(key, enabled ? "1" : "0");
    return 0;
}

// =========================================================================
// 23. SetTeamAsEnemy(team_a, team_b)
// =========================================================================
static int api_SetTeamAsEnemy(lua_State* L) {
    int a = get_int(L, 1);
    int b = get_int(L, 2);
    LOG_INFO("SWBF API: SetTeamAsEnemy(%d, %d)", a, b);
    std::string key = "enemy_" + std::to_string(a) + "_" + std::to_string(b);
    GameSystems::instance().config.set(key, "1");
    return 0;
}

// =========================================================================
// 24. SetTeamAsFriend(team_a, team_b)
// =========================================================================
static int api_SetTeamAsFriend(lua_State* L) {
    int a = get_int(L, 1);
    int b = get_int(L, 2);
    LOG_INFO("SWBF API: SetTeamAsFriend(%d, %d)", a, b);
    std::string key = "friend_" + std::to_string(a) + "_" + std::to_string(b);
    GameSystems::instance().config.set(key, "1");
    return 0;
}

// =========================================================================
// 25. SetMinFlyHeight(height)
// =========================================================================
static int api_SetMinFlyHeight(lua_State* L) {
    float height = get_float(L, 1);
    LOG_INFO("SWBF API: SetMinFlyHeight(%.1f)", static_cast<double>(height));
    GameSystems::instance().config.set("min_fly_height", std::to_string(height));
    return 0;
}

// =========================================================================
// 26. SetMaxPlayerCount(count)
// =========================================================================
static int api_SetMaxPlayerCount(lua_State* L) {
    int count = get_int(L, 1);
    LOG_INFO("SWBF API: SetMaxPlayerCount(%d)", count);
    GameSystems::instance().config.set("max_player_count", std::to_string(count));
    return 0;
}

// =========================================================================
// 27. SetTeamIcon(team, icon_texture)
// =========================================================================
static int api_SetTeamIcon(lua_State* L) {
    int         team = get_int(L, 1);
    std::string icon = get_string(L, 2);
    LOG_INFO("SWBF API: SetTeamIcon(%d, \"%s\")", team, icon.c_str());
    std::string key = "team_icon_" + std::to_string(team);
    GameSystems::instance().config.set(key, icon);
    return 0;
}

} // anonymous namespace


// =========================================================================
// Registration
// =========================================================================

namespace swbf {

void register_swbf_api(LuaRuntime& runtime) {
    LOG_INFO("Registering SWBF mission API (27 functions)...");

    #define REG(name) \
        runtime.register_function(#name, api_##name)

    // Asset loading
    REG(ReadDataFile);
    REG(ReadDataFileInGame);

    // Team / unit configuration
    REG(SetTeamName);
    REG(AddUnitClass);
    REG(SetHeroClass);
    REG(SetUnitCount);

    // Command posts
    REG(AddCommandPost);
    REG(SetCommandPostTeam);

    // Map / rendering
    REG(SetMapNorthAngle);

    // Engine configuration
    REG(SetMemoryPoolSize);
    REG(SetAIViewMultiplier);

    // Audio
    REG(OpenAudioStream);
    REG(SetAmbientMusic);
    REG(SetBleedRate);
    REG(SetSoundEffect);

    // Game mode
    REG(SetConquestMode);

    // Spawning
    REG(SetSpawnDelay);
    REG(EnableSPHeroRules);

    // Gameplay
    REG(SetMaxFlyHeight);

    // Additional helpers
    REG(SetAttackingTeam);
    REG(SetDefendingTeam);
    REG(AllowAISpawn);
    REG(SetTeamAsEnemy);
    REG(SetTeamAsFriend);
    REG(SetMinFlyHeight);
    REG(SetMaxPlayerCount);
    REG(SetTeamIcon);

    #undef REG

    LOG_INFO("SWBF mission API registered.");
}

} // namespace swbf
