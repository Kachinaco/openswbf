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

    LOG_INFO("LVLLoader: loaded %zu textures, %zu models, %zu terrains",
             m_textures.size(), m_models.size(), m_terrains.size());

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
                          terrain.grid_size, terrain.grid_scale);
                m_terrains.push_back(std::move(terrain));
            }
        } catch (const std::exception& e) {
            LOG_WARN("LVLLoader: failed to load tern chunk: %s", e.what());
        }
    }
    else {
        // Unhandled chunk type — log at TRACE level for debugging.
        // Build a readable 4-char tag string.
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
    return m_textures.size() + m_models.size() + m_terrains.size();
}

void LVLLoader::clear() {
    m_textures.clear();
    m_models.clear();
    m_terrains.clear();
}

} // namespace swbf
