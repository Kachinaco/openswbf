#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace swbf {

// ---------------------------------------------------------------------------
// ODF (Object Definition File) parser
//
// ODFs are text-based INI-like files that define every game object in
// Star Wars: Battlefront. Format example:
//
//   [GameObjectClass]
//   ClassLabel = "soldier"
//   ClassParent = "rep_inf_default"
//
//   [Properties]
//   GeometryName = "rep_inf_ep3_rifleman"
//   MaxHealth = 300.0
//
// Sections are delimited by [SectionName] headers. Key-value pairs use '='
// as the separator. Values may be quoted; quotes are stripped on parse.
// Lines starting with '//' are comments.
// ---------------------------------------------------------------------------

/// A single [Section] from an ODF file, containing its key-value pairs.
struct ODFSection {
    std::string name;  // e.g. "GameObjectClass", "Properties"
    std::unordered_map<std::string, std::string> properties;
};

/// Parsed representation of an entire ODF file.
struct ODFFile {
    // Quick-access fields extracted from the [GameObjectClass] section.
    std::string class_label;
    std::string class_parent;
    std::string geometry_name;

    // All sections in order of appearance.
    std::vector<ODFSection> sections;

    /// Look up a value by section name and key. Returns default_val if not found.
    std::string get(const std::string& section, const std::string& key,
                    const std::string& default_val = "") const;

    /// Look up a float value. Returns default_val on missing key or parse failure.
    float get_float(const std::string& section, const std::string& key,
                    float default_val = 0.0f) const;

    /// Look up an int value. Returns default_val on missing key or parse failure.
    int get_int(const std::string& section, const std::string& key,
                int default_val = 0) const;
};

/// Parser for ODF text content.
class ODFParser {
public:
    /// Parse ODF from a text string.
    ODFFile parse(const std::string& text);

    /// Parse ODF from raw byte data (interpreted as text).
    ODFFile parse(const std::vector<uint8_t>& data);
};

} // namespace swbf
