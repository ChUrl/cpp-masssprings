#include "eight_puzzle_codec.hpp"
#include "eight_puzzle_model.hpp"
#include "eight_puzzle_presenter.hpp"
#include "graph_distances.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>

TEST_CASE("8-puzzle explores the full reachable component", "[eight_puzzle]")
{
    eight_puzzle_model model;
    const state_id solved = model.solved_state();

    const exploration_result result = model.explore(solved, solved, std::nullopt);

    REQUIRE(model.state_count() == 181440); // 9! / 2
    REQUIRE(result.complete);
    REQUIRE(!result.directed);
    REQUIRE(result.start == solved);
    REQUIRE(result.current == solved);
    REQUIRE(!result.links.empty());

    for (const auto& [from, to] : result.links) {
        REQUIRE(from < to);
        REQUIRE(to < model.state_count());
    }
}

TEST_CASE("8-puzzle hard preset has a 31-move solution", "[eight_puzzle]")
{
    eight_puzzle_model model;
    eight_puzzle_codec codec;

    const std::optional<state_id> start = codec.parse("P8:[3x3] B:[8672543_1]", model);
    REQUIRE(start);
    REQUIRE(model.is_valid(*start));
    REQUIRE(model.is_solvable(*start));
    REQUIRE(!model.is_goal(*start));

    const exploration_result result = model.explore(*start, *start, std::nullopt);
    REQUIRE(model.state_count() == 181440);

    const state_id goal = model.solved_state();
    graph_distances distances;
    distances.calculate_distances(model.state_count(), result.links, {goal});
    REQUIRE(distances.distances[*start] == 31);
}

TEST_CASE("8-puzzle tile moves are adjacent and reversible", "[eight_puzzle]")
{
    eight_puzzle_model model;
    const state_id solved = model.solved_state();

    REQUIRE(model.blank_cell(solved) == 8);
    REQUIRE(model.value(solved, 8) == 0);
    REQUIRE(eight_puzzle_presenter(model).status_text(model, solved).empty());
    REQUIRE(!model.try_move_tile(solved, 0)); // not adjacent to the blank

    const std::optional<state_id> moved = model.try_move_tile(solved, 7);
    REQUIRE(moved);
    REQUIRE(model.blank_cell(*moved) == 7);
    REQUIRE(model.value(*moved, 8) == 8);

    const std::optional<state_id> back = model.try_move_tile(*moved, 8);
    REQUIRE(back);
    REQUIRE(model.code(*back) == model.code(solved));

    REQUIRE(!model.try_move_blank(solved, 0, 1)); // blank cannot leave the board
    const std::optional<state_id> blank_left = model.try_move_blank(solved, -1, 0);
    REQUIRE(blank_left);
    REQUIRE(model.blank_cell(*blank_left) == 7);
}

TEST_CASE("8-puzzle codec round-trips and rejects invalid states", "[eight_puzzle]")
{
    eight_puzzle_model model;
    eight_puzzle_codec codec;

    const std::optional<state_id> solved = codec.parse("P8:[3x3] B:[12345678_]", model);
    REQUIRE(solved);
    REQUIRE(model.is_goal(*solved));
    REQUIRE(codec.format(model, *solved) == "P8:[3x3] B:[12345678_]");
    REQUIRE(codec.validate(model, *solved) == std::nullopt);

    const std::optional<state_id> solved_zero = codec.parse("P8:[3x3] B:[123456780]", model);
    REQUIRE(solved_zero);
    REQUIRE(model.is_goal(*solved_zero));

    REQUIRE(!codec.parse("P8:[3x3] B:[112345678]", model)); // duplicate tile
    REQUIRE(!codec.parse("P8:[3x3] B:[12345678]", model));  // wrong length
    REQUIRE(!codec.parse("P8:[3x3] B:[21345678_]", model)); // odd permutation
    REQUIRE(!codec.parse("P8:[4x4] B:[12345678_]", model)); // wrong size
}

TEST_CASE("8-puzzle random states are valid and solvable", "[eight_puzzle]")
{
    eight_puzzle_model model;

    const state_id random = model.random_state();
    REQUIRE(model.is_valid(random));
    REQUIRE(model.is_solvable(random));
    REQUIRE(!model.is_goal(random));
}
