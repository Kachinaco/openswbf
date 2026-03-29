#pragma once

#include <string>
#include <unordered_map>

namespace swbf {

// Lightweight runtime configuration store.
//
// Backed by a flat string->string map.  Typed accessors (int, float, bool)
// parse on every call — this keeps the implementation trivial and is fine for
// a config system that is read infrequently (startup, level load, etc.).
class Config {
public:
    Config() = default;
    ~Config() = default;

    // Store a key/value pair, overwriting any previous value for the key.
    void set(const std::string& key, const std::string& value);

    // Retrieve a string value. Returns default_val if the key is absent.
    std::string get(const std::string& key,
                    const std::string& default_val = "") const;

    // Retrieve an integer value. Returns default_val if the key is absent or
    // cannot be parsed as an integer.
    int get_int(const std::string& key, int default_val = 0) const;

    // Retrieve a float value. Returns default_val if the key is absent or
    // cannot be parsed as a float.
    float get_float(const std::string& key, float default_val = 0.0f) const;

    // Retrieve a boolean value. Recognises "1", "true", and "yes"
    // (case-insensitive) as true; everything else (including a missing key)
    // returns default_val.
    bool get_bool(const std::string& key, bool default_val = false) const;

    // Return true if the key exists in the store.
    bool has(const std::string& key) const;

    // Remove a key. Returns true if the key was present.
    bool remove(const std::string& key);

    // Remove all entries.
    void clear();

private:
    std::unordered_map<std::string, std::string> entries_;
};

} // namespace swbf
