#pragma once

namespace swbf {

class LuaRuntime;

/// Register all 27 SWBF mission script API functions with the Lua runtime.
///
/// These are the global functions that SWBF mission .lua scripts call:
///
///   ReadDataFile, ReadDataFileInGame
///   SetTeamName, AddUnitClass, SetHeroClass, SetUnitCount
///   AddCommandPost, SetCommandPostTeam
///   SetMapNorthAngle
///   SetMemoryPoolSize, SetAIViewMultiplier
///   OpenAudioStream, SetAmbientMusic, SetBleedRate, SetSoundEffect
///   SetConquestMode
///   SetSpawnDelay
///   EnableSPHeroRules, SetMaxFlyHeight
///   SetAttackingTeam, SetDefendingTeam, AllowAISpawn
///   SetTeamAsEnemy, SetTeamAsFriend, SetMinFlyHeight
///   SetMaxPlayerCount, SetTeamIcon
///
/// Each function is a lua_CFunction callback that accesses game systems
/// through the GameSystems singleton.  Call register_swbf_api() after
/// GameSystems pointers have been set and the Lua state is initialized.
void register_swbf_api(LuaRuntime& runtime);

} // namespace swbf
