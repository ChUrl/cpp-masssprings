#include "graph_distances.hpp"
#include "sudoku4_codec.hpp"
#include "sudoku4_model.hpp"
#include "sudoku4_presenter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace
{
constexpr std::string_view GIVENS = "1_____3__2_____4";

auto configure_givens(sudoku4_model& model) -> state_id
{
    std::uint16_t mask = 0;
    std::uint64_t code = 0;
    for (int cell = 0; cell < sudoku4_model::CELLS; ++cell) {
        const char digit = GIVENS[static_cast<size_t>(cell)];
        if (digit == '_') {
            continue;
        }
        const int value = digit - '0';
        mask |= static_cast<std::uint16_t>(1u << cell);
        code |= static_cast<std::uint64_t>(value) << (4u * static_cast<unsigned int>(cell));
    }

    model.configure(mask, code);
    return model.initial_state();
}
} // namespace

TEST_CASE("4x4 Sudoku explores the full partial-state component", "[sudoku4]")
{
    sudoku4_model model;
    const state_id start = configure_givens(model);

    REQUIRE(model.state_count() == 1);
    REQUIRE(model.is_valid(start));
    REQUIRE(!model.is_goal(start));
    REQUIRE(model.empty_count(start) == 12);
    REQUIRE(sudoku4_presenter(model).status_text(model, start) == "4/16 Digits");

    const exploration_result result = model.explore(start, start, std::nullopt);

    REQUIRE(model.state_count() == 10898);
    REQUIRE(result.complete);
    REQUIRE(!result.directed);
    REQUIRE(result.start == start);
    REQUIRE(result.current == start);
    REQUIRE(!result.links.empty());

    for (const auto& [from, to] : result.links) {
        REQUIRE(from < to);
        REQUIRE(to < model.state_count());
    }

    size_t goal_count = 0;
    state_id goal = 0;
    for (state_id id = 0; id < model.state_count(); ++id) {
        if (model.is_goal(id)) {
            ++goal_count;
            goal = id;
        }
    }
    REQUIRE(goal_count == 1);

    graph_distances distances;
    distances.calculate_distances(model.state_count(), result.links, {goal});
    REQUIRE(distances.distances[start] == 12); // 16 cells - 4 givens
}

TEST_CASE("4x4 Sudoku givens are fixed and fill/clear moves are reversible", "[sudoku4]")
{
    sudoku4_model model;
    const state_id start = configure_givens(model);

    REQUIRE(model.is_given(0));
    REQUIRE(model.given_value(0) == 1);
    REQUIRE(!model.is_given(1));
    REQUIRE(!model.try_set(start, 0, 2)); // cannot change a given
    REQUIRE(!model.try_set(start, 1, 1)); // conflicts with the given 1 in row 0

    const std::optional<state_id> placed = model.try_set(start, 1, 3);
    REQUIRE(placed);
    REQUIRE(model.value(*placed, 1) == 3);
    REQUIRE(model.empty_count(*placed) == 11);

    const std::optional<state_id> cleared = model.try_set(*placed, 1, 0);
    REQUIRE(cleared);
    REQUIRE(model.code(*cleared) == model.code(start));

    REQUIRE(!model.try_set(start, 1, 0)); // already empty
}

TEST_CASE("4x4 Sudoku codec round-trips and rejects invalid states", "[sudoku4]")
{
    sudoku4_model model;
    sudoku4_codec codec;

    const std::optional<state_id> start =
        codec.parse("S4:[4x4] G:[1_____3__2_____4] B:[1_____3__2_____4]", model);
    REQUIRE(start);
    REQUIRE(model.state_count() == 1);
    REQUIRE(model.is_valid(*start));
    REQUIRE(codec.format(model, *start) == "S4:[4x4] G:[1_____3__2_____4] B:[1_____3__2_____4]");
    REQUIRE(codec.validate(model, *start) == std::nullopt);

    const std::optional<state_id> solved =
        codec.parse("S4:[4x4] G:[1_____3__2_____4] B:[1342243142133124]", model);
    REQUIRE(solved);
    REQUIRE(model.is_goal(*solved));
    REQUIRE(sudoku4_presenter(model).status_text(model, *solved) == "16/16 Digits");
    REQUIRE(codec.format(model, *solved) == "S4:[4x4] G:[1_____3__2_____4] B:[1342243142133124]");

    REQUIRE(!codec.parse("S4:[4x4] G:[1_____3__2_____4] B:[11______________]", model));
    REQUIRE(!codec.parse("S4:[4x4] G:[1_____3__2_____4] B:[2_____3__2_____4]", model));
    REQUIRE(!codec.parse("S4:[4x4] G:[1_______________] B:[1_______________]", model));
    REQUIRE(!codec.parse("S4:[5x5] G:[1_____3__2_____4] B:[1_____3__2_____4]", model));
}

TEST_CASE("4x4 Sudoku reset keeps only the requested state", "[sudoku4]")
{
    sudoku4_model model;
    const state_id start = configure_givens(model);
    const std::optional<state_id> placed = model.try_set(start, 1, 3);
    REQUIRE(placed);
    const std::uint64_t placed_code = model.code(*placed);

    const state_id reset = model.reset_to(*placed);
    REQUIRE(model.state_count() == 1);
    REQUIRE(reset == 0);
    REQUIRE(model.code(reset) == placed_code);
    REQUIRE(model.is_valid(reset));
}
