#ifndef PUZZLE_SPACE_HPP_
#define PUZZLE_SPACE_HPP_

#include "puzzle.hpp"
#include "state_model.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <tuple>
#include <vector>

enum class puzzle_space_phase
{
    idle,
    inventories,
    starting_positions,
    counting,
    probing,
    exploring,
    finishing,
    done,
    cancelled,
    failed,
};

/**
 * Long single steps inside the exploration phase. They are reported separately
 * from the phase so the UI can keep moving while a worker is busy with a step
 * that does not advance the placement counters.
 */
enum class puzzle_space_stage : u32
{
    none = 0,
    indexing = 1,
    aggregating = 2,
};

// One progress slot per search worker. Each worker owns the slot it is currently
// using, so concurrent steps can be aggregated instead of overwriting each other.
constexpr size_t PUZZLE_SPACE_STAGE_WORKERS = 256;

// A slot is zero while idle. Otherwise the stage is in the high 32 bits and the
// permille progress in the low 32 bits.
inline auto pack_stage_slot(const puzzle_space_stage stage, const size_t permille) -> u64
{
    return (u64(stage) << 32) | u64(std::min<size_t>(permille, 1000));
}

[[nodiscard]] inline auto stage_slot_stage(const u64 slot) -> puzzle_space_stage
{
    return static_cast<puzzle_space_stage>(slot >> 32);
}

[[nodiscard]] inline auto stage_slot_permille(const u64 slot) -> size_t
{
    return static_cast<size_t>(slot & 0xffffffffull);
}

/**
 * Configuration for a complete puzzle-space search. Everything that can be derived from a board
 * is derived by make_puzzle_space_config(); the remaining fields are explicit search limits.
 */
struct wall_search_options
{
    bool inventory = false;
    size_t min_walls = 0;
    size_t max_walls = 0;
};

struct puzzle_space_config
{
    u8 width = 3;
    u8 height = 3;
    u8 goal_x = 0;
    u8 goal_y = 0;
    bool restricted = false;
    // Fixed walls. Mutually exclusive with wall_inventory_enabled.
    u64 walls = 0;
    bool wall_inventory_enabled = false;
    blockset2 wall_inventory;
    size_t min_walls = 0;
    size_t max_walls = 0;
    block target;
    blockset2 inventory;
    std::tuple<u8, u8, u8, u8> target_positions{0, 0, 0, 0};
    // Both limits count the target block. The target plus at least one movable
    // block is always required, so min_blocks values below 2 behave like 2.
    size_t max_blocks = 1;
    size_t min_blocks = 2;
    int min_moves = 0;
};

struct puzzle_space_config_result
{
    std::optional<puzzle_space_config> config;
    std::string error;
};

struct puzzle_space_cluster
{
    puzzle cluster_id;
    puzzle hardest;
    int max_moves = 0;
    size_t state_count = 0;
    size_t block_count = 0;
    // Number of wall pieces in the layout. Fixed-wall searches leave this at 0.
    size_t wall_count = 0;
};

enum class puzzle_space_sort
{
    most_moves,
    most_blocks_fewest_states_most_moves,
};

auto sort_puzzle_space_clusters(std::vector<puzzle_space_cluster>& clusters, puzzle_space_sort sort)
    -> void;

struct puzzle_space_result
{
    puzzle_space_config config;
    std::vector<puzzle_space_cluster> clusters;
    bool cancelled = false;
    std::string error;
};

/**
 * Progress shared between the search worker and the UI. All fields are written by the worker and
 * may be read from the main thread at any time.
 */
struct puzzle_space_progress
{
    std::atomic<puzzle_space_phase> phase{puzzle_space_phase::idle};
    // Per-worker progress of the current long step. Workers never share a slot,
    // so the reader can aggregate them (see search_snapshot::stage). The state
    // count weights the aggregate so a giant partition dominates the small ones
    // that are explored next to it.
    std::array<std::atomic<u64>, PUZZLE_SPACE_STAGE_WORKERS> stage_slots{};
    std::array<std::atomic<size_t>, PUZZLE_SPACE_STAGE_WORKERS> stage_slot_states{};
    // States of the partition whose index is being built, and how many
    // partitions are in flight right now.
    std::atomic<size_t> stage_partition_states{0};
    std::atomic<size_t> active_partitions{0};

    std::atomic<size_t> inventories{0};
    std::atomic<size_t> wall_layouts{0};
    std::atomic<size_t> starting_positions{0};
    std::atomic<size_t> candidates_total{0};
    std::atomic<size_t> candidates_done{0};
    // Counting-phase progress; candidates_total is only final once counting ends.
    std::atomic<size_t> counted_partitions{0};
    std::atomic<size_t> counted_placements{0};
    std::atomic<size_t> unique_candidates{0};
    std::atomic<size_t> duplicate_candidates{0};
    std::atomic<size_t> clusters_found{0};
    std::atomic<size_t> states_explored{0};
    std::atomic<size_t> cache_states{0};
    std::atomic<size_t> max_active_states{0};
    std::atomic<size_t> live_states{0};
    std::atomic<size_t> peak_live_states{0};

    // Probe statistics used to initialize the projections.
    std::atomic<size_t> probe_candidates{0};
    std::atomic<size_t> probe_unique{0};

    // Rough memory accounting. All values are estimates of our own containers, not the process
    // resident set size.
    std::atomic<size_t> fixed_bytes{0};
    std::atomic<size_t> cache_bytes{0};
    std::atomic<size_t> active_bytes{0};
    std::atomic<size_t> result_bytes{0};
    std::atomic<size_t> estimated_peak_bytes{0};
    std::atomic<size_t> projected_peak_bytes{0};
    std::atomic<size_t> probe_peak_bytes{0};
    std::atomic<std::int64_t> eta_seconds{-1};
    std::atomic<std::int64_t> probe_eta_seconds{-1};
};

// Derives all search parameters that have a board equivalent. max_blocks == 0 means "use the
// current number of non-wall blocks". min_blocks counts the target like max_blocks does.
auto make_puzzle_space_config(const puzzle& board, size_t max_blocks, int min_moves,
                              size_t min_blocks = 2, const wall_search_options& walls = {})
    -> puzzle_space_config_result;

// Shared by the CLI and the GUI search job. Partitions are processed largest
// first in batches of BATCH_STATE_BUDGET placements; a single partition above
// that budget runs alone with intra-partition parallelism. batch_state_budget
// overrides the batch size (0 = default).
auto run_puzzle_space_search(const puzzle_space_config& config, puzzle_space_progress* progress,
                             std::stop_token stop, threadpool pool = std::nullopt,
                             size_t batch_state_budget = 0) -> puzzle_space_result;

#endif
