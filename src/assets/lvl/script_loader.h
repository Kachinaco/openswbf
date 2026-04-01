#pragma once

#include "core/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// Script data structures
//
// SWBF uses Lua scripts for mission logic, game modes, and UI. Scripts are
// compiled/munged into .lvl files as scr_ chunks.
//
// SWBF1 uses Lua 4.0-era bytecode; SWBF2 uses Lua 5.0.
// Scripts in munged .lvl files can contain either:
//   - Compiled Lua bytecode (pre-compiled by the munger)
//   - Raw Lua source text (in some debug/development builds)
//
// The scr_ chunk hierarchy:
//
//   scr_
//     NAME  — script name (e.g. "tat2_con", "tat2_ctf")
//     INFO  — script metadata (size, type flags)
//     BODY  — script data (bytecode or source text)
//
// The Locl chunk is used for localization string tables, which have a
// similar structure but contain key-value text pairs.
// ---------------------------------------------------------------------------

/// Script content type.
enum class ScriptType : uint8_t {
    Bytecode   = 0,   // Compiled Lua bytecode
    SourceText = 1,   // Raw Lua source code
};

/// A fully parsed script asset.
struct Script {
    std::string  name;       // Script name identifier
    ScriptType   type = ScriptType::Bytecode;

    /// Raw script data — either bytecode or source text.
    std::vector<uint8_t> data;

    /// Returns the script data interpreted as a source text string.
    /// Only meaningful when type == ScriptType::SourceText.
    std::string source_text() const;

    /// Returns true if this script contains Lua bytecode.
    bool is_bytecode() const;
};

/// A localization string entry.
struct LocalizationEntry {
    std::string key;
    std::string value;       // Localized text (may be wide-character encoded)
};

/// A parsed localization table from a Locl chunk.
struct LocalizationTable {
    std::string                    name;
    std::vector<LocalizationEntry> entries;

    /// Look up a localized string by key. Returns empty string if not found.
    std::string get(const std::string& key) const;
};

// ---------------------------------------------------------------------------
// ScriptLoader — parses scr_ and Locl UCFB chunks from munged .lvl files.
//
// Usage:
//   ScriptLoader loader;
//   Script script = loader.load_script(scr_chunk);
//   LocalizationTable table = loader.load_localization(locl_chunk);
// ---------------------------------------------------------------------------

class ScriptLoader {
public:
    ScriptLoader() = default;

    /// Parse a scr_ chunk and return the decoded script asset.
    Script load_script(ChunkReader& chunk);

    /// Parse a Locl chunk and return the decoded localization table.
    LocalizationTable load_localization(ChunkReader& chunk);
};

} // namespace swbf
