#include "crc.h"

#include <cctype>

namespace swbf {

// ---------------------------------------------------------------------------
// Zero Engine FNV hash — FNV-1a with case-insensitive twist
// ---------------------------------------------------------------------------

static constexpr uint32_t FNV_OFFSET_BASIS = 0x811C9DC5u;
static constexpr uint32_t FNV_PRIME        = 0x01000193u;

uint32_t ze_fnv_hash(const char* str) {
    if (!str) return 0;

    uint32_t hash = FNV_OFFSET_BASIS;
    while (*str) {
        // OR with 0x20 folds uppercase ASCII to lowercase (and leaves
        // lowercase / digits / most punctuation unchanged).
        uint8_t byte = static_cast<uint8_t>(*str) | 0x20;
        hash ^= byte;
        hash *= FNV_PRIME;
        ++str;
    }
    return hash;
}

uint32_t ze_fnv_hash(std::string_view str) {
    uint32_t hash = FNV_OFFSET_BASIS;
    for (char c : str) {
        uint8_t byte = static_cast<uint8_t>(c) | 0x20;
        hash ^= byte;
        hash *= FNV_PRIME;
    }
    return hash;
}

// ---------------------------------------------------------------------------
// Zero CRC — CRC-32 variant used for bone/model names in .msh files
//
// Uses polynomial 0x04C11DB7 in a bit-by-bit (no lookup table)
// implementation. Each character is lowercased before being fed in.
// ---------------------------------------------------------------------------

static constexpr uint32_t ZE_CRC_POLY = 0x04C11DB7u;

static uint32_t ze_crc_update(uint32_t crc, uint8_t byte) {
    crc ^= static_cast<uint32_t>(byte) << 24;
    for (int i = 0; i < 8; ++i) {
        if (crc & 0x80000000u) {
            crc = (crc << 1) ^ ZE_CRC_POLY;
        } else {
            crc = crc << 1;
        }
    }
    return crc;
}

uint32_t ze_crc(const char* str) {
    if (!str) return 0;

    uint32_t crc = 0x00000000u;
    while (*str) {
        uint8_t byte = static_cast<uint8_t>(std::tolower(static_cast<unsigned char>(*str)));
        crc = ze_crc_update(crc, byte);
        ++str;
    }
    return crc;
}

uint32_t ze_crc(std::string_view str) {
    uint32_t crc = 0x00000000u;
    for (char c : str) {
        uint8_t byte = static_cast<uint8_t>(std::tolower(static_cast<unsigned char>(c)));
        crc = ze_crc_update(crc, byte);
    }
    return crc;
}

} // namespace swbf
