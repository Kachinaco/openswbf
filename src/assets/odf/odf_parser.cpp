#include "assets/odf/odf_parser.h"

#include "core/log.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <sstream>

namespace swbf {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Trim leading and trailing whitespace from a string_view, return as string.
static std::string trim(std::string_view sv) {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r')) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r')) {
        sv.remove_suffix(1);
    }
    return std::string(sv);
}

/// Strip surrounding double-quotes from a value, if present.
static std::string strip_quotes(const std::string& val) {
    if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
        return val.substr(1, val.size() - 2);
    }
    return val;
}

// ---------------------------------------------------------------------------
// ODFFile accessors
// ---------------------------------------------------------------------------

std::string ODFFile::get(const std::string& section, const std::string& key,
                         const std::string& default_val) const {
    for (const auto& sec : sections) {
        if (sec.name == section) {
            auto it = sec.properties.find(key);
            if (it != sec.properties.end()) {
                return it->second;
            }
        }
    }
    return default_val;
}

float ODFFile::get_float(const std::string& section, const std::string& key,
                         float default_val) const {
    std::string val = get(section, key);
    if (val.empty()) return default_val;

    // std::from_chars for float is not available on all C++17 implementations
    // (notably older GCC and Emscripten), so we use strtof as a portable fallback.
    char* end = nullptr;
    float result = std::strtof(val.c_str(), &end);
    if (end == val.c_str()) {
        return default_val;
    }
    return result;
}

int ODFFile::get_int(const std::string& section, const std::string& key,
                     int default_val) const {
    std::string val = get(section, key);
    if (val.empty()) return default_val;

    int result = default_val;
    auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), result);
    if (ec != std::errc{}) {
        return default_val;
    }
    return result;
}

// ---------------------------------------------------------------------------
// ODFParser
// ---------------------------------------------------------------------------

ODFFile ODFParser::parse(const std::string& text) {
    ODFFile odf;

    std::istringstream stream(text);
    std::string line;
    ODFSection* current_section = nullptr;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);

        // Skip empty lines
        if (trimmed.empty()) continue;

        // Skip comments (lines starting with //)
        if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '/') continue;

        // Section header: [SectionName]
        if (trimmed.front() == '[') {
            auto close = trimmed.find(']');
            if (close == std::string::npos) {
                LOG_WARN("ODF: malformed section header: %s", trimmed.c_str());
                continue;
            }
            std::string section_name = trimmed.substr(1, close - 1);
            odf.sections.push_back(ODFSection{section_name, {}});
            current_section = &odf.sections.back();
            continue;
        }

        // Key = Value pair
        auto eq_pos = trimmed.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = trim(std::string_view(trimmed.data(), eq_pos));
            std::string value = trim(std::string_view(trimmed.data() + eq_pos + 1,
                                                      trimmed.size() - eq_pos - 1));
            // Strip surrounding quotes from the value
            value = strip_quotes(value);

            if (!current_section) {
                // Key-value pair before any section header; create an implicit section
                LOG_WARN("ODF: key '%s' found before any section header, ignoring",
                         key.c_str());
                continue;
            }

            current_section->properties[key] = value;
            continue;
        }

        // Lines that are neither comments, sections, nor key-value pairs are
        // ignored (could be multi-line continuation or unknown syntax).
    }

    // Extract convenience fields from [GameObjectClass] section
    for (const auto& sec : odf.sections) {
        if (sec.name == "GameObjectClass") {
            auto it = sec.properties.find("ClassLabel");
            if (it != sec.properties.end()) {
                odf.class_label = it->second;
            }

            it = sec.properties.find("ClassParent");
            if (it != sec.properties.end()) {
                odf.class_parent = it->second;
            }

            it = sec.properties.find("GeometryName");
            if (it != sec.properties.end()) {
                odf.geometry_name = it->second;
            }
            break;
        }
    }

    // GeometryName may also live in [Properties] — check as fallback
    if (odf.geometry_name.empty()) {
        for (const auto& sec : odf.sections) {
            if (sec.name == "Properties") {
                auto it = sec.properties.find("GeometryName");
                if (it != sec.properties.end()) {
                    odf.geometry_name = it->second;
                    break;
                }
            }
        }
    }

    LOG_DEBUG("ODF: parsed %zu sections, ClassLabel='%s', ClassParent='%s'",
              odf.sections.size(), odf.class_label.c_str(), odf.class_parent.c_str());

    return odf;
}

ODFFile ODFParser::parse(const std::vector<uint8_t>& data) {
    std::string text(data.begin(), data.end());
    return parse(text);
}

} // namespace swbf
