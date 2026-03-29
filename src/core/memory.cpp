#include "memory.h"

#include <cstdlib>
#include <cstring>
#include <new>
#include <utility>

namespace swbf {

ArenaAllocator::ArenaAllocator(std::size_t capacity_bytes)
    : m_capacity(capacity_bytes)
    , m_offset(0)
{
    // Use aligned_alloc so the base pointer itself is well-aligned.
    // C11 requires size to be a multiple of alignment for aligned_alloc,
    // so round up to the next 64-byte boundary.
    constexpr std::size_t BASE_ALIGN = 64;
    std::size_t alloc_size = (capacity_bytes + BASE_ALIGN - 1) & ~(BASE_ALIGN - 1);
    m_base = static_cast<std::uint8_t*>(std::aligned_alloc(BASE_ALIGN, alloc_size));
    if (!m_base) {
        m_capacity = 0;
    }
}

ArenaAllocator::ArenaAllocator(ArenaAllocator&& other) noexcept
    : m_base(other.m_base)
    , m_capacity(other.m_capacity)
    , m_offset(other.m_offset)
{
    other.m_base     = nullptr;
    other.m_capacity = 0;
    other.m_offset   = 0;
}

ArenaAllocator& ArenaAllocator::operator=(ArenaAllocator&& other) noexcept {
    if (this != &other) {
        std::free(m_base);
        m_base     = other.m_base;
        m_capacity = other.m_capacity;
        m_offset   = other.m_offset;
        other.m_base     = nullptr;
        other.m_capacity = 0;
        other.m_offset   = 0;
    }
    return *this;
}

ArenaAllocator::~ArenaAllocator() {
    std::free(m_base);
}

void* ArenaAllocator::allocate(std::size_t size, std::size_t alignment) {
    // Align the current offset up to the requested alignment.
    std::size_t aligned_offset = (m_offset + alignment - 1) & ~(alignment - 1);

    if (aligned_offset + size > m_capacity) {
        return nullptr; // Out of memory in this arena.
    }

    void* ptr = m_base + aligned_offset;
    m_offset = aligned_offset + size;
    return ptr;
}

void ArenaAllocator::reset() {
    m_offset = 0;
}

std::size_t ArenaAllocator::used() const {
    return m_offset;
}

std::size_t ArenaAllocator::capacity() const {
    return m_capacity;
}

} // namespace swbf
