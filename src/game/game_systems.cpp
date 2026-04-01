#include "game/game_systems.h"

namespace swbf {

GameSystems& GameSystems::instance() {
    static GameSystems s_instance;
    return s_instance;
}

void GameSystems::clear() {
    entity_manager      = nullptr;
    spawn_system        = nullptr;
    command_post_system = nullptr;
    conquest_mode       = nullptr;
    weapon_system       = nullptr;
    ai_system           = nullptr;
    audio_device        = nullptr;
    camera              = nullptr;
    vfs                 = nullptr;
    config.clear();
}

} // namespace swbf
