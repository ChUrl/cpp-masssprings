#include "puzzle.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("explore_state_space_with_indices keeps indices consistent", "[explore]")
{
    const puzzle empty(3, 3, 0, 0, false, false);
    const auto start = empty.try_add_block(block(1, 1, 1, 1));
    REQUIRE(start);

    const state_space_result result = start->explore_state_space_with_indices();

    // A 1x1 block on a 3x3 board can reach all 9 positions.
    REQUIRE(result.states.size() == 9);
    REQUIRE(result.indices.size() == 9);
    for (size_t i = 0; i < result.states.size(); ++i) {
        REQUIRE(result.indices.at(result.states[i]) == i);
    }

    // The 3x3 grid has 12 undirected edges, stored once from lower to higher index.
    REQUIRE(result.links.size() == 12);
    for (const auto& [from, to] : result.links) {
        REQUIRE(from < to);
        REQUIRE(to < result.states.size());
    }

    const auto [states, links] = start->explore_state_space();
    REQUIRE(states.size() == result.states.size());
    REQUIRE(links.size() == result.links.size());
}

TEST_CASE("state-space exploration respects fixed walls", "[explore]")
{
    const puzzle empty(3, 3, 0, 0, false, false);
    const std::optional<puzzle> with_block = empty.try_add_block(block(0, 0, 1, 1));
    REQUIRE(with_block);
    const std::optional<puzzle> walled = with_block->try_add_wall(1, 1);
    REQUIRE(walled);

    const state_space_result result = walled->explore_state_space_with_indices();

    // The ring around the center wall contains the other eight cells.
    REQUIRE(result.states.size() == 8);
    for (const puzzle& state : result.states) {
        CHECK(state.wall_mask() == walled->wall_mask());
        CHECK(state.wall_at(1, 1));
        CHECK_FALSE(state.try_get_block(1, 1).has_value());
        CHECK_FALSE(state.try_get_invalid_reason().has_value());
    }
}

TEST_CASE("state-space graph contains every adjacent edge", "[explore]")
{
    const puzzle empty(4, 4, 0, 0, false, false);
    auto p = empty.try_add_block(block(0, 0, 2, 1));
    REQUIRE(p);
    p = p->try_add_block(block(0, 2, 1, 2));
    REQUIRE(p);

    const state_space_result result = p->explore_state_space_with_indices();
    boost::unordered_flat_set<u64> edges;
    for (const auto& [a, b] : result.links) {
        const size_t first = std::min(a, b);
        const size_t second = std::max(a, b);
        CHECK(edges.insert((u64{first} << 32) | static_cast<u64>(second)).second);
    }

    for (size_t i = 0; i < result.states.size(); ++i) {
        result.states[i].for_each_adjacent(
            [&](const puzzle& neighbor)
            {
                const auto it = result.indices.find(neighbor);
                REQUIRE(it != result.indices.end());
                const size_t first = std::min(i, it->second);
                const size_t second = std::max(i, it->second);
                CHECK(edges.contains((u64{first} << 32) | static_cast<u64>(second)));
            });
    }
}

TEST_CASE("parallel exploration matches sequential exploration", "[explore]")
{
    const puzzle empty(5, 5, 0, 0, false, false);
    auto p = empty.try_add_block(block(0, 0, 1, 1));
    REQUIRE(p);
    p = p->try_add_block(block(1, 0, 1, 1));
    REQUIRE(p);
    p = p->try_add_block(block(2, 0, 1, 1));
    REQUIRE(p);
    p = p->try_add_block(block(3, 0, 1, 1));
    REQUIRE(p);

    const state_space_result sequential = p->explore_state_space_with_indices();

    BS::thread_pool<> pool(4);
    const state_space_result parallel = p->explore_state_space_with_indices(threadpool{&pool});

    REQUIRE(parallel.states.size() == sequential.states.size());
    REQUIRE(parallel.links == sequential.links);
    for (size_t i = 0; i < sequential.states.size(); ++i) {
        REQUIRE(parallel.states[i] == sequential.states[i]);
    }
    for (size_t i = 0; i < parallel.states.size(); ++i) {
        REQUIRE(parallel.indices.at(parallel.states[i]) == i);
    }
}
