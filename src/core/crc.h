#pragma once

#include <cstdint>
#include <string_view>

namespace swbf {

// ---------------------------------------------------------------------------
// Zero Engine FNV hash
//
// A modified FNV-1a 32-bit hash used by Pandemic's Zero Engine for string
// hashing (class names, property names, etc.). Each byte is OR'd with 0x20
// before hashing, making it case-insensitive for ASCII letters.
// ---------------------------------------------------------------------------

uint32_t ze_fnv_hash(const char* str);
uint32_t ze_fnv_hash(std::string_view str);

// ---------------------------------------------------------------------------
// Zero CRC (bone-name CRC)
//
// A different CRC used in .msh files for bone/model name lookups.
// This is a basic CRC-32 with a custom polynomial, applied per-byte in a
// case-insensitive manner (tolower before hashing).
// ---------------------------------------------------------------------------

uint32_t ze_crc(const char* str);
uint32_t ze_crc(std::string_view str);

} // namespace swbf
