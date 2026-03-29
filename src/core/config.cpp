#include "config.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace swbf {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

// ---------------------------------------------------------------------------
// Config implementation
// ---------------------------------------------------------------------------

void Config::set(const std::string& key, const std::string& value) {
    entries_[key] = value;
}

std::string Config::get(const std::string& key,
                        const std::string& default_val) const {
    auto it = entries_.find(key);
    if (it == entries_.end()) return default_val;
    return it->second;
}

int Config::get_int(const std::string& key, int default_val) const {
    auto it = entries_.find(key);
    if (it == entries_.end()) return default_val;

    try {
        size_t pos = 0;
        int val = std::stoi(it->second, &pos);
        // Only accept if the entire string was consumed (no trailing junk).
        if (pos == it->second.size()) return val;
    } catch (const std::invalid_argument&) {
    } catch (const std::out_of_range&) {
    }

    return default_val;
}

float Config::get_float(const std::string& key, float default_val) const {
    auto it = entries_.find(key);
    if (it == entries_.end()) return default_val;

    try {
        size_t pos = 0;
        float val = std::stof(it->second, &pos);
        if (pos == it->second.size()) return val;
    } catch (const std::invalid_argument&) {
    } catch (const std::out_of_range&) {
    }

    return default_val;
}

bool Config::get_bool(const std::string& key, bool default_val) const {
    auto it = entries_.find(key);
    if (it == entries_.end()) return default_val;

    std::string lower = to_lower(it->second);
    if (lower == "1" || lower == "true" || lower == "yes") return true;
    if (lower == "0" || lower == "false" || lower == "no") return false;

    return default_val;
}

bool Config::has(const std::string& key) const {
    return entries_.find(key) != entries_.end();
}

bool Config::remove(const std::string& key) {
    return entries_.erase(key) > 0;
}

void Config::clear() {
    entries_.clear();
}

} // namespace swbf
