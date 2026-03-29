#pragma once

#include "core/types.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace swbf {

// ---------------------------------------------------------------------------
// ChunkReader — zero-copy reader for UCFB binary container chunks
//
// UCFB is the container format used by every .lvl asset in SWBF (2004).
// Structure of each chunk:
//
//   [FourCC id (4 bytes)] [uint32_t payload_size (LE)] [payload ...]
//
// Parent chunks contain a sequence of child chunks as their payload.
// Children are aligned to 4-byte boundaries (with null-byte padding).
//
// Usage:
//   auto root = ChunkReader(file_data.data(), file_data.size());
//   assert(root.id() == chunk_id::ucfb);
//   while (root.has_children()) {
//       auto child = root.next_child();
//       // ...
//   }
// ---------------------------------------------------------------------------

class ChunkReader {
public:
    // Construct a reader over a raw UCFB chunk (header + payload).
    // The provided data must begin with the FourCC identifier and size field.
    // The reader does NOT take ownership of the memory.
    ChunkReader(const uint8_t* data, std::size_t size);

    // -----------------------------------------------------------------------
    // Chunk identity
    // -----------------------------------------------------------------------

    // Returns the 4-byte FourCC identifier of this chunk.
    FourCC id() const { return m_id; }

    // Returns the payload size in bytes (as stored in the header).
    u32 size() const { return m_payload_size; }

    // Returns a raw pointer to the start of the payload data.
    const uint8_t* data() const { return m_payload; }

    // -----------------------------------------------------------------------
    // Child-chunk iteration (for parent/container chunks)
    // -----------------------------------------------------------------------

    // Returns true if the internal cursor has not yet reached the end of the
    // payload, meaning more child chunks can be read.
    bool has_children() const;

    // Reads the next child chunk starting at the current cursor position,
    // advances the cursor past that child (including alignment padding),
    // and returns a ChunkReader for the child.
    ChunkReader next_child();

    // Convenience: returns all remaining child chunks in order.
    std::vector<ChunkReader> get_children();

    // -----------------------------------------------------------------------
    // Sequential data reading (for leaf / data chunks)
    // -----------------------------------------------------------------------

    // Reads a value of type T from the current cursor position and advances
    // the cursor by sizeof(T).  T must be trivially copyable.
    template <typename T>
    T read() {
        static_assert(std::is_trivially_copyable_v<T>,
                      "ChunkReader::read<T>() requires a trivially copyable type");
        if (m_cursor + sizeof(T) > m_payload_size) {
            throw std::out_of_range("ChunkReader::read<T>(): not enough data");
        }
        T value;
        std::memcpy(&value, m_payload + m_cursor, sizeof(T));
        m_cursor += sizeof(T);
        return value;
    }

    // Reads a null-terminated string starting at the current cursor position.
    // Advances the cursor past the null terminator.
    std::string read_string();

    // Advances the cursor by `bytes` bytes without reading.
    void skip(std::size_t bytes);

    // Returns the number of payload bytes remaining after the cursor.
    std::size_t remaining() const;

private:
    // Align a byte offset up to the nearest 4-byte boundary.
    static std::size_t align4(std::size_t offset) {
        return (offset + 3u) & ~std::size_t{3};
    }

    // Header size: 4-byte FourCC + 4-byte uint32 size field.
    static constexpr std::size_t HEADER_SIZE = 8;

    FourCC         m_id           = 0;    // Chunk FourCC identifier
    u32            m_payload_size = 0;    // Size of the payload in bytes
    const uint8_t* m_payload      = nullptr; // Pointer to payload start
    std::size_t    m_cursor       = 0;    // Read cursor within the payload
};

} // namespace swbf
