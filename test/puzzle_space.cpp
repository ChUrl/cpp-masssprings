#include "puzzle_space.hpp"
#include "puzzle_space_io.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <stop_token>
#include <string>

namespace
{
// Breadth-first distance from a state to the nearest goal state, or -1 if none is
// reachable within `limit` levels. Used to pin the cluster's hardest state.
auto distance_to_goal(const puzzle& start, const int limit) -> int
{
    if (start.goal_reached()) {
        return 0;
    }
    puzzleset seen;
    seen.insert(start);
    std::vector<puzzle> frontier{start};
    for (int distance = 1; distance <= limit && !frontier.empty(); ++distance) {
        std::vector<puzzle> next;
        for (const puzzle& state : frontier) {
            state.for_each_adjacent(
                [&](const puzzle& neighbour)
                {
                    if (seen.insert(neighbour).second) {
                        next.push_back(neighbour);
                    }
                });
        }
        for (const puzzle& state : next) {
            if (state.goal_reached()) {
                return distance;
            }
        }
        frontier.swap(next);
    }
    return -1;
}

auto make_small_board() -> puzzle
{
    const puzzle empty(3, 3, 0, 0, false, true);
    auto board = empty.try_add_block(block(1, 1, 1, 1, true, false));
    REQUIRE(board);
    board = board->try_add_block(block(0, 0, 1, 1, false, false));
    REQUIRE(board);
    return *board;
}
} // namespace

TEST_CASE("puzzle-space files round-trip clusters and configuration", "[puzzle_space]")
{
    const puzzle board = make_small_board();
    const puzzle_space_config_result built = make_puzzle_space_config(board, 2, 3);
    REQUIRE(built.config);
    const puzzle_space_config& config = *built.config;

    puzzle_space_result result;
    result.config = config;
    result.clusters.push_back(puzzle_space_cluster{
        board,
        board,
        7,
        12,
        board.block_count(),
    });

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "masssprings_puzzle_space_io_test.puzzles";
    std::filesystem::remove(path);
    REQUIRE(!save_puzzle_space_file(path.string(), result));

    const puzzle_space_io_result loaded = load_puzzle_space_file(path.string());
    REQUIRE(loaded.result);
    REQUIRE(loaded.error.empty());
    REQUIRE(loaded.result->config.width == config.width);
    REQUIRE(loaded.result->config.height == config.height);
    REQUIRE(loaded.result->config.restricted == config.restricted);
    REQUIRE(loaded.result->config.min_blocks == config.min_blocks);
    REQUIRE(loaded.result->config.inventory.size() == config.inventory.size());
    REQUIRE(loaded.result->clusters.size() == 1);
    REQUIRE(loaded.result->clusters[0].max_moves == 7);
    REQUIRE(loaded.result->clusters[0].state_count == 12);
    REQUIRE(loaded.result->clusters[0].block_count == board.block_count());
    REQUIRE(loaded.result->clusters[0].cluster_id == board);
    REQUIRE(loaded.result->clusters[0].hardest == board);

    std::filesystem::remove(path);
}

TEST_CASE("legacy wall tokens parse into the separate wall mask", "[puzzle]")
{
    const puzzle p(
        std::string("S:[4x4] G:[3,2] M:[F] B:[{1X1 1x1 _ _} {2*1 _ _ _} {_ _ _ _} {_ _ _ _}]"));
    CHECK(p.block_count() == 2);
    CHECK(p.wall_count() == 2);
    CHECK(p.wall_at(0, 1));
    CHECK(p.wall_at(1, 1));
    CHECK_FALSE(p.wall_at(2, 1));

    // The preset output uses 1*1 wall cells, which is semantically equivalent.
    const std::string repr = p.string_repr();
    CHECK(repr.find("1*1") != std::string::npos);
    CHECK(puzzle(repr) == p);
}

TEST_CASE("puzzle-space files without a WALLS line still load", "[puzzle_space]")
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "masssprings_puzzle_space_legacy_test.puzzles";
    {
        std::ofstream file(path);
        file << "MSPACE 1\n"
                "CONFIG 3 3 0 0 1 2 0\n"
                "TARGET 1 1\n"
                "RANGE 0 0 2 2\n"
                "INVENTORY 1\n"
                "1 1\n"
                "CLUSTERS 0\n";
    }

    const puzzle_space_io_result loaded = load_puzzle_space_file(path.string());
    std::filesystem::remove(path);

    REQUIRE(loaded.result);
    CHECK(loaded.result->config.walls == 0);
    CHECK(loaded.result->config.min_blocks == 2);
    CHECK(loaded.result->clusters.empty());
}

TEST_CASE("puzzle-space files round-trip wall inventories", "[puzzle_space]")
{
    const puzzle board(std::string("S:[4x4] G:[3,3] M:[F] B:[{1X1 1x1 _ _} {_ _ _ _} {1*1 _ _ _} "
                                   "{_ _ _ _}]"));
    const wall_search_options options{true, 0, 1};
    const puzzle_space_config_result built = make_puzzle_space_config(board, 3, 0, 2, options);
    REQUIRE(built.config);

    puzzle_space_result result;
    result.config = *built.config;
    result.clusters.push_back(puzzle_space_cluster{
        board,
        board,
        2,
        3,
        board.block_count(),
        1,
    });

    const std::filesystem::path path = std::filesystem::temp_directory_path() /
        "masssprings_puzzle_space_wall_inventory_test.puzzles";
    std::filesystem::remove(path);
    REQUIRE_FALSE(save_puzzle_space_file(path.string(), result));

    const puzzle_space_io_result loaded = load_puzzle_space_file(path.string());
    std::filesystem::remove(path);

    REQUIRE(loaded.result);
    CHECK(loaded.result->config.wall_inventory_enabled);
    CHECK(loaded.result->config.wall_inventory.size() == 1);
    CHECK(loaded.result->config.min_walls == 0);
    CHECK(loaded.result->config.max_walls == 1);
    REQUIRE(loaded.result->clusters.size() == 1);
    CHECK(loaded.result->clusters[0].wall_count == 1);
    CHECK(loaded.result->clusters[0].cluster_id == board);
}

TEST_CASE("puzzle-space files round-trip fixed walls", "[puzzle_space]")
{
    const std::optional<puzzle> walled = make_small_board().try_add_wall(2, 2);
    REQUIRE(walled);
    const puzzle_space_config_result built = make_puzzle_space_config(*walled, 2, 0);
    REQUIRE(built.config);
    REQUIRE(built.config->walls != 0);

    puzzle_space_result result;
    result.config = *built.config;
    result.clusters.push_back(puzzle_space_cluster{
        *walled,
        *walled,
        3,
        5,
        walled->block_count(),
    });

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "masssprings_puzzle_space_walls_test.puzzles";
    std::filesystem::remove(path);
    REQUIRE_FALSE(save_puzzle_space_file(path.string(), result));

    const puzzle_space_io_result loaded = load_puzzle_space_file(path.string());
    std::filesystem::remove(path);

    REQUIRE(loaded.result);
    CHECK(loaded.result->config.walls == built.config->walls);
    REQUIRE(loaded.result->clusters.size() == 1);
    CHECK(loaded.result->clusters[0].cluster_id == *walled);
    CHECK(loaded.result->clusters[0].cluster_id.wall_count() == 1);
    CHECK(loaded.result->clusters[0].hardest.wall_count() == 1);
}

TEST_CASE("puzzle-space result sorting supports both orders", "[puzzle_space]")
{
    const puzzle id(3, 3, 0, 0, false, true);
    std::vector<puzzle_space_cluster> clusters = {
        {id, id, 10, 100, 2},
        {id, id, 5, 50, 3},
        {id, id, 20, 30, 3},
        {id, id, 50, 10, 2},
    };

    sort_puzzle_space_clusters(clusters, puzzle_space_sort::most_moves);
    REQUIRE(clusters[0].max_moves == 50);
    REQUIRE(clusters[1].max_moves == 20);
    REQUIRE(clusters[2].max_moves == 10);
    REQUIRE(clusters[3].max_moves == 5);

    sort_puzzle_space_clusters(clusters, puzzle_space_sort::most_blocks_fewest_states_most_moves);
    REQUIRE(clusters[0].block_count == 3);
    REQUIRE(clusters[0].state_count == 30);
    REQUIRE(clusters[0].max_moves == 20);
    REQUIRE(clusters[1].block_count == 3);
    REQUIRE(clusters[1].state_count == 50);
    REQUIRE(clusters[2].block_count == 2);
    REQUIRE(clusters[2].state_count == 10);
    REQUIRE(clusters[3].block_count == 2);
    REQUIRE(clusters[3].state_count == 100);
}

TEST_CASE("search display helpers format counts and durations", "[puzzle_space]")
{
    REQUIRE(format_count(0) == "0");
    REQUIRE(format_count(999) == "999");
    REQUIRE(format_count(1000) == "1,000");
    REQUIRE(format_count(1234567) == "1,234,567");

    REQUIRE(format_duration(-1.0) == "--");
    REQUIRE(format_duration(42.0) == "42s");
    REQUIRE(format_duration(192.0) == "3m 12s");
    REQUIRE(format_duration(3720.0) == "1h 02m");

    REQUIRE(format_gb(2ull * 1024ull * 1024ull * 1024ull) == "2.00 GB");
    REQUIRE(format_memory(512) == "512 B");
    REQUIRE(format_memory(2048) == "2 KB");
    REQUIRE(format_memory(3ull * 1024ull * 1024ull) == "3.0 MB");
    REQUIRE(format_memory(2ull * 1024ull * 1024ull * 1024ull) == "2.00 GB");
}

TEST_CASE("puzzle-space config is derived from the current board", "[puzzle_space]")
{
    const puzzle board = make_small_board();
    const puzzle_space_config_result built = make_puzzle_space_config(board, 0, 7);

    REQUIRE(built.config);
    const puzzle_space_config& config = *built.config;
    REQUIRE(config.width == 3);
    REQUIRE(config.height == 3);
    REQUIRE(config.goal_x == 0);
    REQUIRE(config.goal_y == 0);
    REQUIRE(!config.restricted);
    REQUIRE(config.target.get_width() == 1);
    REQUIRE(config.target.get_height() == 1);
    REQUIRE(config.inventory.size() == 1);
    REQUIRE(config.max_blocks == 2);
    REQUIRE(config.min_blocks == 2);
    REQUIRE(config.min_moves == 7);
    REQUIRE(std::get<0>(config.target_positions) == 0);
    REQUIRE(std::get<1>(config.target_positions) == 0);
    REQUIRE(std::get<2>(config.target_positions) == 2);
    REQUIRE(std::get<3>(config.target_positions) == 2);
}

TEST_CASE("puzzle-space target placement follows the movement mode", "[puzzle_space]")
{
    const puzzle rush_hour_empty(6, 6, 4, 2, true, true);
    auto rush_hour = rush_hour_empty.try_add_block(block(0, 2, 2, 1, true, false));
    REQUIRE(rush_hour);
    rush_hour = rush_hour->try_add_block(block(0, 0, 1, 1, false, false));
    REQUIRE(rush_hour);

    const puzzle_space_config_result rush_config = make_puzzle_space_config(*rush_hour, 0, 10);
    REQUIRE(rush_config.config);
    REQUIRE(std::get<0>(rush_config.config->target_positions) == 0);
    REQUIRE(std::get<1>(rush_config.config->target_positions) == 2);
    REQUIRE(std::get<2>(rush_config.config->target_positions) == 4);
    REQUIRE(std::get<3>(rush_config.config->target_positions) == 2);

    const puzzle klotski_empty(4, 5, 1, 3, false, true);
    auto klotski = klotski_empty.try_add_block(block(1, 3, 2, 2, true, false));
    REQUIRE(klotski);
    klotski = klotski->try_add_block(block(0, 0, 1, 1, false, false));
    REQUIRE(klotski);

    const puzzle_space_config_result klotski_config = make_puzzle_space_config(*klotski, 0, 10);
    REQUIRE(klotski_config.config);
    REQUIRE(std::get<0>(klotski_config.config->target_positions) == 0);
    REQUIRE(std::get<1>(klotski_config.config->target_positions) == 0);
    REQUIRE(std::get<2>(klotski_config.config->target_positions) == 2);
    REQUIRE(std::get<3>(klotski_config.config->target_positions) == 3);
}

TEST_CASE("puzzle-space config honors and validates the minimum block count", "[puzzle_space]")
{
    const puzzle board = make_small_board();

    const puzzle_space_config_result wide = make_puzzle_space_config(board, 2, 0, 2);
    REQUIRE(wide.config);
    CHECK(wide.config->min_blocks == 2);

    const puzzle_space_config_result too_high = make_puzzle_space_config(board, 2, 0, 3);
    REQUIRE(!too_high.config);
    CHECK(too_high.error.find("Minimum blocks") != std::string::npos);
}

TEST_CASE("puzzle-space config rejects a board without a target", "[puzzle_space]")
{
    const puzzle empty(3, 3, 0, 0, false, true);
    const puzzle_space_config_result built = make_puzzle_space_config(empty, 3, 0);
    REQUIRE(!built.config);
    REQUIRE(!built.error.empty());
}

TEST_CASE("puzzle-space search reports progress and hardest states", "[puzzle_space]")
{
    const puzzle board = make_small_board();
    const puzzle_space_config_result built = make_puzzle_space_config(board, 0, 0);
    REQUIRE(built.config);

    puzzle_space_progress progress;
    const puzzle_space_result result =
        run_puzzle_space_search(*built.config, &progress, std::stop_token{}, std::nullopt);

    REQUIRE(!result.cancelled);
    REQUIRE(result.error.empty());
    REQUIRE(!result.clusters.empty());
    REQUIRE(progress.candidates_total.load() > 0);
    REQUIRE(progress.candidates_done.load() == progress.candidates_total.load());
    REQUIRE(progress.phase.load() == puzzle_space_phase::done);
    REQUIRE(progress.probe_candidates.load() > 0);
    REQUIRE(progress.probe_unique.load() > 0);
    REQUIRE(progress.unique_candidates.load() + progress.duplicate_candidates.load() ==
            progress.candidates_done.load());
    REQUIRE(progress.cache_states.load() > 0);
    REQUIRE(progress.projected_peak_bytes.load() > 0);

    for (const puzzle_space_cluster& cluster : result.clusters) {
        REQUIRE(cluster.max_moves >= 0);
        REQUIRE(cluster.state_count > 0);
        REQUIRE(cluster.hardest.valid());
        // The reported hardest state really is max_moves away from the goal, i.e.
        // it comes from the last level of the component.
        CHECK(distance_to_goal(cluster.hardest, cluster.max_moves) == cluster.max_moves);
    }

    for (size_t i = 1; i < result.clusters.size(); ++i) {
        REQUIRE(result.clusters[i - 1].max_moves >= result.clusters[i].max_moves);
    }
}

TEST_CASE("puzzle-space search can be cancelled before it starts", "[puzzle_space]")
{
    const puzzle board = make_small_board();
    const puzzle_space_config_result built = make_puzzle_space_config(board, 0, 0);
    REQUIRE(built.config);

    std::stop_source source;
    source.request_stop();

    puzzle_space_progress progress;
    const puzzle_space_result result =
        run_puzzle_space_search(*built.config, &progress, source.get_token(), std::nullopt);

    REQUIRE(result.cancelled);
    REQUIRE(progress.phase.load() == puzzle_space_phase::cancelled);
}

TEST_CASE("cluster exploration reports its hardest state", "[puzzle_space]")
{
    const puzzle board = make_small_board();
    const cluster_exploration exploration = board.get_cluster_id_and_solution();

    REQUIRE(!exploration.cancelled);
    REQUIRE(exploration.winnable);
    REQUIRE(exploration.max_moves >= 0);
    REQUIRE(!exploration.states.empty());
    REQUIRE(std::ranges::find(exploration.states, exploration.hardest) != exploration.states.end());
}

TEST_CASE("puzzle-space stage slots pack the step and its permille progress", "[puzzle_space]")
{
    const u64 slot = pack_stage_slot(puzzle_space_stage::indexing, 427);
    CHECK(stage_slot_stage(slot) == puzzle_space_stage::indexing);
    CHECK(stage_slot_permille(slot) == 427);

    // Progress is clamped to the 0..1000 permille range.
    CHECK(stage_slot_permille(pack_stage_slot(puzzle_space_stage::aggregating, 5'000)) == 1'000);
    CHECK(stage_slot_permille(pack_stage_slot(puzzle_space_stage::aggregating, 0)) == 0);

    // A zero slot means "no worker is in a step".
    CHECK(stage_slot_stage(0) == puzzle_space_stage::none);
}
