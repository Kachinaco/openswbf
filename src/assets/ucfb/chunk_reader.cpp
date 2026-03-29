#include "chunk_reader.h"

#include "core/log.h"

#include <cstring>
#include <stdexcept>

namespace swbf {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ChunkReader::ChunkReader(const uint8_t* data, std::size_t size) {
    if (!data || size < HEADER_SIZE) {
        throw std::runtime_error(
            "ChunkReader: buffer is null or too small to contain a chunk header");
    }

    // Read the FourCC identifier (first 4 bytes, little-endian u32).
    std::memcpy(&m_id, data, sizeof(FourCC));

    // Read the payload size (next 4 bytes, little-endian u32).
    std::memcpy(&m_payload_size, data + 4, sizeof(u32));

    // Sanity check: the declared payload must fit within the provided buffer.
    if (HEADER_SIZE + m_payload_size > size) {
        throw std::runtime_error(
            "ChunkReader: payload size exceeds available buffer");
    }

    m_payload = data + HEADER_SIZE;
    m_cursor  = 0;
}

// ---------------------------------------------------------------------------
// Child-chunk iteration
// ---------------------------------------------------------------------------

bool ChunkReader::has_children() const {
    // We need at least a full header (8 bytes) remaining to have another child.
    return m_cursor + HEADER_SIZE <= m_payload_size;
}

ChunkReader ChunkReader::next_child() {
    if (!has_children()) {
        throw std::out_of_range(
            "ChunkReader::next_child(): no more children in this chunk");
    }

    const uint8_t* child_ptr  = m_payload + m_cursor;
    std::size_t    bytes_left = m_payload_size - m_cursor;

    // Construct a reader for the child chunk.
    ChunkReader child(child_ptr, bytes_left);

    // Advance our cursor past the child's header + payload, then align to
    // the next 4-byte boundary (UCFB pads with null bytes between chunks).
    std::size_t child_total = HEADER_SIZE + child.size();
    m_cursor += align4(child_total);

    // Clamp to payload size in case the final child has no trailing padding.
    if (m_cursor > m_payload_size) {
        m_cursor = m_payload_size;
    }

    return child;
}

std::vector<ChunkReader> ChunkReader::get_children() {
    std::vector<ChunkReader> children;
    while (has_children()) {
        children.push_back(next_child());
    }
    return children;
}

// ---------------------------------------------------------------------------
// Sequential data reading
// ---------------------------------------------------------------------------

std::string ChunkReader::read_string() {
    // Scan forward for a null terminator.
    const uint8_t* start = m_payload + m_cursor;
    const uint8_t* end   = m_payload + m_payload_size;
    const uint8_t* p     = start;

    while (p < end && *p != '\0') {
        ++p;
    }

    if (p >= end) {
        throw std::out_of_range(
            "ChunkReader::read_string(): no null terminator found");
    }

    std::string result(reinterpret_cast<const char*>(start),
                       static_cast<std::size_t>(p - start));

    // Advance cursor past the null terminator.
    m_cursor += static_cast<std::size_t>(p - start) + 1;

    return result;
}

void ChunkReader::skip(std::size_t bytes) {
    if (m_cursor + bytes > m_payload_size) {
        throw std::out_of_range(
            "ChunkReader::skip(): would advance past end of payload");
    }
    m_cursor += bytes;
}

std::size_t ChunkReader::remaining() const {
    return m_payload_size - m_cursor;
}

} // namespace swbf
