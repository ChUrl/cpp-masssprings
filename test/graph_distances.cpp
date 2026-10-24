#include "graph_distances.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

TEST_CASE("graph distances traverse a path from the target", "[distances]")
{
    graph_distances distances;
    const std::vector<spring> edges = {{0, 1}, {1, 2}, {2, 3}, {3, 4}};

    distances.calculate_distances(5, edges, {0});

    REQUIRE(distances.distances == std::vector<int>{0, 1, 2, 3, 4});
    REQUIRE(distances.nearest_targets == std::vector<std::uint32_t>{0, 0, 0, 0, 0});
    REQUIRE(distances.get_shortest_path(4) == std::vector<size_t>{4, 3, 2, 1, 0});
}

TEST_CASE("graph distances follow directed edges towards the target", "[distances]")
{
    graph_distances distances;
    const std::vector<spring> edges = {{0, 1}, {1, 2}};

    distances.calculate_distances(3, edges, {2}, true);

    REQUIRE(distances.distances == std::vector<int>{2, 1, 0});
    REQUIRE(distances.nearest_targets == std::vector<std::uint32_t>{2, 2, 2});
    REQUIRE(distances.get_shortest_path(0) == std::vector<size_t>{0, 1, 2});
}

TEST_CASE("graph distances handle disconnected nodes and multiple targets", "[distances]")
{
    graph_distances distances;
    const std::vector<spring> edges = {{0, 1}, {1, 2}, {4, 5}};

    distances.calculate_distances(7, edges, {2, 4});

    REQUIRE(distances.distances == std::vector<int>{2, 1, 0, -1, 0, 1, -1});
    REQUIRE(distances.nearest_targets[0] == 2);
    REQUIRE(distances.nearest_targets[1] == 2);
    REQUIRE(distances.nearest_targets[2] == 2);
    REQUIRE(distances.nearest_targets[4] == 4);
    REQUIRE(distances.nearest_targets[5] == 4);
    REQUIRE(distances.get_shortest_path(0) == std::vector<size_t>{0, 1, 2});
    REQUIRE(distances.get_shortest_path(3).empty());
}
