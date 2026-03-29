// lvl_dump — diagnostic tool that reads a SWBF .lvl file and prints its
//            chunk structure as an indented tree.

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/types.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace swbf;

// ---------------------------------------------------------------------------
// Counters — accumulated while walking the chunk tree
// ---------------------------------------------------------------------------

struct ChunkCounts {
    int textures = 0;
    int models   = 0;
    int terrains = 0;
    int entities = 0;
    int scripts  = 0;
    int sounds   = 0;
    int worlds   = 0;
    int skeletons = 0;
    int animations = 0;
    int effects  = 0;
    int unknown  = 0;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Convert a FourCC to a printable 4-character string.
// Non-printable bytes are replaced with '?'.
static std::string fourcc_to_string(FourCC id) {
    char buf[5] = {};
    buf[0] = static_cast<char>((id      ) & 0xFF);
    buf[1] = static_cast<char>((id >>  8) & 0xFF);
    buf[2] = static_cast<char>((id >> 16) & 0xFF);
    buf[3] = static_cast<char>((id >> 24) & 0xFF);
    for (int i = 0; i < 4; ++i) {
        if (buf[i] < 0x20 || buf[i] > 0x7E) buf[i] = '?';
    }
    return std::string(buf, 4);
}

// Print N levels of indentation (2 spaces per level).
static void print_indent(int depth) {
    for (int i = 0; i < depth; ++i) {
        std::printf("  ");
    }
}

// Safely read a null-terminated string from a chunk without throwing on
// truncated data.  Returns "" if the chunk has no data.
static std::string safe_read_string(ChunkReader& chunk) {
    if (chunk.remaining() == 0) return "";
    try {
        return chunk.read_string();
    } catch (...) {
        // Fall back: treat remaining bytes as a (possibly unterminated) string.
        std::size_t len = chunk.remaining();
        std::string s(reinterpret_cast<const char*>(chunk.data()), len);
        // Trim at first null if present.
        auto pos = s.find('\0');
        if (pos != std::string::npos) s.resize(pos);
        return s;
    }
}

// ---------------------------------------------------------------------------
// Texture format name
// ---------------------------------------------------------------------------

static const char* texture_format_name(uint32_t fmt) {
    switch (fmt) {
        case 0: return "RGBA8";
        case 1: return "DXT1";
        case 3: return "DXT3";
        case 5: return "R5G6B5";
        case 6: return "A4R4G4B4";
        case 7: return "A8R8G8B8";
        default: return "unknown";
    }
}

// ---------------------------------------------------------------------------
// Summary printers for known top-level asset chunk types
// ---------------------------------------------------------------------------

// tex_ — print texture name and format info from sub-chunks.
static void print_tex_summary(ChunkReader chunk, int depth) {
    std::string name;
    uint32_t format_id = 0xFFFF;
    uint16_t width = 0, height = 0, mip_count = 0;

    while (chunk.has_children()) {
        auto child = chunk.next_child();
        FourCC cid = child.id();

        if (cid == chunk_id::NAME) {
            name = safe_read_string(child);
            print_indent(depth + 1);
            std::printf("NAME: \"%s\"\n", name.c_str());
        } else if (cid == make_fourcc('I', 'N', 'F', 'O')) {
            // tex_ INFO: u32 format, u16 width, u16 height, u16 depth, u16 mips
            if (child.remaining() >= 10) {
                format_id = child.read<uint32_t>();
                width     = child.read<uint16_t>();
                height    = child.read<uint16_t>();
                /*depth*/   child.read<uint16_t>();
                mip_count = child.read<uint16_t>();
            }
            print_indent(depth + 1);
            std::printf("INFO: width=%u, height=%u, format=%s, mips=%u\n",
                        width, height, texture_format_name(format_id), mip_count);
        } else {
            print_indent(depth + 1);
            std::printf("%s (size: %u)\n", fourcc_to_string(cid).c_str(), child.size());
        }
    }
}

// modl — print model name from the NAME sub-chunk.
static void print_modl_summary(ChunkReader chunk, int depth) {
    std::string name;
    int segment_count = 0;
    int material_count = 0;

    while (chunk.has_children()) {
        auto child = chunk.next_child();
        FourCC cid = child.id();

        if (cid == chunk_id::NAME) {
            name = safe_read_string(child);
            print_indent(depth + 1);
            std::printf("NAME: \"%s\"\n", name.c_str());
        } else if (cid == chunk_id::MSH2) {
            // Dig into MSH2 to count materials and segments.
            while (child.has_children()) {
                auto msh_child = child.next_child();
                FourCC mid = msh_child.id();
                if (mid == chunk_id::MATL) {
                    // Count MATD children.
                    while (msh_child.has_children()) {
                        auto matd = msh_child.next_child();
                        if (matd.id() == chunk_id::MATD) material_count++;
                    }
                } else if (mid == chunk_id::MODL_sub) {
                    // Each MODL sub-chunk with GEOM/SEGM is a segment group.
                    segment_count++;
                }
                print_indent(depth + 2);
                std::printf("%s (size: %u)\n", fourcc_to_string(mid).c_str(), msh_child.size());
            }
        } else {
            print_indent(depth + 1);
            std::printf("%s (size: %u)\n", fourcc_to_string(cid).c_str(), child.size());
        }
    }
    if (material_count > 0 || segment_count > 0) {
        print_indent(depth + 1);
        std::printf("=> %d materials, %d sub-models\n", material_count, segment_count);
    }
}

// tern — print terrain grid info from the INFO sub-chunk.
static void print_tern_summary(ChunkReader chunk, int depth) {
    while (chunk.has_children()) {
        auto child = chunk.next_child();
        FourCC cid = child.id();

        if (cid == make_fourcc('I', 'N', 'F', 'O')) {
            if (child.remaining() >= 12) {
                uint32_t grid_size = child.read<uint32_t>();
                float grid_scale   = child.read<float>();
                float height_scale = child.read<float>();
                print_indent(depth + 1);
                std::printf("INFO: grid_size=%u, grid_scale=%.1f, height_scale=%.1f\n",
                            grid_size, grid_scale, height_scale);
            } else {
                print_indent(depth + 1);
                std::printf("INFO (size: %u)\n", child.size());
            }
        } else {
            print_indent(depth + 1);
            std::printf("%s (size: %u)\n", fourcc_to_string(cid).c_str(), child.size());
        }
    }
}

// entc — print entity class name.
static void print_entc_summary(ChunkReader chunk, int depth) {
    while (chunk.has_children()) {
        auto child = chunk.next_child();
        FourCC cid = child.id();

        if (cid == chunk_id::NAME || cid == make_fourcc('T', 'Y', 'P', 'E')) {
            std::string val = safe_read_string(child);
            print_indent(depth + 1);
            std::printf("%s: \"%s\"\n", fourcc_to_string(cid).c_str(), val.c_str());
        } else {
            print_indent(depth + 1);
            std::printf("%s (size: %u)\n", fourcc_to_string(cid).c_str(), child.size());
        }
    }
}

// scr_ — print script name.
static void print_scr_summary(ChunkReader chunk, int depth) {
    while (chunk.has_children()) {
        auto child = chunk.next_child();
        FourCC cid = child.id();

        if (cid == chunk_id::NAME) {
            std::string val = safe_read_string(child);
            print_indent(depth + 1);
            std::printf("NAME: \"%s\"\n", val.c_str());
        } else {
            print_indent(depth + 1);
            std::printf("%s (size: %u)\n", fourcc_to_string(cid).c_str(), child.size());
        }
    }
}

// snd_ — print sound name and basic info.
static void print_snd_summary(ChunkReader chunk, int depth) {
    while (chunk.has_children()) {
        auto child = chunk.next_child();
        FourCC cid = child.id();

        if (cid == chunk_id::NAME) {
            std::string val = safe_read_string(child);
            print_indent(depth + 1);
            std::printf("NAME: \"%s\"\n", val.c_str());
        } else if (cid == make_fourcc('I', 'N', 'F', 'O')) {
            print_indent(depth + 1);
            std::printf("INFO (size: %u)\n", child.size());
        } else {
            print_indent(depth + 1);
            std::printf("%s (size: %u)\n", fourcc_to_string(cid).c_str(), child.size());
        }
    }
}

// ---------------------------------------------------------------------------
// Generic recursive chunk printer (for chunks without specialized handlers)
// ---------------------------------------------------------------------------

static void print_chunk_tree(ChunkReader chunk, int depth, ChunkCounts& counts);

// ---------------------------------------------------------------------------
// Top-level dispatch: print a chunk and its children.
// For known types, use the specialized printer and update the counter.
// For everything else, recurse generically.
// ---------------------------------------------------------------------------

static void print_chunk_tree(ChunkReader chunk, int depth, ChunkCounts& counts) {
    FourCC id = chunk.id();

    print_indent(depth);
    std::printf("%s (size: %u)\n", fourcc_to_string(id).c_str(), chunk.size());

    // --- Known top-level asset types with summary output ---

    if (id == chunk_id::tex_) {
        counts.textures++;
        print_tex_summary(std::move(chunk), depth);
        return;
    }
    if (id == chunk_id::modl) {
        counts.models++;
        print_modl_summary(std::move(chunk), depth);
        return;
    }
    if (id == chunk_id::tern) {
        counts.terrains++;
        print_tern_summary(std::move(chunk), depth);
        return;
    }
    if (id == chunk_id::entc) {
        counts.entities++;
        print_entc_summary(std::move(chunk), depth);
        return;
    }
    if (id == chunk_id::scr_) {
        counts.scripts++;
        print_scr_summary(std::move(chunk), depth);
        return;
    }
    if (id == chunk_id::snd_) {
        counts.sounds++;
        print_snd_summary(std::move(chunk), depth);
        return;
    }

    // --- Other known types — just count them ---

    if (id == chunk_id::wrld)  counts.worlds++;
    if (id == chunk_id::skel)  counts.skeletons++;
    if (id == chunk_id::fx__)  counts.effects++;
    if (id == chunk_id::zaa_ || id == chunk_id::zaf_) counts.animations++;

    // --- Recurse into container chunks ---
    // Try to parse children.  If the chunk has a valid child header, treat
    // it as a container and recurse.  Otherwise it is a leaf (raw data).

    if (chunk.size() >= 8 && chunk.has_children()) {
        // Peek at the first 4 bytes to see if they look like a FourCC.
        // Valid FourCC bytes are printable ASCII (0x20-0x7E).
        const uint8_t* p = chunk.data();
        bool looks_like_container = true;
        for (int i = 0; i < 4; ++i) {
            if (p[i] < 0x20 || p[i] > 0x7E) {
                looks_like_container = false;
                break;
            }
        }

        if (looks_like_container) {
            try {
                while (chunk.has_children()) {
                    auto child = chunk.next_child();
                    print_chunk_tree(std::move(child), depth + 1, counts);
                }
            } catch (...) {
                // Not actually a valid container — silently ignore.
                print_indent(depth + 1);
                std::printf("(raw data, %zu bytes)\n", chunk.remaining());
            }
        }
    }
}

// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------

static bool read_file(const char* path, std::vector<uint8_t>& out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::fprintf(stderr, "Error: cannot open file '%s'\n", path);
        return false;
    }
    auto file_size = file.tellg();
    if (file_size <= 0) {
        std::fprintf(stderr, "Error: file '%s' is empty\n", path);
        return false;
    }
    out.resize(static_cast<std::size_t>(file_size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(out.data()), file_size);
    if (!file) {
        std::fprintf(stderr, "Error: failed to read %lld bytes from '%s'\n",
                     static_cast<long long>(file_size), path);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: lvl_dump <file.lvl>\n");
        std::fprintf(stderr, "Reads a SWBF .lvl file and prints its chunk structure.\n");
        return 1;
    }

    const char* path = argv[1];

    // Read entire file into memory.
    std::vector<uint8_t> data;
    if (!read_file(path, data)) {
        return 1;
    }

    std::printf("File: %s (%zu bytes)\n\n", path, data.size());

    // Parse the root UCFB chunk.
    ChunkReader root(data.data(), data.size());

    if (root.id() != chunk_id::ucfb) {
        std::fprintf(stderr, "Warning: root chunk is '%s', expected 'ucfb'\n",
                     fourcc_to_string(root.id()).c_str());
    }

    // Walk the tree.
    ChunkCounts counts;
    print_chunk_tree(std::move(root), 0, counts);

    // Print totals.
    std::printf("\n--- Summary ---\n");
    std::printf("Found:");

    bool first = true;
    auto emit = [&](int count, const char* label) {
        if (count > 0) {
            if (!first) std::printf(",");
            std::printf(" %d %s", count, label);
            first = false;
        }
    };

    emit(counts.textures,   "textures");
    emit(counts.models,     "models");
    emit(counts.terrains,   "terrains");
    emit(counts.entities,   "entities");
    emit(counts.scripts,    "scripts");
    emit(counts.sounds,     "sounds");
    emit(counts.worlds,     "worlds");
    emit(counts.skeletons,  "skeletons");
    emit(counts.animations, "animations");
    emit(counts.effects,    "effects");

    if (first) {
        std::printf(" (none)\n");
    } else {
        std::printf("\n");
    }

    return 0;
}
