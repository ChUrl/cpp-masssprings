#ifndef SEARCH_CONTROLLER_HPP_
#define SEARCH_CONTROLLER_HPP_

#include "puzzle_space.hpp"
#include "state_model.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>

enum class search_kind
{
    none,
    state_space,
    puzzle_space,
};

enum class search_status
{
    idle,
    running,
    cancelling,
    completed,
    cancelled,
    failed,
};

struct search_snapshot
{
    search_kind kind = search_kind::none;
    search_status status = search_status::idle;
    puzzle_space_phase phase = puzzle_space_phase::idle;
    puzzle_space_stage stage = puzzle_space_stage::none;
    size_t states_explored = 0;
    size_t wall_layouts = 0;
    size_t candidates_done = 0;
    size_t candidates_total = 0;
    // Counting-phase progress; candidates_total is only final once counting ends.
    size_t partitions_total = 0;
    size_t partitions_counted = 0;
    size_t placements_counted = 0;
    // Aggregated progress of the long steps inside the exploration phase,
    // weighted by the partition size each worker is handling: stage_total is the
    // sum of those sizes times 1000, stage_done the sum of their permille
    // progress, stage_workers how many workers are in the step.
    size_t stage_done = 0;
    size_t stage_total = 0;
    size_t stage_workers = 0;
    // Size of the partition whose index is being built, and how many partitions
    // are being processed right now.
    size_t stage_partition_states = 0;
    size_t active_partitions = 0;
    size_t unique_candidates = 0;
    size_t duplicate_candidates = 0;
    size_t clusters_found = 0;
    double elapsed_seconds = 0.0;
    double eta_seconds = -1.0;
    float progress_fraction = -1.0f;
    size_t current_search_bytes = 0;
    size_t estimated_peak_bytes = 0;
    int active_threads = 0;
    std::string error;

    [[nodiscard]] auto running() const -> bool
    {
        return status == search_status::running || status == search_status::cancelling;
    }
};

[[nodiscard]] auto resolve_search_threads(int setting) -> int;

struct state_space_job_result
{
    std::unique_ptr<state_model> model;
    exploration_result exploration;
    std::string type_id;
    std::uint64_t generation = 0;
};

/**
 * Runs one state-space or puzzle-space exploration in the background. All results are handed back
 * to the main thread through take_*_result(); the controller never mutates state_manager data on
 * the worker thread.
 */
class search_controller
{
public:
    struct shared_state;

    search_controller() = default;
    ~search_controller();

    search_controller(const search_controller&) = delete;
    auto operator=(const search_controller&) -> search_controller& = delete;
    search_controller(search_controller&&) = delete;
    auto operator=(search_controller&&) -> search_controller& = delete;

    auto start_state_space(std::unique_ptr<state_model> model, state_id start, state_id current,
                           std::string type_id, std::uint64_t generation, int threads) -> bool;
    auto start_puzzle_space(puzzle_space_config config, int threads) -> bool;

    auto cancel() -> void;
    auto poll() -> void;

    [[nodiscard]] auto running() const -> bool;
    [[nodiscard]] auto kind() const -> search_kind;
    [[nodiscard]] auto snapshot() const -> search_snapshot;
    [[nodiscard]] auto current_memory_bytes() const -> size_t;
    [[nodiscard]] auto active_thread_count() const -> int;

    [[nodiscard]] auto take_state_space_result() -> std::optional<state_space_job_result>;
    [[nodiscard]] auto take_puzzle_space_result() -> std::optional<puzzle_space_result>;

private:
    std::shared_ptr<shared_state> shared_;
    std::unique_ptr<BS::thread_pool<>> pool_;
    std::jthread worker_;

    auto stop_worker() -> void;
    auto make_pool(int threads) -> threadpool;
    [[nodiscard]] auto has_pending_result() const -> bool;
};

#endif
