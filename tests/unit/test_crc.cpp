// Unit tests for core/crc.h — Zero Engine FNV hash and bone-name CRC.

#include "core/crc.h"

#include <gtest/gtest.h>

using namespace swbf;

// ---------------------------------------------------------------------------
// ze_fnv_hash — FNV-1a variant used by the Zero Engine
// ---------------------------------------------------------------------------

TEST(ZeFnvHash, EmptyString) {
    // An empty string should hash to the FNV offset basis because no bytes
    // are folded in.
    const uint32_t expected = 0x811C9DC5u; // FNV_OFFSET_BASIS
    EXPECT_EQ(ze_fnv_hash(""), expected);
    EXPECT_EQ(ze_fnv_hash(std::string_view("")), expected);
}

TEST(ZeFnvHash, NullPointer) {
    // The const-char* overload guards against nullptr and returns 0.
    EXPECT_EQ(ze_fnv_hash(static_cast<const char*>(nullptr)), 0u);
}

TEST(ZeFnvHash, KnownValueHello) {
    // Compute the expected hash manually for "hello":
    //   Each byte is OR'd with 0x20 before XOR + multiply.
    //   'h' | 0x20 = 'h'(0x68), 'e'|0x20 = 'e'(0x65), etc.
    // Because lowercase letters are unchanged by |0x20, this is equivalent to
    // standard FNV-1a on "hello" (all lowercase).
    uint32_t hash = 0x811C9DC5u;
    const char* s = "hello";
    while (*s) {
        uint8_t byte = static_cast<uint8_t>(*s) | 0x20;
        hash ^= byte;
        hash *= 0x01000193u;
        ++s;
    }
    EXPECT_EQ(ze_fnv_hash("hello"), hash);
}

TEST(ZeFnvHash, CaseInsensitive) {
    // The hash should produce the same result regardless of case.
    EXPECT_EQ(ze_fnv_hash("Hello"), ze_fnv_hash("hello"));
    EXPECT_EQ(ze_fnv_hash("HELLO"), ze_fnv_hash("hello"));
    EXPECT_EQ(ze_fnv_hash("hElLo"), ze_fnv_hash("HELLO"));
}

TEST(ZeFnvHash, CaseInsensitiveClassNames) {
    // SWBF class names like "soldier" should be case-insensitive.
    EXPECT_EQ(ze_fnv_hash("soldier"), ze_fnv_hash("Soldier"));
    EXPECT_EQ(ze_fnv_hash("soldier"), ze_fnv_hash("SOLDIER"));
}

TEST(ZeFnvHash, DifferentStringsProduceDifferentHashes) {
    EXPECT_NE(ze_fnv_hash("hello"), ze_fnv_hash("world"));
    EXPECT_NE(ze_fnv_hash("rep_inf_ep3_rifleman"), ze_fnv_hash("cis_inf_rifleman"));
}

TEST(ZeFnvHash, ConstCharAndStringViewOverloadsAgree) {
    // Both overloads should produce identical results.
    const char* test_strings[] = {"hello", "soldier", "rep_inf_ep3_rifleman", "a", ""};
    for (const char* s : test_strings) {
        EXPECT_EQ(ze_fnv_hash(s), ze_fnv_hash(std::string_view(s)))
            << "Mismatch for string: " << s;
    }
}

// ---------------------------------------------------------------------------
// ze_crc — CRC-32 variant for bone/model name lookups in .msh files
// ---------------------------------------------------------------------------

TEST(ZeCrc, EmptyString) {
    // Empty string: no bytes fed into CRC, so result should be 0.
    EXPECT_EQ(ze_crc(""), 0u);
    EXPECT_EQ(ze_crc(std::string_view("")), 0u);
}

TEST(ZeCrc, NullPointer) {
    EXPECT_EQ(ze_crc(static_cast<const char*>(nullptr)), 0u);
}

TEST(ZeCrc, KnownValueSingleChar) {
    // Compute expected CRC for "a" (0x61) using the polynomial 0x04C11DB7.
    // crc starts at 0x00000000, XOR byte<<24 into crc, shift 8 times.
    uint32_t crc = 0x00000000u;
    crc ^= static_cast<uint32_t>('a') << 24;
    for (int i = 0; i < 8; ++i) {
        if (crc & 0x80000000u)
            crc = (crc << 1) ^ 0x04C11DB7u;
        else
            crc = crc << 1;
    }
    EXPECT_EQ(ze_crc("a"), crc);
}

TEST(ZeCrc, CaseInsensitive) {
    // ze_crc lowercases each byte before hashing, so case should not matter.
    EXPECT_EQ(ze_crc("bone_root"), ze_crc("Bone_Root"));
    EXPECT_EQ(ze_crc("BONE_ROOT"), ze_crc("bone_root"));
    EXPECT_EQ(ze_crc("HP_Fire"), ze_crc("hp_fire"));
}

TEST(ZeCrc, DifferentStringsProduceDifferentHashes) {
    EXPECT_NE(ze_crc("bone_l_arm"), ze_crc("bone_r_arm"));
    EXPECT_NE(ze_crc("root"), ze_crc("spine"));
}

TEST(ZeCrc, ConstCharAndStringViewOverloadsAgree) {
    const char* test_strings[] = {"bone_root", "HP_Fire", "x", ""};
    for (const char* s : test_strings) {
        EXPECT_EQ(ze_crc(s), ze_crc(std::string_view(s)))
            << "Mismatch for string: " << s;
    }
}

TEST(ZeCrc, ConsistentResults) {
    // Calling the same string twice should return the same hash.
    uint32_t h1 = ze_crc("bone_root");
    uint32_t h2 = ze_crc("bone_root");
    EXPECT_EQ(h1, h2);
}
