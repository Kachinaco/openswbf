#pragma once

#include "assets/lvl/entity_class_loader.h"
#include "assets/lvl/light_loader.h"
#include "assets/lvl/model_loader.h"
#include "assets/lvl/path_loader.h"
#include "assets/lvl/script_loader.h"
#include "assets/lvl/skeleton_loader.h"
#include "assets/lvl/sound_loader.h"
#include "assets/lvl/terrain_loader.h"
#include "assets/lvl/texture_loader.h"
#include "core/filesystem.h"
#include "core/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// LVLLoader — top-level .lvl container parser.
//
// A .lvl file is a ucfb root chunk whose payload is a flat sequence of
// child chunks.  Each child is an asset of a particular type: textures
// (tex_), models (modl), terrain (tern), entity classes (entc), etc.
//
// LVLLoader iterates those children, dispatches each to the appropriate
// sub-loader, and stores the results in typed vectors.
//
// Usage:
//   swbf::LVLLoader loader;
//   if (loader.load(file_bytes)) {
//       for (auto& tex : loader.textures()) { ... }
//   }
// ---------------------------------------------------------------------------

class LVLLoader {
public:
    LVLLoader() = default;
    ~LVLLoader() = default;

    // Parse a .lvl file from an in-memory byte buffer.
    // Returns true on success.  On failure, an error is logged and the
    // loader's internal collections are left in a partially-loaded state
    // (whatever was successfully parsed before the error is retained).
    bool load(const std::vector<uint8_t>& data);

    // Load a .lvl file from the virtual filesystem.
    // The filepath is resolved through `vfs.read_file()`.
    bool load(const std::string& filepath, VFS& vfs);

    // ---- Accessors for loaded assets ------------------------------------

    const std::vector<Texture>& textures() const { return m_textures; }
    const std::vector<Model>& models() const { return m_models; }
    const std::vector<TerrainData>& terrains() const { return m_terrains; }
    const std::vector<EntityClass>& entity_classes() const { return m_entity_classes; }
    const std::vector<Skeleton>& skeletons() const { return m_skeletons; }
    const std::vector<Light>& lights() const { return m_lights; }
    const std::vector<PathNetwork>& paths() const { return m_paths; }
    const std::vector<Sound>& sounds() const { return m_sounds; }
    const std::vector<Script>& scripts() const { return m_scripts; }
    const std::vector<LocalizationTable>& localizations() const { return m_localizations; }

    // Total number of assets loaded (across all types).
    std::size_t asset_count() const;

    // Clear all loaded data.
    void clear();

private:
    // Dispatch a single child chunk to the appropriate sub-loader.
    void dispatch_chunk(ChunkReader& chunk);

    std::vector<Texture>            m_textures;
    std::vector<Model>              m_models;
    std::vector<TerrainData>        m_terrains;
    std::vector<EntityClass>        m_entity_classes;
    std::vector<Skeleton>           m_skeletons;
    std::vector<Light>              m_lights;
    std::vector<PathNetwork>        m_paths;
    std::vector<Sound>              m_sounds;
    std::vector<Script>             m_scripts;
    std::vector<LocalizationTable>  m_localizations;
};

} // namespace swbf
