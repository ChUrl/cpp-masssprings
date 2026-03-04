#include "puzzle.hpp"

#include <benchmark/benchmark.h>

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

static auto explore_state_space(benchmark::State& state) -> void
{
    const puzzle p = puzzle(puzzles[state.range(0)]);

    for (auto _ : state) {
        auto space = p.explore_state_space();

        benchmark::DoNotOptimize(space);
    }
}

BENCHMARK(explore_state_space)->DenseRange(0, puzzles.size() - 1)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();