#include "log.h"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace swbf {

static LogLevel s_min_level = LogLevel::INFO;

void log_set_level(LogLevel level) {
    s_min_level = level;
}

LogLevel log_get_level() {
    return s_min_level;
}

static const char* level_tag(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "[TRACE]";
        case LogLevel::DEBUG: return "[DEBUG]";
        case LogLevel::INFO:  return "[INFO] ";
        case LogLevel::WARN:  return "[WARN] ";
        case LogLevel::ERROR: return "[ERROR]";
    }
    return "[?????]";
}

static void print_timestamp() {
    using clock = std::chrono::system_clock;
    auto now = clock::now();
    auto time_t_now = clock::to_time_t(now);

    // Milliseconds within the current second
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif

    std::fprintf(stderr, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
        static_cast<int>(ms.count()));
}

void logv(LogLevel level, const char* fmt, std::va_list args) {
    if (level < s_min_level) return;

    print_timestamp();
    std::fprintf(stderr, " %s ", level_tag(level));
    std::vfprintf(stderr, fmt, args);
    std::fputc('\n', stderr);
}

void log(LogLevel level, const char* fmt, ...) {
    if (level < s_min_level) return;

    std::va_list args;
    va_start(args, fmt);
    logv(level, fmt, args);
    va_end(args);
}

} // namespace swbf
