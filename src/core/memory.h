#pragma once

#include <cstddef>
#include <cstdint>

namespace swbf {

// ---------------------------------------------------------------------------
// ArenaAllocator
//
// A simple bump allocator that hands out memory from a contiguous block.
// Individual allocations cannot be freed; call reset() to reclaim everything
// at once. Useful for per-frame or per-level transient data.
// ---------------------------------------------------------------------------

class ArenaAllocator {
public:
    // Construct an arena that owns `capacity_bytes` of memory.
    explicit ArenaAllocator(std::size_t capacity_bytes);

    // Non-copyable, movable.
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;
    ArenaAllocator(ArenaAllocator&& other) noexcept;
    ArenaAllocator& operator=(ArenaAllocator&& other) noexcept;

    ~ArenaAllocator();

    // Allocate `size` bytes with the given alignment (must be power of 2).
    // Returns nullptr if the arena is exhausted.
    void* allocate(std::size_t size, std::size_t alignment = 16);

    // Typed helper — allocates sizeof(T)*count bytes aligned to alignof(T).
    template <typename T>
    T* allocate(std::size_t count = 1) {
        void* ptr = allocate(sizeof(T) * count, alignof(T));
        return static_cast<T*>(ptr);
    }

    // Free all allocations at once. Does not release the backing memory.
    void reset();

    // Number of bytes currently in use.
    std::size_t used() const;

    // Total capacity of the arena in bytes.
    std::size_t capacity() const;

private:
    std::uint8_t* m_base     = nullptr;
    std::size_t   m_capacity = 0;
    std::size_t   m_offset   = 0;
};

} // namespace swbf
