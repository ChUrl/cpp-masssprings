#include "graph_distances.hpp"
#include "hanoi_codec.hpp"
#include "hanoi_model.hpp"
#include "hanoi_presenter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>

TEST_CASE("10-disk Towers of Hanoi explores all states", "[hanoi]")
{
    hanoi_model model;
    const state_id start = model.initial_state();

    const exploration_result result = model.explore(start, start, std::nullopt);

    REQUIRE(model.state_count() == 59049); // 3^10
    REQUIRE(result.complete);
    REQUIRE(!result.directed);
    REQUIRE(result.start == start);
    REQUIRE(result.current == start);
    REQUIRE(!result.links.empty());

    for (const auto& [from, to] : result.links) {
        REQUIRE(from < to);
        REQUIRE(to < model.state_count());
    }

    const state_id solved = model.solved_state();
    REQUIRE(model.is_goal(solved));

    graph_distances distances;
    distances.calculate_distances(model.state_count(), result.links, {solved});
    REQUIRE(distances.distances[start] == 1023); // 2^10 - 1
}

TEST_CASE("3-disk Towers of Hanoi has the expected optimal path", "[hanoi]")
{
    hanoi_model model(3);
    const state_id start = model.initial_state();
    const exploration_result result = model.explore(start, start, std::nullopt);

    REQUIRE(model.state_count() == 27); // 3^3

    const state_id solved = model.solved_state();
    graph_distances distances;
    distances.calculate_distances(model.state_count(), result.links, {solved});
    REQUIRE(distances.distances[start] == 7); // 2^3 - 1
}

TEST_CASE("Towers of Hanoi moves respect disk sizes and are reversible", "[hanoi]")
{
    hanoi_model model;
    const state_id start = model.initial_state();

    REQUIRE(model.top_disk(start, 0) == 0);
    REQUIRE(model.top_disk(start, 1) == -1);

    const state_id solved = model.solved_state();
    REQUIRE(model.disks_on_target(start) == 0);
    REQUIRE(model.disks_on_target(solved) == model.disk_count());

    hanoi_presenter presenter(model);
    REQUIRE(presenter.status_text(model, start) == "0/10 Disks");
    REQUIRE(presenter.status_text(model, solved) == "10/10 Disks");

    REQUIRE(!model.try_move(start, 1, 0)); // no disk on the middle peg

    const std::optional<state_id> disk0_to_peg1 = model.try_move(start, 0, 1);
    REQUIRE(disk0_to_peg1);
    REQUIRE(model.peg(*disk0_to_peg1, 0) == 1);

    const std::optional<state_id> disk1_to_peg2 = model.try_move(*disk0_to_peg1, 0, 2);
    REQUIRE(disk1_to_peg2);
    REQUIRE(model.peg(*disk1_to_peg2, 1) == 2);

    // Disk 1 cannot be placed on top of the smaller disk 0.
    REQUIRE(!model.try_move(*disk1_to_peg2, 2, 1));

    // Moving the top disk back is always legal.
    const std::optional<state_id> back = model.try_move(*disk0_to_peg1, 1, 0);
    REQUIRE(back);
    REQUIRE(model.code(*back) == model.code(start));
}

TEST_CASE("Towers of Hanoi codec round-trips", "[hanoi]")
{
    hanoi_model model;
    hanoi_codec codec;

    const std::optional<state_id> start = codec.parse("H:[10] T:[2] B:[0000000000]", model);
    REQUIRE(start);
    REQUIRE(model.is_valid(*start));
    REQUIRE(!model.is_goal(*start));
    REQUIRE(codec.format(model, *start) == "H:[10] T:[2] B:[0000000000]");
    REQUIRE(codec.validate(model, *start) == std::nullopt);

    const std::optional<state_id> solved = codec.parse("H:[10] T:[2] B:[2222222222]", model);
    REQUIRE(solved);
    REQUIRE(model.is_goal(*solved));
    REQUIRE(codec.format(model, *solved) == "H:[10] T:[2] B:[2222222222]");

    REQUIRE(!codec.parse("H:[10] T:[2] B:[012]", model));
    REQUIRE(!codec.parse("H:[10] T:[3] B:[0000000000]", model));
    REQUIRE(!codec.parse("H:[0] T:[2] B:[]", model));
}

TEST_CASE("Towers of Hanoi reset keeps only one state", "[hanoi]")
{
    hanoi_model model;
    const state_id start = model.initial_state();
    const std::uint32_t start_code = model.code(start);
    const state_id solved = model.solved_state();

    REQUIRE(model.state_count() == 2);

    const state_id reset = model.reset_to(solved);
    REQUIRE(model.state_count() == 1);
    REQUIRE(reset == 0);
    REQUIRE(model.is_goal(reset));
    REQUIRE(model.code(reset) != start_code);
}
