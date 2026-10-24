#include "memory_tracker.hpp"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace
{
constexpr size_t allocation_magic = 0x4d534d454d545241ull;

struct alignas(std::max_align_t) allocation_header
{
    void* raw = nullptr;
    size_t size = 0;
    size_t magic = allocation_magic;
};

static_assert(sizeof(allocation_header) % alignof(std::max_align_t) == 0,
              "allocation header must preserve max_align_t alignment");

std::atomic<size_t> live_bytes{0};
std::atomic<size_t> peak_bytes{0};
std::atomic<size_t> allocation_count{0};
std::atomic<size_t> free_count{0};

auto record_allocation(const size_t size) -> void
{
    const size_t current = live_bytes.fetch_add(size, std::memory_order_relaxed) + size;
    allocation_count.fetch_add(1, std::memory_order_relaxed);

    size_t peak = peak_bytes.load(std::memory_order_relaxed);
    while (current > peak &&
           !peak_bytes.compare_exchange_weak(peak, current, std::memory_order_relaxed)) {
    }
}

auto record_free(const size_t size) -> void
{
    live_bytes.fetch_sub(size, std::memory_order_relaxed);
    free_count.fetch_add(1, std::memory_order_relaxed);
}

auto allocate_regular(const size_t size) -> void*
{
    void* raw = std::malloc(sizeof(allocation_header) + size);
    if (raw == nullptr) {
        throw std::bad_alloc();
    }

    auto* header = static_cast<allocation_header*>(raw);
    header->raw = raw;
    header->size = size;
    header->magic = allocation_magic;
    record_allocation(size);
    return header + 1;
}

auto allocate_aligned(const size_t size, const std::align_val_t alignment_value) -> void*
{
    const size_t alignment = static_cast<size_t>(alignment_value);
    if (alignment <= alignof(std::max_align_t)) {
        return allocate_regular(size);
    }
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        throw std::bad_alloc();
    }

    const size_t total = sizeof(allocation_header) + size + alignment - 1;
    void* raw = std::malloc(total);
    if (raw == nullptr) {
        throw std::bad_alloc();
    }

    const auto start = reinterpret_cast<std::uintptr_t>(raw) + sizeof(allocation_header);
    const auto aligned = (start + alignment - 1) & ~(alignment - 1);
    auto* header = reinterpret_cast<allocation_header*>(aligned) - 1;
    header->raw = raw;
    header->size = size;
    header->magic = allocation_magic;
    record_allocation(size);
    return reinterpret_cast<void*>(aligned);
}

auto release(void* pointer) -> void
{
    if (pointer == nullptr) {
        return;
    }

    auto* header = static_cast<allocation_header*>(pointer) - 1;
    const size_t size = header->size;
    void* raw = header->raw;
    header->magic = 0;
    record_free(size);
    std::free(raw);
}
} // namespace

auto memory_tracker::get_snapshot() -> snapshot
{
    snapshot result;
    result.live_bytes = live_bytes.load(std::memory_order_relaxed);
    result.peak_bytes = peak_bytes.load(std::memory_order_relaxed);
    result.allocations = allocation_count.load(std::memory_order_relaxed);
    result.frees = free_count.load(std::memory_order_relaxed);
    return result;
}

void* operator new(const std::size_t size)
{
    return allocate_regular(size);
}

void* operator new[](const std::size_t size)
{
    return allocate_regular(size);
}

void operator delete(void* pointer) noexcept
{
    release(pointer);
}

void operator delete[](void* pointer) noexcept
{
    release(pointer);
}

void operator delete(void* pointer, const std::size_t) noexcept
{
    release(pointer);
}

void operator delete[](void* pointer, const std::size_t) noexcept
{
    release(pointer);
}

void* operator new(const std::size_t size, const std::nothrow_t&) noexcept
{
    try {
        return allocate_regular(size);
    }
    catch (...) {
        return nullptr;
    }
}

void* operator new[](const std::size_t size, const std::nothrow_t&) noexcept
{
    try {
        return allocate_regular(size);
    }
    catch (...) {
        return nullptr;
    }
}

void operator delete(void* pointer, const std::nothrow_t&) noexcept
{
    release(pointer);
}

void operator delete[](void* pointer, const std::nothrow_t&) noexcept
{
    release(pointer);
}

void* operator new(const std::size_t size, const std::align_val_t alignment)
{
    return allocate_aligned(size, alignment);
}

void* operator new[](const std::size_t size, const std::align_val_t alignment)
{
    return allocate_aligned(size, alignment);
}

void operator delete(void* pointer, const std::align_val_t) noexcept
{
    release(pointer);
}

void operator delete[](void* pointer, const std::align_val_t) noexcept
{
    release(pointer);
}

void operator delete(void* pointer, const std::size_t, const std::align_val_t) noexcept
{
    release(pointer);
}

void operator delete[](void* pointer, const std::size_t, const std::align_val_t) noexcept
{
    release(pointer);
}

void* operator new(const std::size_t size, const std::align_val_t alignment,
                   const std::nothrow_t&) noexcept
{
    try {
        return allocate_aligned(size, alignment);
    }
    catch (...) {
        return nullptr;
    }
}

void* operator new[](const std::size_t size, const std::align_val_t alignment,
                     const std::nothrow_t&) noexcept
{
    try {
        return allocate_aligned(size, alignment);
    }
    catch (...) {
        return nullptr;
    }
}

void operator delete(void* pointer, const std::align_val_t, const std::nothrow_t&) noexcept
{
    release(pointer);
}

void operator delete[](void* pointer, const std::align_val_t, const std::nothrow_t&) noexcept
{
    release(pointer);
}
