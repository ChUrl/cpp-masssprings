#include "config.hpp"
#include "octree.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <random>
#include <vector>

TEST_CASE("Parallel Morton sort matches the serial tree", "[octree]")
{
    std::mt19937 generator(123456);
    std::uniform_real_distribution<float> distribution(-100.0f, 100.0f);

    std::vector<Vector3> positions;
    positions.reserve(12000);
    for (int i = 0; i < 12000; ++i) {
        positions.emplace_back(distribution(generator), distribution(generator),
                               distribution(generator));
    }

    simulation_settings settings;
    settings.max_depth = 8;
    settings.small_task_block_size = 64;
    settings.gpu_physics = true;

    BS::thread_pool<> pool(4);
    threadpool parallel_pool = &pool;

    octree parallel_tree;
    octree serial_tree;
    octree::build_octree_morton(parallel_tree, positions, parallel_pool, settings);
    octree::build_octree_morton(serial_tree, positions, std::nullopt, settings);

    REQUIRE(parallel_tree.node_count() == serial_tree.node_count());
    REQUIRE(parallel_tree.child_count() == serial_tree.child_count());
    REQUIRE(parallel_tree.morton_order() == serial_tree.morton_order());

    for (int query = 0; query < 64; ++query) {
        const size_t index = static_cast<size_t>(query) * positions.size() / 64;
        const int self = static_cast<int>(index);
        const Vector3 parallel_force = parallel_tree.calculate_force_morton(
            parallel_tree.node_root_index(), positions[index], self);
        const Vector3 serial_force = serial_tree.calculate_force_morton(
            serial_tree.node_root_index(), positions[index], self);

        REQUIRE(parallel_force.x == Catch::Approx(serial_force.x).margin(1e-3f));
        REQUIRE(parallel_force.y == Catch::Approx(serial_force.y).margin(1e-3f));
        REQUIRE(parallel_force.z == Catch::Approx(serial_force.z).margin(1e-3f));
    }
}
