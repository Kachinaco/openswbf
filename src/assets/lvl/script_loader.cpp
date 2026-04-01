#include "script_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <cstring>

namespace swbf {

// ---------------------------------------------------------------------------
// Sub-chunk FourCCs used inside scr_ and Locl chunks
// ---------------------------------------------------------------------------

namespace {

constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');
constexpr FourCC INFO = make_fourcc('I', 'N', 'F', 'O');
constexpr FourCC BODY = make_fourcc('B', 'O', 'D', 'Y');
constexpr FourCC DATA = make_fourcc('D', 'A', 'T', 'A');
constexpr FourCC SIZE = make_fourcc('S', 'I', 'Z', 'E');

// Lua bytecode signature bytes for detection.
// Lua 4.0: 0x1B 'L' 'u' 'a'
// Lua 5.0: 0x1B 'L' 'u' 'a' (same header, different version byte at offset 4)
constexpr uint8_t LUA_SIGNATURE[] = {0x1B, 'L', 'u', 'a'};
constexpr std::size_t LUA_SIG_SIZE = 4;

bool is_lua_bytecode(const uint8_t* data, std::size_t size) {
    if (size < LUA_SIG_SIZE) return false;
    return std::memcmp(data, LUA_SIGNATURE, LUA_SIG_SIZE) == 0;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Script accessors
// ---------------------------------------------------------------------------

std::string Script::source_text() const {
    if (data.empty()) return {};
    return std::string(data.begin(), data.end());
}

bool Script::is_bytecode() const {
    return is_lua_bytecode(data.data(), data.size());
}

// ---------------------------------------------------------------------------
// LocalizationTable accessor
// ---------------------------------------------------------------------------

std::string LocalizationTable::get(const std::string& key) const {
    for (const auto& entry : entries) {
        if (entry.key == key) {
            return entry.value;
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
// ScriptLoader — load_script
//
// scr_ chunk hierarchy:
//
//   scr_
//     NAME  — script name (null-terminated string)
//     INFO  — metadata: uint32 size, uint32 type_flags
//     BODY  — script data (bytecode or source text)
//
// Some .lvl files embed a scr_ container with sub-chunks, others store
// the script data directly after NAME and INFO.
// ---------------------------------------------------------------------------

Script ScriptLoader::load_script(ChunkReader& chunk) {
    Script script;

    if (chunk.id() != chunk_id::scr_) {
        LOG_WARN("ScriptLoader: expected scr_ chunk, got 0x%08X", chunk.id());
        return script;
    }

    // Try to read as chunked data first.
    // If the sub-chunks don't parse, fall back to treating the payload
    // as raw script data.
    bool has_body = false;
    uint32_t expected_size = 0;

    std::vector<ChunkReader> children;
    try {
        children = chunk.get_children();
    } catch (...) {
        // If get_children fails, the payload is likely raw script data.
        // Treat the entire payload as the script body.
        if (chunk.size() > 0) {
            script.data.assign(chunk.data(), chunk.data() + chunk.size());
            script.type = is_lua_bytecode(script.data.data(), script.data.size())
                              ? ScriptType::Bytecode
                              : ScriptType::SourceText;
        }
        return script;
    }

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == NAME) {
            script.name = child.read_string();
        }
        else if (id == INFO) {
            // Metadata.
            if (child.remaining() >= 4) {
                expected_size = child.read<uint32_t>();
            }
            // Type flags (if present): bit 0 = bytecode, bit 1 = source
            if (child.remaining() >= 4) {
                uint32_t type_flags = child.read<uint32_t>();
                if (type_flags & 0x1) {
                    script.type = ScriptType::Bytecode;
                } else {
                    script.type = ScriptType::SourceText;
                }
            }
        }
        else if (id == SIZE) {
            // Alternative to INFO for size.
            if (child.remaining() >= 4) {
                expected_size = child.read<uint32_t>();
            }
        }
        else if (id == BODY || id == DATA) {
            // Script data payload.
            if (child.size() > 0) {
                script.data.assign(child.data(), child.data() + child.size());
                has_body = true;
            }
        }
    }

    // If no BODY/DATA was found but there are children, some formats
    // store the script as a child chunk with a different FourCC.
    // Check remaining children for any that look like Lua data.
    if (!has_body && !children.empty()) {
        for (auto& child : children) {
            if (child.id() != NAME && child.id() != INFO && child.id() != SIZE) {
                // Try this chunk as the script body.
                if (child.size() > 0) {
                    script.data.assign(child.data(), child.data() + child.size());
                    has_body = true;
                    break;
                }
            }
        }
    }

    // Auto-detect script type from content if not set by INFO flags.
    if (has_body && !script.data.empty()) {
        script.type = is_lua_bytecode(script.data.data(), script.data.size())
                          ? ScriptType::Bytecode
                          : ScriptType::SourceText;
    }

    (void)expected_size; // Used for validation if needed.

    LOG_DEBUG("ScriptLoader: loaded '%s' (%s, %zu bytes)",
              script.name.c_str(),
              script.type == ScriptType::Bytecode ? "bytecode" : "source",
              script.data.size());

    return script;
}

// ---------------------------------------------------------------------------
// ScriptLoader — load_localization
//
// Locl chunk hierarchy:
//
//   Locl
//     BODY — packed string table:
//       uint32 entry_count
//       per entry:
//         null-terminated key string (ASCII)
//         null-terminated value string (may be wide-char encoded)
//
// Some formats use NAME + DATA pairs instead of a single BODY.
// ---------------------------------------------------------------------------

LocalizationTable ScriptLoader::load_localization(ChunkReader& chunk) {
    LocalizationTable table;

    if (chunk.id() != chunk_id::Locl) {
        LOG_WARN("ScriptLoader: expected Locl chunk, got 0x%08X", chunk.id());
        return table;
    }

    std::vector<ChunkReader> children = chunk.get_children();

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == NAME) {
            table.name = child.read_string();
        }
        else if (id == BODY || id == DATA) {
            // Read entry count.
            if (child.remaining() < 4) continue;

            uint32_t entry_count = child.read<uint32_t>();
            table.entries.reserve(entry_count);

            for (uint32_t i = 0; i < entry_count; ++i) {
                if (child.remaining() == 0) break;

                LocalizationEntry entry;
                try {
                    entry.key = child.read_string();
                } catch (...) {
                    break;
                }

                if (child.remaining() == 0) break;

                // Value may be wide-character encoded. Read as raw bytes
                // and interpret as a narrow string for now.
                // Wide-char support (UTF-16 to UTF-8) can be added later.
                try {
                    // Try to read as a null-terminated string.
                    // For wide-char data, this will read up to the first
                    // null byte, which may be part of a UTF-16 sequence.
                    // A proper implementation would detect the encoding.
                    entry.value = child.read_string();
                } catch (...) {
                    break;
                }

                table.entries.push_back(std::move(entry));
            }
        }
    }

    // If no BODY was found, try reading key-value pairs from direct children.
    if (table.entries.empty()) {
        std::string pending_key;
        for (auto& child : children) {
            if (child.id() == NAME && !pending_key.empty()) {
                // This NAME is a value for the previous key.
                LocalizationEntry entry;
                entry.key = std::move(pending_key);
                entry.value = child.read_string();
                table.entries.push_back(std::move(entry));
                pending_key.clear();
            } else if (child.id() == NAME && pending_key.empty() &&
                       !table.name.empty()) {
                // Second NAME onwards are key-value alternations.
                pending_key = child.read_string();
            }
        }
    }

    LOG_DEBUG("ScriptLoader: loaded localization '%s' with %zu entries",
              table.name.c_str(), table.entries.size());

    return table;
}

} // namespace swbf
