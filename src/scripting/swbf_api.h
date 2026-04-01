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
///
/// Each function is a static C callback that accesses game systems through
/// the GameSystems singleton.  Call register_swbf_api() after GameSystems
/// pointers have been set.
void register_swbf_api(LuaRuntime& runtime);

} // namespace swbf
