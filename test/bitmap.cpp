// ReSharper disable CppLocalVariableMayBeConst
#include "puzzle.hpp"

#include <random>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("bitmap_is_full all bits set", "[puzzle][board]")
{
    puzzle p1(5, 5);
    puzzle p2(3, 4);
    puzzle p3(5, 4);
    puzzle p4(3, 7);
    uint64_t bitmap = -1;

    REQUIRE(p1.bitmap_is_full(bitmap));
    REQUIRE(p2.bitmap_is_full(bitmap));
    REQUIRE(p3.bitmap_is_full(bitmap));
    REQUIRE(p4.bitmap_is_full(bitmap));
}

TEST_CASE("bitmap_is_full no bits set", "[puzzle][board]")
{
    puzzle p1(5, 5);
    puzzle p2(3, 4);
    puzzle p3(5, 4);
    puzzle p4(3, 7);
    uint64_t bitmap = 0;

    REQUIRE_FALSE(p1.bitmap_is_full(bitmap));
    REQUIRE_FALSE(p2.bitmap_is_full(bitmap));
    REQUIRE_FALSE(p3.bitmap_is_full(bitmap));
    REQUIRE_FALSE(p4.bitmap_is_full(bitmap));
}

TEST_CASE("bitmap_is_full necessary bits set", "[puzzle][board]")
{
    puzzle p1(5, 5);
    puzzle p2(3, 4);
    puzzle p3(5, 4);
    puzzle p4(3, 7);

    uint64_t bitmap1 = (1ull << 25) - 1; // 5 * 5
    uint64_t bitmap2 = (1ull << 12) - 1;  // 3 * 4
    uint64_t bitmap3 = (1ull << 20) - 1; // 5 * 4
    uint64_t bitmap4 = (1ull << 21) - 1; // 3 * 7

    REQUIRE(p1.bitmap_is_full(bitmap1));
    REQUIRE(p2.bitmap_is_full(bitmap2));
    REQUIRE(p3.bitmap_is_full(bitmap3));
    REQUIRE(p4.bitmap_is_full(bitmap4));
}

TEST_CASE("bitmap_is_full necessary bits not set", "[puzzle][board]")
{
    puzzle p1(5, 5);
    puzzle p2(3, 4);
    puzzle p3(5, 4);
    puzzle p4(3, 7);

    uint64_t bitmap1 = (1ull << 25) - 1; // 5 * 5
    uint64_t bitmap2 = (1ull << 12) - 1;  // 3 * 4
    uint64_t bitmap3 = (1ull << 20) - 1; // 5 * 4
    uint64_t bitmap4 = (1ull << 21) - 1; // 3 * 7

    bitmap1 &= ~(1ull << 12);
    bitmap2 &= ~(1ull << 6);
    bitmap3 &= ~(1ull << 8);
    bitmap4 &= ~(1ull << 18);

    REQUIRE_FALSE(p1.bitmap_is_full(bitmap1));
    REQUIRE_FALSE(p2.bitmap_is_full(bitmap2));
    REQUIRE_FALSE(p3.bitmap_is_full(bitmap3));
    REQUIRE_FALSE(p4.bitmap_is_full(bitmap4));
}