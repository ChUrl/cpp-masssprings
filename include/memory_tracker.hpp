#ifndef MEMORY_TRACKER_HPP_
#define MEMORY_TRACKER_HPP_

#include <cstddef>

/**
 * Process-wide tracker for allocations made through C++ new/delete. It reports live requested
 * bytes, the peak live value and allocation/free counts. It does not include malloc/C allocations,
 * GPU memory, static storage or thread stacks, and live bytes are not the same as resident memory.
 */
namespace memory_tracker
{
struct snapshot
{
    size_t live_bytes = 0;
    size_t peak_bytes = 0;
    size_t allocations = 0;
    size_t frees = 0;
};

[[nodiscard]] auto get_snapshot() -> snapshot;
} // namespace memory_tracker

#endif
