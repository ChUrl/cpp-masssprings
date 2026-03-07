// ReSharper disable CppLocalVariableMayBeConst
#include "puzzle.hpp"

#include <random>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

static auto board_mask(const int w, const int h) -> u64
{
    const int cells = w * h;
    if (cells == 64) {
        return ~0ULL;
    }
    return (1ULL << cells) - 1ULL;
}

TEST_CASE("Empty board returns (0,0)", "[puzzle][board]")
{
    puzzle p(5, 5);

    int x = -1, y = -1;
    REQUIRE(p.bitmap_find_first_empty(0ULL, x, y));

    REQUIRE(x == 0);
    REQUIRE(y == 0);
}

TEST_CASE("Full board detection respects width*height only", "[puzzle][board]")
{
    auto [w, h] = GENERATE(std::tuple{3, 3}, std::tuple{4, 4}, std::tuple{5, 6}, std::tuple{8, 8});

    puzzle p(w, h);

    u64 mask = board_mask(w, h);

    int x = -1, y = -1;

    REQUIRE_FALSE(p.bitmap_find_first_empty(mask, x, y));

    // Bits outside board should not affect fullness
    REQUIRE_FALSE(p.bitmap_find_first_empty(mask | (~mask), x, y));
}

TEST_CASE("Single empty cell at various positions", "[puzzle][board]")
{
    auto [w, h] = GENERATE(std::tuple{3, 3}, std::tuple{4, 4}, std::tuple{5, 5}, std::tuple{8, 8});

    puzzle p(w, h);

    int cells = w * h;

    auto empty_index = GENERATE_COPY(values<int>({ 0, cells / 2, cells - 1}));

    u64 bitmap = board_mask(w, h);
    bitmap &= ~(1ULL << empty_index);

    int x = -1, y = -1;
    REQUIRE(p.bitmap_find_first_empty(bitmap, x, y));

    REQUIRE(x == empty_index % w);
    REQUIRE(y == empty_index / w);
}

TEST_CASE("Bits outside board are ignored", "[puzzle][board]")
{
    puzzle p(3, 3); // 9 cells

    u64 mask = board_mask(3, 3);

    // Board is full
    u64 bitmap = mask;

    // Set extra bits outside 9 cells
    bitmap |= (1ULL << 20);
    bitmap |= (1ULL << 63);

    int x = -1, y = -1;
    REQUIRE_FALSE(p.bitmap_find_first_empty(bitmap, x, y));
}

TEST_CASE("First empty found in forward search branch", "[puzzle][branch]")
{
    puzzle p(4, 4); // 16 cells

    // Only MSB (within board) set
    u64 bitmap = (1ULL << 15);

    int x = -1, y = -1;
    REQUIRE(p.bitmap_find_first_empty(bitmap, x, y));

    REQUIRE(x == 0);
    REQUIRE(y == 0);
}

TEST_CASE("Backward search branch finds gap before MSB cluster", "[puzzle][branch]")
{
    puzzle p(4, 4); // 16 cells

    // Set bits 15,14,13 but leave 12 empty
    u64 bitmap = (1ULL << 15) | (1ULL << 14) | (1ULL << 13);

    int x = -1, y = -1;
    REQUIRE(p.bitmap_find_first_empty(bitmap, x, y));

    REQUIRE(x == 0);
    REQUIRE(y == 0);
}

TEST_CASE("Rectangular board coordinate mapping", "[puzzle][rect]")
{
    puzzle p(5, 3); // 15 cells

    int empty_index = 11;

    u64 bitmap = board_mask(5, 3);
    bitmap &= ~(1ULL << empty_index);

    int x = -1, y = -1;
    REQUIRE(p.bitmap_find_first_empty(bitmap, x, y));

    REQUIRE(x == empty_index % 5);
    REQUIRE(y == empty_index / 5);
}

TEST_CASE("Non-64-sized board near limit", "[puzzle][limit]")
{
    puzzle p(7, 8); // 56 cells

    u64 mask = board_mask(7, 8);

    // Full board should return false
    int x = -1, y = -1;
    REQUIRE_FALSE(p.bitmap_find_first_empty(mask, x, y));

    // Clear highest valid cell
    int empty_index = 55;
    mask &= ~(1ULL << empty_index);

    REQUIRE(p.bitmap_find_first_empty(mask, x, y));
    REQUIRE(x == empty_index % 7);
    REQUIRE(y == empty_index / 7);
}

// --- Oracle: find first zero bit inside board ---
static auto oracle_find_first_empty(u64 bitmap, int w, int h, int& x, int& y) -> bool
{
    int cells = w * h;

    for (int i = 0; i < cells; ++i) {
        if ((bitmap & (1ULL << i)) == 0) {
            x = i % w;
            y = i / w;
            return true;
        }
    }

    return false;
}

TEST_CASE("Oracle validation across board sizes 3x3 to 8x8", "[puzzle][oracle]")
{
    auto [w, h] = GENERATE(std::tuple{3, 3}, std::tuple{4, 4}, std::tuple{5, 5}, std::tuple{6, 6}, std::tuple{7, 7},
                           std::tuple{8, 8}, std::tuple{3, 5}, std::tuple{5, 3}, std::tuple{7, 8}, std::tuple{8, 7});

    puzzle p(w, h);

    u64 mask = board_mask(w, h);

    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<u64> dist(0, UINT64_MAX);

    for (int iteration = 0; iteration < 200; ++iteration) {
        u64 bitmap = dist(rng);

        int ox = -1, oy = -1;
        bool oracle_result = oracle_find_first_empty(bitmap, w, h, ox, oy);

        int x = -1, y = -1;
        bool result = p.bitmap_find_first_empty(bitmap, x, y);

        REQUIRE(result == oracle_result);

        if (result) {
            REQUIRE(x == ox);
            REQUIRE(y == oy);
        }
    }
}

TEST_CASE("Bits set outside board only behaves as empty board", "[puzzle][outside]")
{
    puzzle p(3, 3); // 9 cells

    u64 bitmap = (1ULL << 40) | (1ULL << 63);

    int x = -1, y = -1;
    REQUIRE(p.bitmap_find_first_empty(bitmap, x, y));

    REQUIRE(x == 0);
    REQUIRE(y == 0);
}

TEST_CASE("Last valid cell empty stresses upper bound", "[puzzle][boundary]")
{
    auto [w, h] = GENERATE(std::tuple{4, 4}, std::tuple{5, 6}, std::tuple{7, 8}, std::tuple{8, 8});

    puzzle p(w, h);

    int cells = w * h;
    u64 bitmap = board_mask(w, h);

    // Clear last valid bit
    bitmap &= ~(1ULL << (cells - 1));

    int x = -1, y = -1;
    REQUIRE(p.bitmap_find_first_empty(bitmap, x, y));

    REQUIRE(x == (cells - 1) % w);
    REQUIRE(y == (cells - 1) / w);
}

TEST_CASE("Board sizes between 33 and 63 cells", "[puzzle][midrange]")
{
    auto [w, h] = GENERATE(std::tuple{6, 6}, // 36
                           std::tuple{7, 6}, // 42
                           std::tuple{7, 7}, // 49
                           std::tuple{8, 7}, // 56
                           std::tuple{7, 8}  // 56
    );

    puzzle p(w, h);

    int cells = w * h;

    for (int index : {31, 32, cells - 2}) {
        if (index >= cells) continue;

        u64 bitmap = board_mask(w, h);
        bitmap &= ~(1ULL << index);

        int x = -1, y = -1;
        REQUIRE(p.bitmap_find_first_empty(bitmap, x, y));

        REQUIRE(x == index % w);
        REQUIRE(y == index / w);
    }
}

TEST_CASE("Multiple holes choose lowest index", "[puzzle][multiple]")
{
    puzzle p(5, 5);

    u64 bitmap = board_mask(5, 5);

    // Clear several positions
    bitmap &= ~(1ULL << 3);
    bitmap &= ~(1ULL << 7);
    bitmap &= ~(1ULL << 12);

    int x = -1, y = -1;
    REQUIRE(p.bitmap_find_first_empty(bitmap, x, y));

    // Oracle expectation: index 3
    REQUIRE(x == 3 % 5);
    REQUIRE(y == 3 / 5);
}