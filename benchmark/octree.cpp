#include "octree.hpp"
#include "config.hpp"

#include <benchmark/benchmark.h>
#include <random>
#include <vector>

namespace
{
auto make_positions(const size_t count) -> std::vector<Vector3>
{
    std::mt19937 generator(42);
    std::uniform_real_distribution<float> distribution(-100.0f, 100.0f);

    std::vector<Vector3> positions;
    positions.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        positions.emplace_back(distribution(generator), distribution(generator),
                               distribution(generator));
    }
    return positions;
}

auto build_tree(benchmark::State& state, const bool parallel) -> void
{
    const std::vector<Vector3> positions = make_positions(431501);
    simulation_settings settings;
    settings.max_depth = 8;
    settings.small_task_block_size = 256;
    settings.gpu_physics = parallel;

    BS::thread_pool<> pool(22);
    threadpool thread_pool = parallel ? threadpool{&pool} : std::nullopt;
    octree tree;

    for (auto _ : state) {
        octree::build_octree_morton(tree, positions, thread_pool, settings);
        benchmark::DoNotOptimize(tree.node_count());
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(positions.size()));
    state.counters["bbox_ms"] = tree.stage_ms[0];
    state.counters["morton_ms"] = tree.stage_ms[1];
    state.counters["sort_ms"] = tree.stage_ms[2];
    state.counters["leaves_ms"] = tree.stage_ms[3];
    state.counters["levels_ms"] = tree.stage_ms[4];
    state.counters["flatten_ms"] = tree.stage_ms[5];
}
} // namespace

static auto build_octree_parallel(benchmark::State& state) -> void
{
    build_tree(state, true);
}

static auto build_octree_serial(benchmark::State& state) -> void
{
    build_tree(state, false);
}

BENCHMARK(build_octree_parallel)->Unit(benchmark::kMillisecond);
BENCHMARK(build_octree_serial)->Unit(benchmark::kMillisecond);
