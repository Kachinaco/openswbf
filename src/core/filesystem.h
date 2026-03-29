#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

// Virtual filesystem abstraction.
// Maintains an ordered list of search paths (mount points). When a file is
// requested the paths are searched in mount order and the first match wins,
// mimicking an overlay / layered filesystem.
//
// On native builds this uses std::filesystem for directory iteration and
// existence checks. On Emscripten it falls back to standard POSIX I/O which
// is fully supported by Emscripten's MEMFS / NODEFS backends.
class VFS {
public:
    VFS() = default;
    ~VFS() = default;

    // Non-copyable, movable.
    VFS(const VFS&) = delete;
    VFS& operator=(const VFS&) = delete;
    VFS(VFS&&) = default;
    VFS& operator=(VFS&&) = default;

    // Add a directory to the search path.
    // Returns true if the directory exists and was successfully added.
    bool mount(const std::string& path);

    // Read an entire file into a byte vector.
    // Searches each mount point in order and returns the first match.
    // Returns an empty vector if the file is not found.
    std::vector<uint8_t> read_file(const std::string& path) const;

    // Check whether a file exists in any mounted path.
    bool file_exists(const std::string& path) const;

    // List file names (non-recursive) inside a directory, relative to the
    // matching mount point. Results are de-duplicated across mount points.
    std::vector<std::string> list_files(const std::string& directory) const;

private:
    // Resolve a relative path against mounted directories.
    // Returns the full native path of the first match, or an empty string.
    std::string resolve(const std::string& relative_path) const;

    std::vector<std::string> search_paths_;
};

} // namespace swbf
