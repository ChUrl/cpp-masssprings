// ReSharper disable CppTooWideScope
// ReSharper disable CppDFAUnreadVariable
#include "puzzle.hpp"

#include <random>
#include <unordered_set>
#include <benchmark/benchmark.h>
#include <boost/unordered/unordered_flat_map.hpp>

static std::vector<std::string> puzzles = {
    // 0: RushHour 1
    "S:[6x6] G:[4,2] M:[R] B:[{3x1 _ _ _ _ 1x3} {_ _ _ _ _ _} {_ _ 1x2 2X1 _ _} {_ _ _ 1x2 2x1 _} {1x2 _ 1x2 _ 2x1 _} {_ _ _ 3x1 _ _}]",
    // 1: RushHour 2
    "S:[6x6] G:[4,2] M:[R] B:[{1x2 3x1 _ _ 1x2 1x3} {_ 3x1 _ _ _ _} {2X1 _ 1x2 1x2 1x2 _} {2x1 _ _ _ _ _} {_ _ _ 1x2 2x1 _} {_ _ _ _ 2x1 _}]",
    // 2: RushHour 3
    "S:[6x6] G:[4,2] M:[R] B:[{3x1 _ _ 1x2 _ _} {1x2 2x1 _ _ _ 1x2} {_ 2X1 _ 1x2 1x2 _} {2x1 _ 1x2 _ _ 1x2} {_ _ _ 2x1 _ _} {_ 2x1 _ 2x1 _ _}]",
    // 3: RushHour 4
    "S:[6x6] G:[4,2] M:[R] B:[{1x3 2x1 _ _ 1x2 _} {_ 1x2 1x2 _ _ 1x3} {_ _ _ 2X1 _ _} {3x1 _ _ 1x2 _ _} {_ _ 1x2 _ 2x1 _} {2x1 _ _ 2x1 _ _}]",
    // 4: RushHour + Walls 1
    "S:[6x6] G:[4,2] M:[R] B:[{1x2 2x1 _ 1*1 _ _} {_ _ _ 1x2 2x1 _} {1x2 2X1 _ _ _ _} {_ _ 1x2 2x1 _ 1x3} {2x1 _ _ _ _ _} {2x1 _ 3x1 _ _ _}]",
    // 5: RushHour + Walls 2
    "S:[6x6] G:[4,2] M:[R] B:[{2x1 _ _ 1x2 1x2 1*1} {3x1 _ _ _ _ _} {1x2 2X1 _ 1x2 _ _} {_ _ 1x2 _ 2x1 _} {_ _ _ 2x1 _ 1x2} {_ _ 2x1 _ 1*1 _}]",
    // 6: Dad's Puzzler
    "S:[4x5] G:[0,3] M:[F] B:[{2X2 _ 2x1 _} {_ _ 2x1 _} {1x1 1x1 _ _} {1x2 1x2 2x1 _} {_ _ 2x1 _}]",
    // 7: Nine Blocks
    "S:[4x5] G:[0,3] M:[F] B:[{1x2 1x2 _ _} {_ _ 2x1 _} {1x2 1x2 2x1 _} {_ _ 2X2 _} {1x1 1x1 _ _}]",
    // 8: Quzzle
    "S:[4x5] G:[2,0] M:[F] B:[{2X2 _ 2x1 _} {_ _ 1x2 1x2} {_ _ _ _} {1x2 2x1 _ 1x1} {_ 2x1 _ 1x1}]",
    // 9: Thin Klotski
    "S:[4x5] G:[1,4] M:[F] B:[{1x2 _ 2X1 _} {_ 2x2 _ 1x1} {_ _ _ 1x1} {2x2 _ 1x1 1x1} {_ _ 1x1 1x1}]",
    // 10: Fat Klotski
    "S:[4x5] G:[1,3] M:[F] B:[{_ 2X2 _ 1x1} {1x1 _ _ 1x2} {1x1 2x2 _ _} {1x1 _ _ _} {1x1 1x1 2x1 _}]",
    // 11: Klotski
    "S:[4x5] G:[1,3] M:[F] B:[{1x2 2X2 _ 1x2} {_ _ _ _} {1x2 2x1 _ 1x2} {_ 1x1 1x1 _} {1x1 _ _ 1x1}]",
    // 12: Century
    "S:[4x5] G:[1,3] M:[F] B:[{1x1 2X2 _ 1x1} {1x2 _ _ 1x2} {_ 1x2 _ _} {1x1 _ _ 1x1} {2x1 _ 2x1 _}]",
    // 13: Super Century
    "S:[4x5] G:[1,3] M:[F] B:[{1x2 1x1 1x1 1x1} {_ 1x2 2X2 _} {1x2 _ _ _} {_ 2x1 _ 1x1} {_ 2x1 _ _}]",
    // 14: Supercompo
    "S:[4x5] G:[1,3] M:[F] B:[{_ 2X2 _ _} {1x1 _ _ 1x1} {1x2 2x1 _ 1x2} {_ 2x1 _ _} {1x1 2x1 _ 1x1}]",
};

template <u8 N>
struct uint_hasher
{
    int64_t nums;

    auto operator()(const std::array<u64, N>& ints) const noexcept -> size_t
    {
        size_t h = 0;
        for (size_t i = 0; i < N; ++i) {
            puzzle::hash_combine(h, ints[i]);
        }
        return h;
    }
};

template <u8 N>
static auto unordered_set_uint64(benchmark::State& state) -> void
{
    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<u64> distribution(
        std::numeric_limits<u64>::min(),
        std::numeric_limits<u64>::max()
    );

    std::unordered_set<std::array<u64, N>, uint_hasher<N>> set;
    std::array<u64, N> ints;
    for (size_t i = 0; i < N; ++i) {
        ints[i] = distribution(generator);
    }

    for (auto _ : state) {
        for (size_t i = 0; i < 100000; ++i) {
            set.emplace(ints);
        }

        benchmark::DoNotOptimize(set);
    }
}

template <u8 N>
static auto unordered_flat_set_uint64(benchmark::State& state) -> void
{
    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<u64> distribution(
        std::numeric_limits<u64>::min(),
        std::numeric_limits<u64>::max()
    );

    boost::unordered_flat_set<std::array<u64, N>, uint_hasher<N>> set;
    std::array<u64, N> ints;
    for (size_t i = 0; i < N; ++i) {
        ints[i] = distribution(generator);
    }

    for (auto _ : state) {
        for (size_t i = 0; i < 100000; ++i) {
            set.emplace(ints);
        }

        benchmark::DoNotOptimize(set);
    }
}

static auto unordered_flat_set_block_hasher(benchmark::State& state) -> void
{
    blockset set;
    const block b = block(2, 3, 1, 2, true, false);

    for (auto _ : state) {
        for (size_t i = 0; i < 100000; ++i) {
            set.emplace(b);
        }

        benchmark::DoNotOptimize(set);
    }
}

static auto unordered_flat_set_block_hasher2(benchmark::State& state) -> void
{
    blockset2 set;
    const block b = block(2, 3, 1, 2, true, false);

    for (auto _ : state) {
        for (size_t i = 0; i < 100000; ++i) {
            set.emplace(b);
        }

        benchmark::DoNotOptimize(set);
    }
}

static auto unordered_flat_set_puzzle_hasher(benchmark::State& state) -> void
{
    puzzleset set;
    const puzzle p = puzzle(puzzles[0]);

    for (auto _ : state) {
        for (size_t i = 0; i < 100000; ++i) {
            set.emplace(p);
        }

        benchmark::DoNotOptimize(set);
    }
}

static auto explore_state_space(benchmark::State& state) -> void
{
    const puzzle p = puzzle(puzzles[state.range(0)]);

    for (auto _ : state) {
        auto space = p.explore_state_space();

        benchmark::DoNotOptimize(space);
    }
}

static auto explore_rush_hour_puzzle_space(benchmark::State& state) -> void
{
    constexpr u8 max_blocks = 5;

    constexpr u8 board_width = 4;
    constexpr u8 board_height = 5;
    constexpr u8 goal_x = board_width - 1;
    constexpr u8 goal_y = 2;
    constexpr bool restricted = true;

    const blockset2 permitted_blocks = {
        block(0, 0, 2, 1, false, false),
        block(0, 0, 3, 1, false, false),
        block(0, 0, 1, 2, false, false),
        block(0, 0, 1, 3, false, false)
    };
    const block target_block = block(0, 0, 2, 1, true, false);
    constexpr std::tuple<u8, u8, u8, u8> target_block_pos_range = {
        0,
        goal_y,
        goal_x,
        goal_y
    };

    const puzzle p = puzzle(board_width, board_height, goal_x, goal_y, restricted, true);

    for (auto _ : state) {
        puzzleset result = p.explore_puzzle_space(
            permitted_blocks,
            target_block,
            target_block_pos_range,
            max_blocks,
            0,
            std::nullopt);

        benchmark::DoNotOptimize(result);
    }
}

BENCHMARK(unordered_set_uint64<4>)->Unit(benchmark::kMicrosecond);
BENCHMARK(unordered_set_uint64<8>)->Unit(benchmark::kMicrosecond);
BENCHMARK(unordered_set_uint64<16>)->Unit(benchmark::kMicrosecond);
BENCHMARK(unordered_flat_set_uint64<4>)->Unit(benchmark::kMicrosecond);
BENCHMARK(unordered_flat_set_uint64<8>)->Unit(benchmark::kMicrosecond);
BENCHMARK(unordered_flat_set_uint64<16>)->Unit(benchmark::kMicrosecond);
BENCHMARK(unordered_flat_set_block_hasher)->Unit(benchmark::kMicrosecond);
BENCHMARK(unordered_flat_set_block_hasher2)->Unit(benchmark::kMicrosecond);
BENCHMARK(unordered_flat_set_puzzle_hasher)->Unit(benchmark::kMicrosecond);
BENCHMARK(explore_state_space)->DenseRange(0, puzzles.size() - 1)->Unit(benchmark::kMicrosecond);
BENCHMARK(explore_rush_hour_puzzle_space)->Unit(benchmark::kSecond);

BENCHMARK_MAIN();