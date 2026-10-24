#include "search_controller.hpp"
#include "cpu_layout_engine.hpp"
#include "puzzle_registry.hpp"
#include "puzzle_space.hpp"
#include "sliding_block_model.hpp"
#include "state_manager.hpp"

#include <catch2/catch_test_macros.hpp>

#include "memory_tracker.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

namespace
{
auto wait_for_finish(search_controller& search) -> void
{
    for (int attempt = 0; attempt < 2000 && search.running(); ++attempt) {
        search.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    search.poll();
}
} // namespace

TEST_CASE("state-space jobs return an explored model to the main thread", "[search]")
{
    sliding_block_model model;
    const state_id start = model.initial_state();

    search_controller search;
    REQUIRE(search.start_state_space(model.clone(), start, start, "sliding_block", 42, 2));
    REQUIRE(search.running());
    REQUIRE(search.kind() == search_kind::state_space);

    wait_for_finish(search);

    std::optional<state_space_job_result> result = search.take_state_space_result();
    REQUIRE(result);
    REQUIRE(result->generation == 42);
    REQUIRE(result->type_id == "sliding_block");
    REQUIRE(!result->exploration.cancelled);
    REQUIRE(result->model);
    REQUIRE(result->model->state_count() >= 1);
    REQUIRE(search.take_state_space_result() == std::nullopt);
}

TEST_CASE("state_manager installs a completed background state space", "[search]")
{
    cpu_layout_engine physics(std::nullopt, nullptr);
    puzzle_registry registry;
    register_builtin_puzzles(registry);

    const std::filesystem::path preset_file =
        std::filesystem::temp_directory_path() / "masssprings_search_controller_test.puzzle";
    std::filesystem::remove(preset_file);

    state_manager state(physics, registry, preset_file.string());
    REQUIRE(state.start_state_space_search());

    for (int attempt = 0; attempt < 2000 && state.search_is_running(); ++attempt) {
        state.poll_search();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    state.poll_search();

    REQUIRE(!state.search_is_running());
    REQUIRE(state.get_state_count() >= 1);
    REQUIRE(state.get_current_index() == state.get_starting_index());
    const memory_usage usage = state.memory_usage();
    REQUIRE(usage.model > 0);
    REQUIRE(usage.graph > 0);
    REQUIRE(state.memory_bytes() == usage.total());
}

TEST_CASE("preset cycling stays inside the active puzzle type", "[search]")
{
    cpu_layout_engine physics(std::nullopt, nullptr);
    puzzle_registry registry;
    register_builtin_puzzles(registry);

    const std::filesystem::path preset_file =
        std::filesystem::temp_directory_path() / "masssprings_preset_cycle_test.puzzle";
    std::filesystem::remove(preset_file);
    {
        std::ofstream file(preset_file);
        file << "# First\n"
                "S:[3x3] G:[0,0] M:[F] B:[{1X1 1x1 _} {_ _ _} {_ _ _}]\n"
                "# Second\n"
                "S:[3x3] G:[0,0] M:[F] B:[{1X1 _ 1x1} {_ _ _} {_ _ _}]\n"
                "@type lights_out\n"
                "# Lights\n"
                "LO:[4x4] B:[1111111111111111]\n";
    }

    state_manager state(physics, registry, preset_file.string());
    REQUIRE(state.current_puzzle_type() == "sliding_block");
    REQUIRE(state.get_type_preset_count() == 2);
    REQUIRE(state.get_type_preset_position() == 0);

    state.load_next_preset();
    CHECK(state.current_puzzle_type() == "sliding_block");
    CHECK(state.get_type_preset_position() == 1);
    CHECK(state.get_current_preset_comment() == "# Second");

    // Wraps back to the first sliding-block preset instead of entering lights_out.
    state.load_next_preset();
    CHECK(state.current_puzzle_type() == "sliding_block");
    CHECK(state.get_type_preset_position() == 0);
    CHECK(state.get_current_preset_comment() == "# First");

    // Wraps backwards to the last preset of the type.
    state.load_previous_preset();
    CHECK(state.current_puzzle_type() == "sliding_block");
    CHECK(state.get_type_preset_position() == 1);

    // Switching the type (the puzzle spinner) starts at that type's first preset.
    REQUIRE(state.switch_puzzle("lights_out"));
    CHECK(state.current_puzzle_type() == "lights_out");
    CHECK(state.get_type_preset_count() == 1);
    CHECK(state.get_type_preset_position() == 0);

    state.load_next_preset();
    CHECK(state.current_puzzle_type() == "lights_out");
    CHECK(state.get_type_preset_position() == 0);

    std::filesystem::remove(preset_file);
}

TEST_CASE("state_manager stores and loads puzzle-space results", "[search]")
{
    cpu_layout_engine physics(std::nullopt, nullptr);
    puzzle_registry registry;
    register_builtin_puzzles(registry);

    const std::filesystem::path preset_file =
        std::filesystem::temp_directory_path() / "masssprings_puzzle_space_test.puzzle";
    {
        std::ofstream file(preset_file);
        file << "@type sliding_block\n"
                "# Test\n"
                "S:[3x3] G:[0,0] M:[F] B:[{1X1 1x1 _} {_ _ _} {_ _ _}]\n";
    }

    state_manager state(physics, registry, preset_file.string());
    REQUIRE(state.start_puzzle_space_search(2, 0));
    REQUIRE(state.get_search_origin_comment() == "# Test");
    REQUIRE(!state.search_origin_is_custom());

    for (int attempt = 0; attempt < 5000 && state.search_is_running(); ++attempt) {
        state.poll_search();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    state.poll_search();

    REQUIRE(!state.search_is_running());
    const puzzle_space_result* results = state.get_puzzle_space_results();
    REQUIRE(results != nullptr);
    REQUIRE(!results->clusters.empty());

    // The hardest result is installed automatically when the result is handed to the main thread.
    REQUIRE(state.has_custom_state());
    REQUIRE(state.current_puzzle_type() == "sliding_block");
    REQUIRE(state.get_custom_label() == "# Result 1");
    REQUIRE(state.get_state_count() == 1);

    if (results->clusters.size() > 1) {
        state.request_load_puzzle_space_result(1);
        state.poll_search();
        REQUIRE(state.get_puzzle_space_selection() == 1);
        REQUIRE(state.has_custom_state());
        REQUIRE(state.get_custom_label() == "# Result 2");
        REQUIRE(state.get_state_count() == 1);
    }

    state.set_puzzle_space_sort(puzzle_space_sort::most_blocks_fewest_states_most_moves);
    REQUIRE(state.get_puzzle_space_sort() ==
            puzzle_space_sort::most_blocks_fewest_states_most_moves);
    REQUIRE(state.get_puzzle_space_selection() == 0);
    state.poll_search();
    REQUIRE(state.get_custom_label() == "# Result 1");
    REQUIRE(state.get_state_count() == 1);

    state.save_current_to_preset_file("Saved result");
    REQUIRE(!state.has_custom_state());

    std::filesystem::remove(preset_file);
}

TEST_CASE("puzzle-space search runs without a memory confirmation", "[search]")
{
    const puzzle empty(3, 3, 0, 0, false, true);
    auto board = empty.try_add_block(block(1, 1, 1, 1, true, false));
    REQUIRE(board);
    board = board->try_add_block(block(0, 0, 1, 1, false, false));
    REQUIRE(board);

    const puzzle_space_config_result built = make_puzzle_space_config(*board, 0, 0);
    REQUIRE(built.config);

    search_controller search;
    REQUIRE(search.start_puzzle_space(*built.config, 1));
    REQUIRE(search.running());

    // The search never pauses: it either runs or has already finished.
    CHECK(search.snapshot().status == search_status::running);

    wait_for_finish(search);

    std::optional<puzzle_space_result> result = search.take_puzzle_space_result();
    REQUIRE(result);
    REQUIRE(!result->cancelled);
}

TEST_CASE("PerfDbg memory report", "[.memory]")
{
    cpu_layout_engine physics(std::nullopt, nullptr);
    puzzle_registry registry;
    register_builtin_puzzles(registry);

    state_manager state(physics, registry, "default.puzzle");
    state.load_preset(15);
    REQUIRE(state.start_state_space_search());

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(3);
    while (state.search_is_running() && std::chrono::steady_clock::now() < deadline) {
        state.poll_search();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    state.poll_search();
    REQUIRE_FALSE(state.search_is_running());

    const memory_usage usage = state.memory_usage();
    const memory_tracker::snapshot tracked = memory_tracker::get_snapshot();
    std::printf("PERFDBG states=%zu sizeof(puzzle)=%zu model=%zu graph=%zu physics=%zu "
                "total=%zu tracked_live=%zu tracked_peak=%zu\n",
                state.get_state_count(), sizeof(puzzle), usage.model, usage.graph,
                usage.physics_cpu + usage.physics_octree + usage.physics_readback, usage.total(),
                tracked.live_bytes, tracked.peak_bytes);
}

TEST_CASE("puzzle-space jobs return results to the main thread", "[search]")
{
    const puzzle empty(3, 3, 0, 0, false, true);
    auto board = empty.try_add_block(block(1, 1, 1, 1, true, false));
    REQUIRE(board);
    board = board->try_add_block(block(0, 0, 1, 1, false, false));
    REQUIRE(board);

    const puzzle_space_config_result built = make_puzzle_space_config(*board, 0, 0);
    REQUIRE(built.config);

    search_controller search;
    REQUIRE(search.start_puzzle_space(*built.config, 2));
    REQUIRE(search.running());
    REQUIRE(search.kind() == search_kind::puzzle_space);

    wait_for_finish(search);

    std::optional<puzzle_space_result> result = search.take_puzzle_space_result();
    REQUIRE(result);
    REQUIRE(!result->cancelled);
    REQUIRE(!result->clusters.empty());
    REQUIRE(search.take_puzzle_space_result() == std::nullopt);
}
