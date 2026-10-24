#include "memory_tracker.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <new>

TEST_CASE("global allocation tracker follows live and peak bytes", "[memory]")
{
    const memory_tracker::snapshot before = memory_tracker::get_snapshot();

    void* pointer = ::operator new(4096);
    const memory_tracker::snapshot allocated = memory_tracker::get_snapshot();

    REQUIRE(allocated.live_bytes == before.live_bytes + 4096);
    REQUIRE(allocated.allocations == before.allocations + 1);
    REQUIRE(allocated.peak_bytes >= allocated.live_bytes);

    ::operator delete(pointer);
    const memory_tracker::snapshot released = memory_tracker::get_snapshot();

    REQUIRE(released.live_bytes == allocated.live_bytes - 4096);
    REQUIRE(released.frees == before.frees + 1);
}

TEST_CASE("global allocation tracker handles aligned allocations", "[memory]")
{
    const memory_tracker::snapshot before = memory_tracker::get_snapshot();

    void* pointer = ::operator new(256, std::align_val_t{64});
    REQUIRE(reinterpret_cast<std::uintptr_t>(pointer) % 64 == 0);

    const memory_tracker::snapshot allocated = memory_tracker::get_snapshot();
    REQUIRE(allocated.live_bytes == before.live_bytes + 256);

    ::operator delete(pointer, std::align_val_t{64});
    const memory_tracker::snapshot released = memory_tracker::get_snapshot();
    REQUIRE(released.live_bytes == allocated.live_bytes - 256);
}
