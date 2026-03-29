#pragma once

#include <cstdarg>

#if defined(__GNUC__) || defined(__clang__)
#define SWBF_PRINTF_FMT(fmt_idx, va_idx) __attribute__((format(printf, fmt_idx, va_idx)))
#else
#define SWBF_PRINTF_FMT(fmt_idx, va_idx)
#endif

namespace swbf {

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR
};

// Set the minimum log level. Messages below this level are discarded.
void log_set_level(LogLevel level);

// Get the current minimum log level.
LogLevel log_get_level();

// Core logging function. Prefer the macros below.
SWBF_PRINTF_FMT(2, 3)
void log(LogLevel level, const char* fmt, ...);

// va_list variant for forwarding from other variadic functions.
SWBF_PRINTF_FMT(2, 0)
void logv(LogLevel level, const char* fmt, std::va_list args);

} // namespace swbf

// Convenience macros — these compile away to nothing if you want, but by
// default they always emit the call and the runtime level check filters.
#define LOG_TRACE(...) ::swbf::log(::swbf::LogLevel::TRACE, __VA_ARGS__)
#define LOG_DEBUG(...) ::swbf::log(::swbf::LogLevel::DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  ::swbf::log(::swbf::LogLevel::INFO,  __VA_ARGS__)
#define LOG_WARN(...)  ::swbf::log(::swbf::LogLevel::WARN,  __VA_ARGS__)
#define LOG_ERROR(...) ::swbf::log(::swbf::LogLevel::ERROR, __VA_ARGS__)
