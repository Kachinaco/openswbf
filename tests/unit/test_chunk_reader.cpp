// Unit tests for assets/ucfb/chunk_reader.h — UCFB binary container reader.

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/types.h"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

using namespace swbf;

// ---------------------------------------------------------------------------
// Helpers: hand-craft synthetic UCFB chunk data in memory
// ---------------------------------------------------------------------------

// Write a little-endian uint32 to a byte vector.
static void push_u32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>((val >>  0) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >>  8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

// Write a float as 4 little-endian bytes.
static void push_float(std::vector<uint8_t>& buf, float val) {
    uint32_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    push_u32(buf, bits);
}

// Write a null-terminated string.
static void push_string(std::vector<uint8_t>& buf, const char* str) {
    while (*str) {
        buf.push_back(static_cast<uint8_t>(*str));
        ++str;
    }
    buf.push_back(0); // null terminator
}

// Build a complete chunk: [FourCC (4 bytes)][payload_size (4 bytes LE)][payload]
// Returns the bytes for the entire chunk.
static std::vector<uint8_t> make_chunk(FourCC id, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> chunk;
    push_u32(chunk, id);
    push_u32(chunk, static_cast<uint32_t>(payload.size()));
    chunk.insert(chunk.end(), payload.begin(), payload.end());
    return chunk;
}

// ---------------------------------------------------------------------------
// Tests: construction and identity
// ---------------------------------------------------------------------------

TEST(ChunkReader, ReadsIdAndSize) {
    // Create a simple chunk with FourCC "ucfb" and a 4-byte payload.
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
    auto chunk_data = make_chunk(chunk_id::ucfb, payload);

    ChunkReader reader(chunk_data.data(), chunk_data.size());
    EXPECT_EQ(reader.id(), chunk_id::ucfb);
    EXPECT_EQ(reader.size(), 4u);
    EXPECT_EQ(reader.remaining(), 4u);
}

TEST(ChunkReader, CustomFourCC) {
    FourCC test_id = make_fourcc('T', 'E', 'S', 'T');
    std::vector<uint8_t> payload = {0xAA, 0xBB};
    auto chunk_data = make_chunk(test_id, payload);

    ChunkReader reader(chunk_data.data(), chunk_data.size());
    EXPECT_EQ(reader.id(), test_id);
    EXPECT_EQ(reader.size(), 2u);
}

TEST(ChunkReader, ThrowsOnNullData) {
    EXPECT_THROW(ChunkReader(nullptr, 100), std::runtime_error);
}

TEST(ChunkReader, ThrowsOnTooSmallBuffer) {
    uint8_t tiny[4] = {0};
    EXPECT_THROW(ChunkReader(tiny, 4), std::runtime_error);
}

TEST(ChunkReader, ThrowsWhenPayloadExceedsBuffer) {
    // Header claims 100 bytes of payload, but buffer only has 12 total.
    std::vector<uint8_t> bad;
    push_u32(bad, chunk_id::ucfb);
    push_u32(bad, 100); // claimed payload size
    bad.resize(12, 0);  // only 4 bytes of payload available
    EXPECT_THROW(ChunkReader(bad.data(), bad.size()), std::runtime_error);
}

// ---------------------------------------------------------------------------
// Tests: reading typed values
// ---------------------------------------------------------------------------

TEST(ChunkReader, ReadUint32) {
    std::vector<uint8_t> payload;
    push_u32(payload, 0xDEADBEEF);
    push_u32(payload, 42);
    auto chunk_data = make_chunk(chunk_id::ucfb, payload);

    ChunkReader reader(chunk_data.data(), chunk_data.size());
    EXPECT_EQ(reader.read<uint32_t>(), 0xDEADBEEF);
    EXPECT_EQ(reader.read<uint32_t>(), 42u);
    EXPECT_EQ(reader.remaining(), 0u);
}

TEST(ChunkReader, ReadFloat) {
    std::vector<uint8_t> payload;
    push_float(payload, 3.14f);
    push_float(payload, -1.0f);
    auto chunk_data = make_chunk(chunk_id::ucfb, payload);

    ChunkReader reader(chunk_data.data(), chunk_data.size());
    EXPECT_FLOAT_EQ(reader.read<float>(), 3.14f);
    EXPECT_FLOAT_EQ(reader.read<float>(), -1.0f);
}

TEST(ChunkReader, ReadThrowsOnOverflow) {
    // Payload is only 2 bytes; trying to read a 4-byte uint32 should throw.
    std::vector<uint8_t> payload = {0x01, 0x02};
    auto chunk_data = make_chunk(chunk_id::ucfb, payload);

    ChunkReader reader(chunk_data.data(), chunk_data.size());
    EXPECT_THROW(reader.read<uint32_t>(), std::out_of_range);
}

TEST(ChunkReader, ReadUint16) {
    std::vector<uint8_t> payload;
    // Manually write a little-endian uint16: 0x1234
    payload.push_back(0x34);
    payload.push_back(0x12);
    auto chunk_data = make_chunk(chunk_id::ucfb, payload);

    ChunkReader reader(chunk_data.data(), chunk_data.size());
    EXPECT_EQ(reader.read<uint16_t>(), 0x1234u);
}

// ---------------------------------------------------------------------------
// Tests: reading strings
// ---------------------------------------------------------------------------

TEST(ChunkReader, ReadString) {
    std::vector<uint8_t> payload;
    push_string(payload, "hello");
    auto chunk_data = make_chunk(chunk_id::ucfb, payload);

    ChunkReader reader(chunk_data.data(), chunk_data.size());
    EXPECT_EQ(reader.read_string(), "hello");
}

TEST(ChunkReader, ReadMultipleStrings) {
    std::vector<uint8_t> payload;
    push_string(payload, "alpha");
    push_string(payload, "beta");
    auto chunk_data = make_chunk(chunk_id::ucfb, payload);

    ChunkReader reader(chunk_data.data(), chunk_data.size());
    EXPECT_EQ(reader.read_string(), "alpha");
    EXPECT_EQ(reader.read_string(), "beta");
}

TEST(ChunkReader, ReadStringThrowsWithoutNullTerminator) {
    // Payload with no null terminator.
    std::vector<uint8_t> payload = {'h', 'e', 'l', 'l', 'o'};
    auto chunk_data = make_chunk(chunk_id::ucfb, payload);

    ChunkReader reader(chunk_data.data(), chunk_data.size());
    EXPECT_THROW(reader.read_string(), std::out_of_range);
}

// ---------------------------------------------------------------------------
// Tests: skip and remaining
// ---------------------------------------------------------------------------

TEST(ChunkReader, SkipAndRemaining) {
    std::vector<uint8_t> payload(20, 0xAA);
    auto chunk_data = make_chunk(chunk_id::ucfb, payload);

    ChunkReader reader(chunk_data.data(), chunk_data.size());
    EXPECT_EQ(reader.remaining(), 20u);

    reader.skip(8);
    EXPECT_EQ(reader.remaining(), 12u);

    reader.skip(12);
    EXPECT_EQ(reader.remaining(), 0u);
}

TEST(ChunkReader, SkipThrowsPastEnd) {
    std::vector<uint8_t> payload(4, 0);
    auto chunk_data = make_chunk(chunk_id::ucfb, payload);

    ChunkReader reader(chunk_data.data(), chunk_data.size());
    EXPECT_THROW(reader.skip(5), std::out_of_range);
}

// ---------------------------------------------------------------------------
// Tests: child chunks
// ---------------------------------------------------------------------------

TEST(ChunkReader, ReadChildChunks) {
    // Build a parent chunk that contains two child chunks.
    // Child 1: FourCC="tex_", payload = [uint32_t 42]
    std::vector<uint8_t> child1_payload;
    push_u32(child1_payload, 42);
    auto child1 = make_chunk(chunk_id::tex_, child1_payload);

    // Child 2: FourCC="modl", payload = [float 1.5f]
    std::vector<uint8_t> child2_payload;
    push_float(child2_payload, 1.5f);
    auto child2 = make_chunk(chunk_id::modl, child2_payload);

    // Combine children into parent payload.
    std::vector<uint8_t> parent_payload;
    parent_payload.insert(parent_payload.end(), child1.begin(), child1.end());
    // child1 is 12 bytes (8 header + 4 payload), already 4-byte aligned.
    parent_payload.insert(parent_payload.end(), child2.begin(), child2.end());

    auto parent = make_chunk(chunk_id::ucfb, parent_payload);

    ChunkReader reader(parent.data(), parent.size());
    EXPECT_EQ(reader.id(), chunk_id::ucfb);
    EXPECT_TRUE(reader.has_children());

    // Read first child
    ChunkReader c1 = reader.next_child();
    EXPECT_EQ(c1.id(), chunk_id::tex_);
    EXPECT_EQ(c1.size(), 4u);
    EXPECT_EQ(c1.read<uint32_t>(), 42u);

    // Read second child
    EXPECT_TRUE(reader.has_children());
    ChunkReader c2 = reader.next_child();
    EXPECT_EQ(c2.id(), chunk_id::modl);
    EXPECT_EQ(c2.size(), 4u);
    EXPECT_FLOAT_EQ(c2.read<float>(), 1.5f);

    // No more children
    EXPECT_FALSE(reader.has_children());
}

TEST(ChunkReader, GetChildrenReturnsAll) {
    // Three small children, each with a 4-byte payload.
    std::vector<uint8_t> parent_payload;

    for (int i = 0; i < 3; ++i) {
        std::vector<uint8_t> child_payload;
        push_u32(child_payload, static_cast<uint32_t>(i + 100));
        auto child = make_chunk(chunk_id::tex_, child_payload);
        parent_payload.insert(parent_payload.end(), child.begin(), child.end());
    }

    auto parent = make_chunk(chunk_id::ucfb, parent_payload);
    ChunkReader reader(parent.data(), parent.size());

    auto children = reader.get_children();
    ASSERT_EQ(children.size(), 3u);
    EXPECT_EQ(children[0].read<uint32_t>(), 100u);
    EXPECT_EQ(children[1].read<uint32_t>(), 101u);
    EXPECT_EQ(children[2].read<uint32_t>(), 102u);
}

TEST(ChunkReader, ThrowsOnNextChildWhenEmpty) {
    // Empty payload: no children.
    std::vector<uint8_t> empty_payload;
    auto chunk_data = make_chunk(chunk_id::ucfb, empty_payload);

    ChunkReader reader(chunk_data.data(), chunk_data.size());
    EXPECT_FALSE(reader.has_children());
    EXPECT_THROW(reader.next_child(), std::out_of_range);
}

// ---------------------------------------------------------------------------
// Tests: 4-byte alignment between children
// ---------------------------------------------------------------------------

TEST(ChunkReader, AlignmentBetweenChildren) {
    // Build a child whose total size (header + payload) is NOT a multiple of 4.
    // Child payload is 5 bytes => total child = 8 + 5 = 13 bytes.
    // The parent should pad to the next 4-byte boundary (16 bytes) before the
    // next child.
    std::vector<uint8_t> child1_payload = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto child1 = make_chunk(chunk_id::tex_, child1_payload);

    // Child2: 4-byte payload
    std::vector<uint8_t> child2_payload;
    push_u32(child2_payload, 0xCAFEBABE);
    auto child2 = make_chunk(chunk_id::modl, child2_payload);

    // Build parent payload with proper alignment padding.
    std::vector<uint8_t> parent_payload;
    parent_payload.insert(parent_payload.end(), child1.begin(), child1.end());

    // Pad child1 to 4-byte boundary: 13 bytes -> pad to 16
    while (parent_payload.size() % 4 != 0) {
        parent_payload.push_back(0x00);
    }

    parent_payload.insert(parent_payload.end(), child2.begin(), child2.end());

    auto parent = make_chunk(chunk_id::ucfb, parent_payload);
    ChunkReader reader(parent.data(), parent.size());

    ChunkReader c1 = reader.next_child();
    EXPECT_EQ(c1.id(), chunk_id::tex_);
    EXPECT_EQ(c1.size(), 5u);

    // The reader should skip alignment padding and find the second child.
    EXPECT_TRUE(reader.has_children());
    ChunkReader c2 = reader.next_child();
    EXPECT_EQ(c2.id(), chunk_id::modl);
    EXPECT_EQ(c2.read<uint32_t>(), 0xCAFEBABE);
}

// ---------------------------------------------------------------------------
// Tests: mixed reading (data then string)
// ---------------------------------------------------------------------------

TEST(ChunkReader, MixedDataAndStringReads) {
    std::vector<uint8_t> payload;
    push_u32(payload, 7);          // uint32_t: 7
    push_float(payload, 2.5f);     // float: 2.5
    push_string(payload, "test");  // string: "test\0" (5 bytes)

    auto chunk_data = make_chunk(chunk_id::ucfb, payload);
    ChunkReader reader(chunk_data.data(), chunk_data.size());

    EXPECT_EQ(reader.read<uint32_t>(), 7u);
    EXPECT_FLOAT_EQ(reader.read<float>(), 2.5f);
    EXPECT_EQ(reader.read_string(), "test");
}

// ---------------------------------------------------------------------------
// Tests: make_fourcc utility
// ---------------------------------------------------------------------------

TEST(MakeFourCC, ProducesExpectedValue) {
    // "ucfb" in little-endian: 'u'=0x75 at byte 0, 'c'=0x63 at byte 1,
    // 'f'=0x66 at byte 2, 'b'=0x62 at byte 3.
    FourCC expected = 0x62666375u; // 'b'<<24 | 'f'<<16 | 'c'<<8 | 'u'
    EXPECT_EQ(make_fourcc('u', 'c', 'f', 'b'), expected);
    EXPECT_EQ(chunk_id::ucfb, expected);
}

TEST(MakeFourCC, DifferentFourCCsDiffer) {
    EXPECT_NE(chunk_id::ucfb, chunk_id::modl);
    EXPECT_NE(chunk_id::tex_, chunk_id::skel);
}
