#include "puzzle_context.hpp"
#include "puzzle_registry.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("puzzle registry can switch between kinds at runtime", "[context]")
{
    puzzle_registry registry;
    register_builtin_puzzles(registry);

    REQUIRE(registry.ids().size() >= 2);

    puzzle_context context;
    REQUIRE(!context.valid());

    std::unique_ptr<puzzle_kind> lights = registry.create("lights_out");
    REQUIRE(lights);
    context.replace(std::move(lights));

    REQUIRE(context.valid());
    REQUIRE(context.type_id() == "lights_out");
    REQUIRE(context.generation() == 1);

    const std::optional<state_id> solved =
        context.codec().parse("LO:[4x4] B:[0000000000000000]", context.model());
    REQUIRE(solved);
    REQUIRE(context.model().state_count() == 1);
    REQUIRE(context.model().is_goal(*solved));
    REQUIRE(context.model().describe(*solved) == "LO:[4x4] B:[0000000000000000]");

    std::unique_ptr<puzzle_kind> sliding = registry.create("sliding_block");
    REQUIRE(sliding);
    context.replace(std::move(sliding));

    REQUIRE(context.type_id() == "sliding_block");
    REQUIRE(context.generation() == 2);

    const std::optional<state_id> empty =
        context.codec().parse("S:[3x3] M:[R] B:[{_ _ _} {_ _ _} {_ _ _}]", context.model());
    REQUIRE(empty);
    REQUIRE(context.model().state_count() == 1);
    REQUIRE(context.codec().validate(context.model(), *empty) == std::nullopt);

    std::unique_ptr<puzzle_kind> hanoi = registry.create("hanoi");
    REQUIRE(hanoi);
    context.replace(std::move(hanoi));

    REQUIRE(context.type_id() == "hanoi");
    REQUIRE(context.generation() == 3);

    const std::optional<state_id> hanoi_start =
        context.codec().parse("H:[10] T:[2] B:[0000000000]", context.model());
    REQUIRE(hanoi_start);
    REQUIRE(context.model().state_count() == 1);
    REQUIRE(!context.model().is_goal(*hanoi_start));
    REQUIRE(context.model().describe(*hanoi_start) == "H:[10] T:[2] B:[0000000000]");

    std::unique_ptr<puzzle_kind> eight = registry.create("eight_puzzle");
    REQUIRE(eight);
    context.replace(std::move(eight));

    REQUIRE(context.type_id() == "eight_puzzle");
    REQUIRE(context.generation() == 4);

    const std::optional<state_id> eight_solved =
        context.codec().parse("P8:[3x3] B:[12345678_]", context.model());
    REQUIRE(eight_solved);
    REQUIRE(context.model().state_count() == 1);
    REQUIRE(context.model().is_goal(*eight_solved));
    REQUIRE(context.model().describe(*eight_solved) == "P8:[3x3] B:[12345678_]");

    std::unique_ptr<puzzle_kind> sudoku = registry.create("sudoku4");
    REQUIRE(sudoku);
    context.replace(std::move(sudoku));

    REQUIRE(context.type_id() == "sudoku4");
    REQUIRE(context.generation() == 5);

    const std::optional<state_id> sudoku_start = context.codec().parse(
        "S4:[4x4] G:[1_____3__2_____4] B:[1_____3__2_____4]", context.model());
    REQUIRE(sudoku_start);
    REQUIRE(context.model().state_count() == 1);
    REQUIRE(!context.model().is_goal(*sudoku_start));
    REQUIRE(context.model().describe(*sudoku_start) ==
            "S4:[4x4] G:[1_____3__2_____4] B:[1_____3__2_____4]");
}
