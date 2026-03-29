#include "filesystem.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#ifdef __EMSCRIPTEN__
#  include <dirent.h>
#  include <sys/stat.h>
#else
#  include <filesystem>
#endif

namespace swbf {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Join a base path and a relative path with a path separator.
static std::string join_path(const std::string& base,
                             const std::string& relative) {
    if (base.empty()) return relative;
    if (relative.empty()) return base;

    char last = base.back();
    if (last == '/' || last == '\\')
        return base + relative;
    return base + '/' + relative;
}

// ---------------------------------------------------------------------------
// Native (std::filesystem) helpers
// ---------------------------------------------------------------------------
#ifndef __EMSCRIPTEN__

static bool dir_exists_native(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

static bool file_exists_native(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

static std::vector<std::string> list_dir_native(const std::string& dir) {
    std::vector<std::string> result;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec)) {
            result.push_back(entry.path().filename().string());
        }
    }
    return result;
}

#endif // !__EMSCRIPTEN__

// ---------------------------------------------------------------------------
// Emscripten (POSIX) helpers
// ---------------------------------------------------------------------------
#ifdef __EMSCRIPTEN__

static bool dir_exists_posix(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

static bool file_exists_posix(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static std::vector<std::string> list_dir_posix(const std::string& dir) {
    std::vector<std::string> result;
    DIR* d = opendir(dir.c_str());
    if (!d) return result;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        // Skip . and ..
        if (std::strcmp(entry->d_name, ".") == 0 ||
            std::strcmp(entry->d_name, "..") == 0)
            continue;

        // Only include regular files (stat to be sure, since d_type is
        // not reliable on every filesystem).
        std::string full = join_path(dir, entry->d_name);
        struct stat st;
        if (stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            result.emplace_back(entry->d_name);
        }
    }
    closedir(d);
    return result;
}

#endif // __EMSCRIPTEN__

// ---------------------------------------------------------------------------
// Portable wrappers that dispatch to the correct backend
// ---------------------------------------------------------------------------

static bool is_directory(const std::string& path) {
#ifdef __EMSCRIPTEN__
    return dir_exists_posix(path);
#else
    return dir_exists_native(path);
#endif
}

static bool is_file(const std::string& path) {
#ifdef __EMSCRIPTEN__
    return file_exists_posix(path);
#else
    return file_exists_native(path);
#endif
}

static std::vector<std::string> list_directory(const std::string& dir) {
#ifdef __EMSCRIPTEN__
    return list_dir_posix(dir);
#else
    return list_dir_native(dir);
#endif
}

// ---------------------------------------------------------------------------
// VFS implementation
// ---------------------------------------------------------------------------

bool VFS::mount(const std::string& path) {
    if (path.empty()) return false;
    if (!is_directory(path)) return false;

    // Avoid duplicate mounts.
    for (const auto& sp : search_paths_) {
        if (sp == path) return true;
    }

    search_paths_.push_back(path);
    return true;
}

std::string VFS::resolve(const std::string& relative_path) const {
    for (const auto& sp : search_paths_) {
        std::string full = join_path(sp, relative_path);
        if (is_file(full)) return full;
    }
    return {};
}

std::vector<uint8_t> VFS::read_file(const std::string& path) const {
    std::string resolved = resolve(path);
    if (resolved.empty()) return {};

    std::FILE* fp = std::fopen(resolved.c_str(), "rb");
    if (!fp) return {};

    // Determine file size.
    std::fseek(fp, 0, SEEK_END);
    long size = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);

    if (size <= 0) {
        std::fclose(fp);
        return {};
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    size_t read = std::fread(buffer.data(), 1, buffer.size(), fp);
    std::fclose(fp);

    buffer.resize(read);
    return buffer;
}

bool VFS::file_exists(const std::string& path) const {
    return !resolve(path).empty();
}

std::vector<std::string> VFS::list_files(const std::string& directory) const {
    std::vector<std::string> result;

    for (const auto& sp : search_paths_) {
        std::string full_dir = join_path(sp, directory);
        if (!is_directory(full_dir)) continue;

        for (auto& name : list_directory(full_dir)) {
            // De-duplicate across mount points.
            if (std::find(result.begin(), result.end(), name) == result.end()) {
                result.push_back(std::move(name));
            }
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

} // namespace swbf
