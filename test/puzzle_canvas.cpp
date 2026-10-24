#include "puzzle_presenter.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("puzzle canvas cell hit-testing includes internal block padding", "[puzzle_canvas]")
{
    puzzle_canvas canvas;
    canvas.bounds = Rectangle{0.0f, 0.0f, 100.0f, 100.0f};
    canvas.columns = 2;
    canvas.rows = 2;
    canvas.padding = 10;

    const auto center = canvas.cell_at(Vector2{27.0f, 27.0f});
    REQUIRE(center);
    CHECK(center->first == 0);
    CHECK(center->second == 0);

    const auto other = canvas.cell_at(Vector2{70.0f, 70.0f});
    REQUIRE(other);
    CHECK(other->first == 1);
    CHECK(other->second == 1);

    // Internal gaps belong to the nearest/previous cell. This is what makes clicking the
    // middle of a 2x1 or 2x2 block select that block.
    const auto horizontal_gap = canvas.cell_at(Vector2{50.0f, 27.0f});
    REQUIRE(horizontal_gap);
    CHECK(horizontal_gap->first == 0);
    CHECK(horizontal_gap->second == 0);

    const auto center_gap = canvas.cell_at(Vector2{50.0f, 50.0f});
    REQUIRE(center_gap);
    CHECK(center_gap->first == 0);
    CHECK(center_gap->second == 0);

    // The outer margin is not part of the board.
    CHECK_FALSE(canvas.cell_at(Vector2{5.0f, 27.0f}));
    CHECK_FALSE(canvas.cell_at(Vector2{27.0f, 5.0f}));
    CHECK_FALSE(canvas.cell_at(Vector2{95.0f, 27.0f}));
    CHECK_FALSE(canvas.cell_at(Vector2{27.0f, 95.0f}));
}
