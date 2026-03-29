#pragma once

#include <cstddef>
#include <cstdint>

namespace swbf {

// Unsigned integer aliases
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// Signed integer aliases
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

// Floating-point aliases
using f32 = float;
using f64 = double;

// FourCC — a 4-character code packed into a uint32_t.
// Used extensively in chunk-based file formats (.lvl, .msh, etc.)
using FourCC = u32;

// Construct a FourCC at compile time from four characters.
// Example: make_fourcc('u', 'c', 'f', 'b') for "ucfb" chunk headers.
constexpr FourCC make_fourcc(char a, char b, char c, char d) {
    return static_cast<FourCC>(
        (static_cast<u32>(static_cast<u8>(a))      ) |
        (static_cast<u32>(static_cast<u8>(b)) <<  8) |
        (static_cast<u32>(static_cast<u8>(c)) << 16) |
        (static_cast<u32>(static_cast<u8>(d)) << 24)
    );
}

} // namespace swbf
