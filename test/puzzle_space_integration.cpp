// Integration tests for puzzle-space exploration.
//
// These tests are written against the *observable contract* of
// run_puzzle_space_search(), not against its internals, so they can be run after
// every stage of the rewrite. The reference implementation below is completely
// independent of the search pipeline: it enumerates goal-containing placements
// and explores components with puzzle::for_each_adjacent() and graph_distances.
//
// How to run:
//   ./cmake-build-release/tests "[puzzle_space][integration]"
//   ./cmake-build-release/tests "[puzzle_space][integration][.slow]"   (heavy cases)
//   ./cmake-build-release/tests "[.semantics]"                         (new invariants)
//
// Stage expectations:
//   - Stage 0 (current algorithm): all [integration] tests pass, except hidden
//     [.semantics] tests that encode intentional behaviour changes.
//   - Stage 1+ (new pipeline): all [integration] tests pass, including the
//     [.slow] cases, and the hidden [.semantics] test must pass as well.

#include "graph_distances.hpp"
#include "puzzle_space.hpp"
#include "puzzle_space_io.hpp"
#include "search_controller.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
// ---------------------------------------------------------------------------
// Reference implementation used as an oracle
// ---------------------------------------------------------------------------

struct reference_cluster
{
    puzzle id;
    size_t state_count = 0;
    size_t block_count = 0;
    int max_moves = 0;
    std::vector<puzzle> states;
    std::vector<puzzle> farthest;
};

auto cluster_less(const int max_moves_a, const size_t states_a, const puzzle& id_a,
                  const int max_moves_b, const size_t states_b, const puzzle& id_b) -> bool
{
    if (max_moves_a != max_moves_b) {
        return max_moves_a > max_moves_b;
    }
    if (states_a != states_b) {
        return states_a > states_b;
    }
    return id_a < id_b;
}

auto reference_less(const reference_cluster& a, const reference_cluster& b) -> bool
{
    return cluster_less(a.max_moves, a.state_count, a.id, b.max_moves, b.state_count, b.id);
}

/**
 * Enumerates every placement that has the target on the goal cell and between
 * one and max_blocks-1 other blocks. Every winnable component contains at least
 * one such state, so exploring from these states covers exactly the winnable
 * part of the puzzle space.
 *
 * The enumeration is canonical (first empty cell: place or skip), so each
 * placement is produced exactly once. It is deliberately a separate
 * implementation from the production search.
 */
auto enumerate_goal_states(const puzzle_space_config& config) -> std::vector<puzzle>
{
    std::vector<puzzle> result;
    if (config.max_blocks < 2) {
        return result;
    }

    std::vector<block> shapes;
    shapes.reserve(config.inventory.size());
    for (const block b : config.inventory) {
        shapes.push_back(b);
    }
    std::ranges::sort(shapes,
                      [](const block a, const block b)
                      {
                          return std::tuple(a.get_width(), a.get_height()) <
                              std::tuple(b.get_width(), b.get_height());
                      });

    const block goal_target(config.goal_x, config.goal_y, config.target.get_width(),
                            config.target.get_height(), true, false);
    const puzzle empty(config.width, config.height, config.goal_x, config.goal_y, config.restricted,
                       true, config.walls);
    const std::optional<puzzle> base = empty.try_add_block(goal_target);
    REQUIRE(base.has_value());

    const int width = config.width;
    const int height = config.height;
    const int max_other = static_cast<int>(config.max_blocks) - 1;
    const int min_other = std::max(1, static_cast<int>(std::max<size_t>(1, config.min_blocks)) - 1);

    const auto dfs = [&](auto&& self, const puzzle& current, const u64 occupied, const int pos,
                         const int placed) -> void
    {
        if (placed >= min_other) {
            result.push_back(current);
        }
        if (placed >= max_other) {
            return;
        }

        int cell = pos;
        while (cell < width * height && ((occupied >> cell) & 1u) != 0) {
            ++cell;
        }
        if (cell >= width * height) {
            return;
        }

        const int x = cell % width;
        const int y = cell / width;
        for (const block shape : shapes) {
            if (x + shape.get_width() > width || y + shape.get_height() > height) {
                continue;
            }
            const block candidate(x, y, shape.get_width(), shape.get_height(), false, false);
            const std::optional<puzzle> next = current.try_add_block(candidate);
            if (!next) {
                continue;
            }
            self(self, *next, next->blocks_bitmap(), cell + 1, placed + 1);
        }

        // Leave this cell empty and continue with the next one.
        self(self, current, occupied, cell + 1, placed);
    };

    dfs(dfs, *base, base->blocks_bitmap(), 0, 0);
    return result;
}

/**
 * Explores the full connected component that contains the seed using only the
 * production state rules and graph_distances. The result contains the component
 * minimum state, its size and its exact eccentricity from the goal states.
 */
auto explore_component(const puzzle& seed) -> reference_cluster
{
    reference_cluster result;
    puzzlemap<size_t> indices;
    std::vector<spring> links;

    indices.emplace(seed, 0);
    result.states.push_back(seed);

    // states grows while it is traversed; the loop index is the BFS queue.
    for (size_t i = 0; i < result.states.size(); ++i) {
        const puzzle current = result.states[i];
        current.for_each_adjacent(
            [&](const puzzle& next)
            {
                auto [it, inserted] = indices.emplace(next, result.states.size());
                if (inserted) {
                    result.states.push_back(next);
                }
                if (i < it->second) {
                    links.emplace_back(i, it->second);
                }
            });
    }

    result.state_count = result.states.size();
    result.block_count = result.states.front().block_count();
    result.id = *std::ranges::min_element(result.states);

    std::vector<size_t> goals;
    for (size_t i = 0; i < result.states.size(); ++i) {
        if (result.states[i].goal_reached()) {
            goals.push_back(i);
        }
    }
    REQUIRE(!goals.empty());

    graph_distances distances;
    distances.calculate_distances(result.states.size(), links, goals);

    result.max_moves = 0;
    for (const int distance : distances.distances) {
        result.max_moves = std::max(result.max_moves, distance);
    }
    for (size_t i = 0; i < result.states.size(); ++i) {
        if (distances.distances[i] == result.max_moves) {
            result.farthest.push_back(result.states[i]);
        }
    }

    return result;
}

/**
 * Returns all winnable components whose max_moves is at least min_moves,
 * sorted exactly like the production result.
 */
auto reference_clusters(const puzzle_space_config& config) -> std::vector<reference_cluster>
{
    std::vector<reference_cluster> result;
    puzzleset assigned;

    for (const puzzle& goal_state : enumerate_goal_states(config)) {
        if (assigned.contains(goal_state)) {
            continue;
        }

        reference_cluster component = explore_component(goal_state);
        for (const puzzle& state : component.states) {
            assigned.insert(state);
        }

        // States without any non-target block are not puzzles; the reference
        // ignores them, and the production search should ignore them too.
        if (component.block_count < 2) {
            continue;
        }
        if (component.max_moves >= config.min_moves) {
            result.push_back(std::move(component));
        }
    }

    std::ranges::sort(result, reference_less);
    return result;
}

// ---------------------------------------------------------------------------
// Helpers for running and comparing searches
// ---------------------------------------------------------------------------

auto run_search(const puzzle_space_config& config, const int threads = 1,
                puzzle_space_progress* progress = nullptr, const size_t batch_state_budget = 0)
    -> puzzle_space_result
{
    std::optional<BS::thread_pool<>> pool;
    threadpool tp = std::nullopt;
    if (threads > 1) {
        pool.emplace(static_cast<unsigned int>(threads));
        tp = &*pool;
    }
    return run_puzzle_space_search(config, progress, std::stop_token{}, tp, batch_state_budget);
}

auto result_clusters_without_degenerate(const puzzle_space_result& result)
    -> std::vector<const puzzle_space_cluster*>
{
    std::vector<const puzzle_space_cluster*> clusters;
    for (const puzzle_space_cluster& cluster : result.clusters) {
        if (cluster.block_count >= 2) {
            clusters.push_back(&cluster);
        }
    }
    return clusters;
}

/**
 * Compares the production result with the reference. Each cluster must match
 * the reference component exactly (id, size, hardness and block count), and the
 * hardest state must be one of the reference states at maximum distance.
 */
auto check_matches_reference(const puzzle_space_result& result,
                             const std::vector<reference_cluster>& expected) -> void
{
    const std::vector<const puzzle_space_cluster*> actual =
        result_clusters_without_degenerate(result);

    INFO("result: " << result.clusters.size() << " clusters, " << actual.size()
                    << " non-degenerate; reference: " << expected.size());
    REQUIRE(actual.size() == expected.size());

    for (size_t i = 0; i < actual.size(); ++i) {
        INFO("cluster " << i << " id=" << actual[i]->cluster_id.string_repr());
        CHECK(actual[i]->cluster_id == expected[i].id);
        CHECK(actual[i]->state_count == expected[i].state_count);
        CHECK(actual[i]->max_moves == expected[i].max_moves);
        CHECK(actual[i]->block_count == expected[i].block_count);

        CHECK_FALSE(actual[i]->cluster_id.try_get_invalid_reason().has_value());
        CHECK_FALSE(actual[i]->hardest.try_get_invalid_reason().has_value());
        CHECK(actual[i]->block_count == actual[i]->cluster_id.block_count());

        CHECK(std::ranges::find(expected[i].states, actual[i]->hardest) !=
              expected[i].states.end());
        CHECK(std::ranges::find(expected[i].farthest, actual[i]->hardest) !=
              expected[i].farthest.end());
    }

    // Cluster ids must be unique and ordered by the production sort.
    for (size_t i = 1; i < actual.size(); ++i) {
        CHECK(actual[i - 1]->cluster_id != actual[i]->cluster_id);
        CHECK(cluster_less(actual[i - 1]->max_moves, actual[i - 1]->state_count,
                           actual[i - 1]->cluster_id, actual[i]->max_moves, actual[i]->state_count,
                           actual[i]->cluster_id));
    }
}

auto require_valid_run(const puzzle_space_result& result) -> void
{
    CHECK_FALSE(result.cancelled);
    CHECK(result.error.empty());
    for (const puzzle_space_cluster& cluster : result.clusters) {
        CHECK(cluster.state_count > 0);
        CHECK(cluster.max_moves >= 0);
        CHECK(cluster.block_count >= 1);
        CHECK_FALSE(cluster.cluster_id.try_get_invalid_reason().has_value());
        CHECK_FALSE(cluster.hardest.try_get_invalid_reason().has_value());
    }
}

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

auto three_by_three_board() -> puzzle
{
    return puzzle(std::string("S:[3x3] G:[0,0] M:[F] B:[{1X1 1x1 _} {_ _ _} {_ _ _}]"));
}

auto klotski_board() -> puzzle
{
    return puzzle(std::string("S:[4x5] G:[1,3] M:[F] B:[{1x2 2X2 _ 1x2} {_ _ _ _} {1x2 2x1 _ "
                              "1x2} {_ 1x1 1x1 _} {1x1 _ _ 1x1}]"));
}

auto walled_board() -> puzzle
{
    return puzzle(std::string("S:[5x5] G:[4,2] M:[F] B:[{1X1 1x1 _ _ _} {_ 1*1 _ _ _} "
                              "{_ _ 1*1 _ _} {_ _ _ 1*1 _} {_ _ _ _ _}]"));
}

auto rush_hour_board() -> puzzle
{
    return puzzle(
        std::string("S:[6x6] G:[4,2] M:[R] B:[{3x1 _ _ _ _ 1x3} {_ _ _ _ _ _} {_ _ 1x2 2X1 _ _} "
                    "{_ _ _ 1x2 2x1 _} {1x2 _ 1x2 _ 2x1 _} {_ _ _ 3x1 _ _}]"));
}

auto make_config(const puzzle& board, const size_t max_blocks, const int min_moves,
                 const size_t min_blocks = 2, const wall_search_options& walls = {})
    -> puzzle_space_config
{
    const puzzle_space_config_result built =
        make_puzzle_space_config(board, max_blocks, min_moves, min_blocks, walls);
    REQUIRE(built.config.has_value());
    return *built.config;
}
} // namespace

// ---------------------------------------------------------------------------
// Equivalence with the independent reference
// ---------------------------------------------------------------------------

TEST_CASE("puzzle-space matches the reference on a 3x3 board", "[puzzle_space][integration]")
{
    const puzzle board = three_by_three_board();

    for (const size_t max_blocks : {2u, 3u}) {
        for (const int min_moves : {0, 1}) {
            for (const size_t min_blocks : {2u, 3u}) {
                if (min_blocks > max_blocks) {
                    continue;
                }
                INFO("max_blocks=" << max_blocks << " min_blocks=" << min_blocks
                                   << " min_moves=" << min_moves);
                const puzzle_space_config config =
                    make_config(board, max_blocks, min_moves, min_blocks);
                const puzzle_space_result result = run_search(config);
                require_valid_run(result);
                check_matches_reference(result, reference_clusters(config));
            }
        }
    }
}

TEST_CASE("puzzle-space search supports a wall inventory", "[puzzle_space][integration]")
{
    const puzzle board(std::string("S:[4x4] G:[3,3] M:[F] B:[{1X1 1x1 _ _} {_ _ _ _} {1*1 _ _ _} "
                                   "{_ _ _ _}]"));
    const wall_search_options options{true, 0, 1};
    const puzzle_space_config config = make_config(board, 3, 0, 2, options);
    const puzzle_space_result result = run_search(config);
    require_valid_run(result);
    for (const puzzle_space_cluster& cluster : result.clusters) {
        CHECK(cluster.wall_count <= 1);
        CHECK(cluster.cluster_id.wall_mask() == cluster.hardest.wall_mask());
    }
}

TEST_CASE("puzzle-space matches the reference on a walled board", "[puzzle_space][integration]")
{
    const puzzle board = walled_board();

    for (const size_t max_blocks : {3u, 4u}) {
        INFO("max_blocks=" << max_blocks);
        const puzzle_space_config config = make_config(board, max_blocks, 0);
        REQUIRE(config.walls != 0);
        const puzzle_space_result result = run_search(config);
        require_valid_run(result);
        check_matches_reference(result, reference_clusters(config));
    }
}

TEST_CASE("puzzle-space matches the reference on the 4x5 Klotski board",
          "[puzzle_space][integration]")
{
    const puzzle board = klotski_board();

    for (const size_t max_blocks : {4u, 5u}) {
        for (const int min_moves : {0, 2}) {
            INFO("max_blocks=" << max_blocks << " min_moves=" << min_moves);
            const puzzle_space_config config = make_config(board, max_blocks, min_moves);
            const puzzle_space_result result = run_search(config);
            require_valid_run(result);
            check_matches_reference(result, reference_clusters(config));
        }
    }
}

TEST_CASE("puzzle-space matches the reference on 6x6 Rush Hour (3 blocks)",
          "[puzzle_space][integration]")
{
    const puzzle board = rush_hour_board();

    for (const int min_moves : {0, 2, 4}) {
        INFO("min_moves=" << min_moves);
        const puzzle_space_config config = make_config(board, 3, min_moves);
        const puzzle_space_result result = run_search(config);
        require_valid_run(result);
        check_matches_reference(result, reference_clusters(config));
    }
}

TEST_CASE("puzzle-space matches the reference on 6x6 Rush Hour (4 blocks)",
          "[puzzle_space][integration][.slow]")
{
    const puzzle board = rush_hour_board();
    const puzzle_space_config config = make_config(board, 4, 2);
    const puzzle_space_result result = run_search(config);
    require_valid_run(result);
    check_matches_reference(result, reference_clusters(config));
}

// ---------------------------------------------------------------------------
// Filtering and edge cases
// ---------------------------------------------------------------------------

TEST_CASE("puzzle-space min_moves only removes clusters, never changes survivors",
          "[puzzle_space][integration]")
{
    const puzzle board = rush_hour_board();
    const puzzle_space_config unfiltered = make_config(board, 3, 0);
    const puzzle_space_config filtered = make_config(board, 3, 3);

    const puzzle_space_result all = run_search(unfiltered);
    const puzzle_space_result some = run_search(filtered);
    require_valid_run(all);
    require_valid_run(some);

    std::vector<const puzzle_space_cluster*> expected;
    for (const puzzle_space_cluster& cluster : all.clusters) {
        if (cluster.block_count >= 2 && cluster.max_moves >= filtered.min_moves) {
            expected.push_back(&cluster);
        }
    }

    const std::vector<const puzzle_space_cluster*> actual =
        result_clusters_without_degenerate(some);
    REQUIRE(actual.size() == expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        INFO("cluster " << i);
        CHECK(actual[i]->cluster_id == expected[i]->cluster_id);
        CHECK(actual[i]->state_count == expected[i]->state_count);
        CHECK(actual[i]->max_moves == expected[i]->max_moves);
        CHECK(actual[i]->block_count == expected[i]->block_count);
    }
}

TEST_CASE("puzzle-space returns an empty result when min_moves filters everything",
          "[puzzle_space][integration]")
{
    const puzzle_space_config config = make_config(three_by_three_board(), 3, 1'000);

    puzzle_space_progress progress;
    const puzzle_space_result result = run_search(config, 1, &progress);

    CHECK_FALSE(result.cancelled);
    CHECK(result.error.empty());
    CHECK(result.clusters.empty());
    CHECK(progress.phase.load() == puzzle_space_phase::done);
    CHECK(progress.candidates_total.load() > 0);
    CHECK(progress.candidates_done.load() == progress.candidates_total.load());
}

TEST_CASE("puzzle-space with only a target produces no clusters", "[puzzle_space][integration]")
{
    const puzzle_space_config config = make_config(three_by_three_board(), 1, 0);
    const puzzle_space_result result = run_search(config);

    CHECK_FALSE(result.cancelled);
    CHECK(result.error.empty());
    CHECK(result.clusters.empty());
}

// ---------------------------------------------------------------------------
// Determinism and concurrency
// ---------------------------------------------------------------------------

TEST_CASE("puzzle-space results do not depend on the thread count", "[puzzle_space][integration]")
{
    const puzzle_space_config config = make_config(rush_hour_board(), 3, 0);

    const puzzle_space_result sequential = run_search(config, 1);
    const puzzle_space_result parallel_a = run_search(config, 4);
    const puzzle_space_result parallel_b = run_search(config, 4);

    require_valid_run(sequential);
    require_valid_run(parallel_a);
    require_valid_run(parallel_b);

    const auto compare = [](const puzzle_space_result& a, const puzzle_space_result& b)
    {
        const auto a_clusters = result_clusters_without_degenerate(a);
        const auto b_clusters = result_clusters_without_degenerate(b);
        REQUIRE(a_clusters.size() == b_clusters.size());
        for (size_t i = 0; i < a_clusters.size(); ++i) {
            INFO("cluster " << i);
            CHECK(a_clusters[i]->cluster_id == b_clusters[i]->cluster_id);
            CHECK(a_clusters[i]->state_count == b_clusters[i]->state_count);
            CHECK(a_clusters[i]->max_moves == b_clusters[i]->max_moves);
            CHECK(a_clusters[i]->block_count == b_clusters[i]->block_count);
        }
    };

    compare(sequential, parallel_a);
    compare(parallel_a, parallel_b);
}

TEST_CASE("puzzle-space parallel partition path matches the serial path",
          "[puzzle_space][integration][.slow]")
{
    const puzzle_space_config config = make_config(rush_hour_board(), 4, 2);

    const puzzle_space_result serial = run_search(config, 1);
    // Force a small batch budget so the 411k-state partition is a
    // single-partition batch and exercises the intra-partition parallel path.
    const puzzle_space_result parallel = run_search(config, 4, nullptr, 100'000);
    require_valid_run(serial);
    require_valid_run(parallel);

    const auto serial_clusters = result_clusters_without_degenerate(serial);
    const auto parallel_clusters = result_clusters_without_degenerate(parallel);
    REQUIRE(serial_clusters.size() == parallel_clusters.size());
    for (size_t i = 0; i < serial_clusters.size(); ++i) {
        INFO("cluster " << i);
        CHECK(serial_clusters[i]->cluster_id == parallel_clusters[i]->cluster_id);
        CHECK(serial_clusters[i]->state_count == parallel_clusters[i]->state_count);
        CHECK(serial_clusters[i]->max_moves == parallel_clusters[i]->max_moves);
        CHECK(serial_clusters[i]->block_count == parallel_clusters[i]->block_count);
    }
}

TEST_CASE("puzzle-space can be cancelled during exploration", "[puzzle_space][integration][.slow]")
{
    const puzzle_space_config config = make_config(rush_hour_board(), 4, 0);

    std::stop_source stop;
    puzzle_space_progress progress;
    std::optional<puzzle_space_result> result;

    std::thread worker(
        [&]
        {
            result.emplace(
                run_puzzle_space_search(config, &progress, stop.get_token(), std::nullopt));
        });

    // Wait until exploration actually started (or the search finished early).
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline &&
           progress.phase.load() != puzzle_space_phase::exploring &&
           progress.phase.load() != puzzle_space_phase::done &&
           progress.phase.load() != puzzle_space_phase::cancelled) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const bool was_running = progress.phase.load() == puzzle_space_phase::exploring;
    stop.request_stop();
    worker.join();

    REQUIRE(result.has_value());
    if (was_running) {
        CHECK(result->cancelled);
        CHECK(progress.phase.load() == puzzle_space_phase::cancelled);
    } else {
        // The search was too fast to cancel; the important part is that it
        // terminated cleanly.
        CHECK(result->error.empty());
    }
}

TEST_CASE("puzzle-space cancel does not wait for the current partition",
          "[puzzle_space][integration][.slow]")
{
    const puzzle_space_config config = make_config(rush_hour_board(), 4, 0);

    std::optional<BS::thread_pool<>> pool;
    pool.emplace(4u);
    const threadpool threads = &*pool;

    std::stop_source stop;
    puzzle_space_progress progress;
    std::optional<puzzle_space_result> result;

    std::thread worker(
        [&]
        { result.emplace(run_puzzle_space_search(config, &progress, stop.get_token(), threads)); });

    // Wait until the exploration phase is really running; the cancellation below
    // must not have to wait for whatever partition is in flight (index build,
    // aggregation, ...).
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (std::chrono::steady_clock::now() < deadline &&
           progress.phase.load() != puzzle_space_phase::exploring) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const bool was_running = progress.phase.load() == puzzle_space_phase::exploring;
    const auto cancel_start = std::chrono::steady_clock::now();
    stop.request_stop();
    worker.join();
    const double cancel_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - cancel_start).count();

    REQUIRE(result.has_value());
    if (was_running) {
        CHECK(result->cancelled);
        CHECK(cancel_seconds < 2.0);
    }
}

// ---------------------------------------------------------------------------
// Progress, IO and the asynchronous controller
// ---------------------------------------------------------------------------

TEST_CASE("puzzle-space progress reaches done with consistent counters",
          "[puzzle_space][integration]")
{
    const puzzle_space_config config = make_config(rush_hour_board(), 3, 0);

    puzzle_space_progress progress;
    const puzzle_space_result result = run_search(config, 1, &progress);

    require_valid_run(result);
    CHECK(progress.phase.load() == puzzle_space_phase::done);
    CHECK(progress.candidates_total.load() > 0);
    CHECK(progress.candidates_done.load() == progress.candidates_total.load());
    CHECK(progress.states_explored.load() > 0);
    CHECK(progress.clusters_found.load() >= result.clusters.size());
    CHECK(progress.estimated_peak_bytes.load() > 0);
    CHECK(progress.projected_peak_bytes.load() > 0);
    CHECK(progress.eta_seconds.load() >= 0);

    // Every step scope must have released its worker slot.
    for (const std::atomic<u64>& slot : progress.stage_slots) {
        CHECK(slot.load(std::memory_order_relaxed) == 0);
    }
    for (const std::atomic<size_t>& states : progress.stage_slot_states) {
        CHECK(states.load(std::memory_order_relaxed) == 0);
    }
}

TEST_CASE("puzzle-space progress advances while a partition is running",
          "[puzzle_space][integration][.slow]")
{
    const puzzle_space_config config = make_config(rush_hour_board(), 4, 2);
    puzzle_space_progress progress;

    std::atomic<bool> sampling{true};
    std::set<size_t> observed;
    std::set<size_t> observed_memory;
    std::thread sampler(
        [&]
        {
            while (sampling.load(std::memory_order_relaxed)) {
                observed.insert(progress.candidates_done.load(std::memory_order_relaxed));
                observed_memory.insert(progress.cache_bytes.load(std::memory_order_relaxed));
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

    const puzzle_space_result result = run_search(config, 1, &progress);
    sampling.store(false, std::memory_order_relaxed);
    sampler.join();

    require_valid_run(result);
    CHECK(progress.phase.load() == puzzle_space_phase::done);
    CHECK(progress.candidates_done.load() == progress.candidates_total.load());

    // The search must publish progress many times while enumerating, unioning
    // and running the distance BFS inside the largest partition, not only once
    // per partition.
    CHECK(observed.size() > 4);
    // The live memory estimate must follow the partitions actually in flight.
    CHECK(observed_memory.size() > 1);
    CHECK(progress.live_states.load() == 0);
    CHECK(progress.cache_bytes.load() == 0);
}

TEST_CASE("puzzle-space result survives a save and load round trip", "[puzzle_space][integration]")
{
    const puzzle_space_config config = make_config(klotski_board(), 4, 0);
    const puzzle_space_result result = run_search(config);
    require_valid_run(result);
    REQUIRE(!result.clusters.empty());

    const std::filesystem::path path = std::filesystem::temp_directory_path() /
        "masssprings_puzzle_space_integration_roundtrip.puzzles";
    std::filesystem::remove(path);

    REQUIRE_FALSE(save_puzzle_space_file(path.string(), result).has_value());

    const puzzle_space_io_result loaded = load_puzzle_space_file(path.string());
    std::filesystem::remove(path);

    REQUIRE(loaded.result.has_value());
    CHECK(loaded.error.empty());
    CHECK(loaded.result->config.width == config.width);
    CHECK(loaded.result->config.height == config.height);
    CHECK(loaded.result->config.goal_x == config.goal_x);
    CHECK(loaded.result->config.goal_y == config.goal_y);
    CHECK(loaded.result->config.restricted == config.restricted);
    CHECK(loaded.result->config.max_blocks == config.max_blocks);
    CHECK(loaded.result->config.min_blocks == config.min_blocks);
    CHECK(loaded.result->config.min_moves == config.min_moves);
    CHECK(loaded.result->clusters.size() == result.clusters.size());

    for (size_t i = 0; i < result.clusters.size(); ++i) {
        INFO("cluster " << i);
        CHECK(loaded.result->clusters[i].cluster_id == result.clusters[i].cluster_id);
        CHECK(loaded.result->clusters[i].hardest == result.clusters[i].hardest);
        CHECK(loaded.result->clusters[i].max_moves == result.clusters[i].max_moves);
        CHECK(loaded.result->clusters[i].state_count == result.clusters[i].state_count);
        CHECK(loaded.result->clusters[i].block_count == result.clusters[i].block_count);
    }
}

TEST_CASE("search controller returns the same puzzle-space result as a direct call",
          "[puzzle_space][integration]")
{
    const puzzle_space_config config = make_config(rush_hour_board(), 3, 0);

    const puzzle_space_result direct = run_search(config, 1);
    require_valid_run(direct);

    search_controller controller;
    REQUIRE(controller.start_puzzle_space(config, 1));

    for (int attempt = 0; attempt < 10'000 && controller.running(); ++attempt) {
        controller.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    controller.poll();
    REQUIRE_FALSE(controller.running());

    // Idle searches must not leave any worker step slots behind.
    const search_snapshot finished = controller.snapshot();
    CHECK(finished.stage == puzzle_space_stage::none);
    CHECK(finished.stage_workers == 0);

    std::optional<puzzle_space_result> controlled = controller.take_puzzle_space_result();
    REQUIRE(controlled.has_value());
    require_valid_run(*controlled);

    const auto direct_clusters = result_clusters_without_degenerate(direct);
    const auto controlled_clusters = result_clusters_without_degenerate(*controlled);
    REQUIRE(direct_clusters.size() == controlled_clusters.size());
    for (size_t i = 0; i < direct_clusters.size(); ++i) {
        INFO("cluster " << i);
        CHECK(direct_clusters[i]->cluster_id == controlled_clusters[i]->cluster_id);
        CHECK(direct_clusters[i]->state_count == controlled_clusters[i]->state_count);
        CHECK(direct_clusters[i]->max_moves == controlled_clusters[i]->max_moves);
        CHECK(direct_clusters[i]->block_count == controlled_clusters[i]->block_count);
    }
}

// ---------------------------------------------------------------------------
// Intentional behaviour changes of the new pipeline
// ---------------------------------------------------------------------------

TEST_CASE("puzzle-space never returns clusters without a non-target block", "[.semantics]")
{
    // The legacy implementation emits the target-only chain because the
    // "skip every cell" branch of place_block_set() also fires for every
    // superset. That component is not a puzzle and must be excluded by the new
    // pipeline.
    const puzzle_space_config config = make_config(rush_hour_board(), 2, 0);
    const puzzle_space_result result = run_search(config);

    for (const puzzle_space_cluster& cluster : result.clusters) {
        INFO(cluster.cluster_id.string_repr());
        CHECK(cluster.block_count >= 2);
    }
}
