#include "lvl_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <cstring>

namespace swbf {

// ---------------------------------------------------------------------------
// load (from byte buffer)
// ---------------------------------------------------------------------------

bool LVLLoader::load(const std::vector<uint8_t>& data) {
    if (data.size() < 8) {
        LOG_ERROR("LVLLoader: file too small (%zu bytes)", data.size());
        return false;
    }

    // The top-level chunk should be a 'ucfb' container.
    ChunkReader root(data.data(), data.size());

    if (root.id() != chunk_id::ucfb) {
        LOG_ERROR("LVLLoader: expected ucfb root chunk, got 0x%08X", root.id());
        return false;
    }

    LOG_INFO("LVLLoader: parsing ucfb container (%u bytes payload)", root.size());

    // Iterate all top-level children inside the ucfb container.
    while (root.has_children()) {
        try {
            ChunkReader child = root.next_child();
            dispatch_chunk(child);
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: error reading child chunk: %s", e.what());
            // Continue with the next chunk if possible; the cursor has
            // already been advanced past the broken chunk's declared size.
        }
    }

    LOG_INFO("LVLLoader: loaded %zu textures, %zu models, %zu terrains, "
             "%zu entity classes, %zu skeletons, %zu animations, %zu lights, "
             "%zu paths, %zu sounds, %zu scripts, %zu localizations, "
             "%zu worlds, %zu skies, %zu effects, %zu HUDs, %zu music, %zu fonts",
             m_textures.size(), m_models.size(), m_terrains.size(),
             m_entity_classes.size(), m_skeletons.size(), m_animations.size(),
             m_lights.size(), m_paths.size(), m_sounds.size(), m_scripts.size(),
             m_localizations.size(), m_worlds.size(), m_skies.size(),
             m_effects.size(), m_hud_layouts.size(), m_music_tracks.size(),
             m_fonts.size());

    return true;
}

// ---------------------------------------------------------------------------
// load (from VFS)
// ---------------------------------------------------------------------------

bool LVLLoader::load(const std::string& filepath, VFS& vfs) {
    std::vector<uint8_t> data = vfs.read_file(filepath);
    if (data.empty()) {
        LOG_ERROR("LVLLoader: could not read '%s' from VFS", filepath.c_str());
        return false;
    }
    return load(data);
}

// ---------------------------------------------------------------------------
// dispatch_chunk
// ---------------------------------------------------------------------------

void LVLLoader::dispatch_chunk(ChunkReader& chunk) {
    const FourCC id = chunk.id();

    if (id == chunk_id::tex_) {
        // Texture asset
        try {
            TextureLoader tex_loader;
            Texture tex = tex_loader.load(chunk);
            if (!tex.name.empty()) {
                LOG_DEBUG("LVLLoader: loaded texture '%s' (%ux%u)",
                          tex.name.c_str(), tex.width, tex.height);
                m_textures.push_back(std::move(tex));
            }
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load tex_ chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::modl) {
        // Model asset — fully parse geometry, materials, and segments.
        try {
            ModelLoader model_loader;
            Model model = model_loader.load(chunk);
            LOG_DEBUG("LVLLoader: loaded model '%s' (%zu segments, %zu materials)",
                      model.name.c_str(), model.segments.size(),
                      model.materials.size());
            m_models.push_back(std::move(model));
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load modl chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::tern) {
        // Terrain
        try {
            TerrainLoader tern_loader;
            TerrainData terrain = tern_loader.load(chunk);
            if (terrain.grid_size > 0) {
                LOG_DEBUG("LVLLoader: loaded terrain (grid %u, %.1f scale)",
                          terrain.grid_size, static_cast<double>(terrain.grid_scale));
                m_terrains.push_back(std::move(terrain));
            }
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load tern chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::entc || id == chunk_id::ordc ||
             id == chunk_id::wpnc || id == chunk_id::expc) {
        // Entity class definition (soldier, weapon, ordnance, explosion)
        try {
            EntityClassLoader ec_loader;
            EntityClass ec = ec_loader.load(chunk);
            if (!ec.name.empty() || ec.type_hash != 0) {
                LOG_DEBUG("LVLLoader: loaded entity class '%s' (%zu properties)",
                          ec.name.c_str(), ec.properties.size());
                m_entity_classes.push_back(std::move(ec));
            }
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load entity class chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::skel) {
        // Skeleton (bone hierarchy)
        try {
            SkeletonLoader skel_loader;
            Skeleton skel = skel_loader.load(chunk);
            if (!skel.bones.empty()) {
                LOG_DEBUG("LVLLoader: loaded skeleton '%s' (%zu bones)",
                          skel.name.c_str(), skel.bones.size());
                m_skeletons.push_back(std::move(skel));
            }
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load skel chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::lght) {
        // Lights
        try {
            LightLoader lght_loader;
            auto lights = lght_loader.load(chunk);
            LOG_DEBUG("LVLLoader: loaded %zu lights", lights.size());
            for (auto& light : lights) {
                m_lights.push_back(std::move(light));
            }
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load lght chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::plan || id == chunk_id::PATH) {
        // AI path / planning network
        try {
            PathLoader path_loader;
            PathNetwork network = path_loader.load(chunk);
            if (!network.nodes.empty()) {
                LOG_DEBUG("LVLLoader: loaded path network '%s' (%zu nodes)",
                          network.name.c_str(), network.nodes.size());
                m_paths.push_back(std::move(network));
            }
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load path chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::snd_) {
        // Sound / audio asset
        try {
            SoundLoader snd_loader;
            Sound snd = snd_loader.load(chunk);
            if (!snd.name.empty() || !snd.sample_data.empty()) {
                LOG_DEBUG("LVLLoader: loaded sound '%s' (%.2fs)",
                          snd.name.c_str(), static_cast<double>(snd.duration()));
                m_sounds.push_back(std::move(snd));
            }
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load snd_ chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::scr_) {
        // Lua script
        try {
            ScriptLoader scr_loader;
            Script script = scr_loader.load_script(chunk);
            if (!script.name.empty() || !script.data.empty()) {
                LOG_DEBUG("LVLLoader: loaded script '%s' (%zu bytes, %s)",
                          script.name.c_str(), script.data.size(),
                          script.is_bytecode() ? "bytecode" : "source");
                m_scripts.push_back(std::move(script));
            }
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load scr_ chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::Locl) {
        // Localization string table
        try {
            ScriptLoader scr_loader;
            LocalizationTable table = scr_loader.load_localization(chunk);
            if (!table.entries.empty()) {
                LOG_DEBUG("LVLLoader: loaded localization '%s' (%zu entries)",
                          table.name.c_str(), table.entries.size());
                m_localizations.push_back(std::move(table));
            }
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load Locl chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::zaa_ || id == chunk_id::zaf_) {
        // Animation data (animation set or animation file)
        try {
            const Skeleton* skel_ptr = nullptr;
            if (!m_skeletons.empty()) {
                skel_ptr = &m_skeletons.back();
            }

            AnimationLoader anim_loader;
            auto clips = anim_loader.load(chunk, skel_ptr);
            for (auto& clip : clips) {
                LOG_DEBUG("LVLLoader: loaded animation '%s' (%.2fs, %zu tracks)",
                          clip.name.c_str(), static_cast<double>(clip.duration),
                          clip.tracks.size());
                m_animations.push_back(std::move(clip));
            }
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load animation chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::wrld) {
        // World instances (object placements)
        try {
            WorldLoader wrld_loader;
            WorldData world = wrld_loader.load(chunk);
            if (!world.instances.empty()) {
                LOG_DEBUG("LVLLoader: loaded world '%s' (%zu instances)",
                          world.name.c_str(), world.instances.size());
                m_worlds.push_back(std::move(world));
            }
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load wrld chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::sky_) {
        // Sky dome / skybox configuration
        try {
            SkyLoader sky_loader;
            SkyData sky = sky_loader.load(chunk);
            LOG_DEBUG("LVLLoader: loaded sky '%s'", sky.name.c_str());
            m_skies.push_back(std::move(sky));
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load sky_ chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::fx__) {
        // Visual effects definition
        try {
            EffectsLoader fx_loader;
            EffectData fx = fx_loader.load(chunk);
            if (!fx.name.empty() || !fx.raw_data.empty()) {
                LOG_DEBUG("LVLLoader: loaded effect '%s'", fx.name.c_str());
                m_effects.push_back(std::move(fx));
            }
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load fx__ chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::hud_) {
        // HUD layout
        try {
            HudLoader hud_loader;
            HudLayout hud = hud_loader.load(chunk);
            LOG_DEBUG("LVLLoader: loaded HUD layout '%s'", hud.name.c_str());
            m_hud_layouts.push_back(std::move(hud));
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load hud_ chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::mus_) {
        // Music / streaming audio
        try {
            MusicLoader mus_loader;
            MusicTrack track = mus_loader.load(chunk);
            if (!track.name.empty() || !track.stream_data.empty()) {
                LOG_DEBUG("LVLLoader: loaded music track '%s'",
                          track.name.c_str());
                m_music_tracks.push_back(std::move(track));
            }
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load mus_ chunk: %s", e.what());
        }
    }
    else if (id == chunk_id::font) {
        // Font / glyph atlas
        try {
            FontLoader font_loader;
            FontData fnt = font_loader.load(chunk);
            LOG_DEBUG("LVLLoader: loaded font '%s'", fnt.name.c_str());
            m_fonts.push_back(std::move(fnt));
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load font chunk: %s", e.what());
        }
    }
    else {
        // Unhandled chunk type -- log at TRACE level for debugging.
        char tag_str[5] = {};
        std::memcpy(tag_str, &id, 4);
        LOG_TRACE("LVLLoader: skipping unhandled chunk '%s' (%u bytes)",
                  tag_str, chunk.size());
    }
}

// ---------------------------------------------------------------------------
// Accessors / utility
// ---------------------------------------------------------------------------

std::size_t LVLLoader::asset_count() const {
    return m_textures.size() + m_models.size() + m_terrains.size() +
           m_entity_classes.size() + m_skeletons.size() + m_animations.size() +
           m_lights.size() + m_paths.size() + m_sounds.size() +
           m_scripts.size() + m_localizations.size() + m_worlds.size() +
           m_skies.size() + m_effects.size() + m_hud_layouts.size() +
           m_music_tracks.size() + m_fonts.size();
}

std::vector<WorldInstance> LVLLoader::all_instances() const {
    std::vector<WorldInstance> result;
    for (const auto& world : m_worlds) {
        result.insert(result.end(), world.instances.begin(),
                      world.instances.end());
    }
    return result;
}

void LVLLoader::clear() {
    m_textures.clear();
    m_models.clear();
    m_terrains.clear();
    m_entity_classes.clear();
    m_skeletons.clear();
    m_animations.clear();
    m_lights.clear();
    m_paths.clear();
    m_sounds.clear();
    m_scripts.clear();
    m_localizations.clear();
    m_worlds.clear();
    m_skies.clear();
    m_effects.clear();
    m_hud_layouts.clear();
    m_music_tracks.clear();
    m_fonts.clear();
}

} // namespace swbf
