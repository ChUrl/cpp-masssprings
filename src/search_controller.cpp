#include "search_controller.hpp"

#include "config.hpp"
#include "util.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <mutex>

struct search_controller::shared_state
{
    std::atomic<search_kind> kind{search_kind::none};
    std::atomic<search_status> status{search_status::idle};
    std::atomic<bool> done{false};
    std::atomic<size_t> states_explored{0};
    std::atomic<std::int64_t> elapsed_ms{0};

    puzzle_space_progress progress;

    std::mutex result_mtx;
    std::optional<state_space_job_result> state_result;
    std::optional<puzzle_space_result> puzzle_result;

    std::mutex error_mtx;
    std::string error;

    std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
};

namespace
{
class state_space_observer : public exploration_observer
{
private:
    search_controller::shared_state* shared;
    std::stop_token stop;

public:
    state_space_observer(search_controller::shared_state* _shared, const std::stop_token _stop)
        : shared(_shared), stop(_stop)
    {}

    auto on_states_explored(const size_t states) -> bool override
    {
        shared->states_explored.store(states, std::memory_order_relaxed);
        return !stop.stop_requested();
    }
};
} // namespace

search_controller::~search_controller()
{
    stop_worker();
}

auto resolve_search_threads(const int setting) -> int
{
#ifdef THREADPOOL
    if (setting > 0) {
        return setting;
    }
    const unsigned int hardware = std::thread::hardware_concurrency();
    return static_cast<int>(hardware > 2 ? std::max(1u, hardware - 2) : 1u);
#else
    (void)setting;
    return 1;
#endif
}

auto search_controller::make_pool(const int threads) -> threadpool
{
#ifdef THREADPOOL
    const unsigned int count =
        static_cast<unsigned int>(std::max(1, resolve_search_threads(threads)));
    pool_ = std::make_unique<BS::thread_pool<>>(count);
    return threadpool{pool_.get()};
#else
    (void)threads;
    return std::nullopt;
#endif
}

auto search_controller::stop_worker() -> void
{
    if (worker_.joinable()) {
        worker_.request_stop();
        worker_.join();
    }
    pool_.reset();
    shared_.reset();
}

auto search_controller::has_pending_result() const -> bool
{
    if (!shared_) {
        return false;
    }
    std::lock_guard<std::mutex> lock(shared_->result_mtx);
    return shared_->state_result.has_value() || shared_->puzzle_result.has_value();
}

auto search_controller::start_state_space(std::unique_ptr<state_model> model, const state_id start,
                                          const state_id current, std::string type_id,
                                          const std::uint64_t generation, const int threads) -> bool
{
    if (running() || !model) {
        return false;
    }

    poll();
    if (has_pending_result()) {
        return false;
    }
    stop_worker();
    shared_ = std::make_shared<shared_state>();
    shared_->kind.store(search_kind::state_space, std::memory_order_relaxed);
    shared_->status.store(search_status::running, std::memory_order_relaxed);
    shared_->progress.phase.store(puzzle_space_phase::exploring, std::memory_order_relaxed);
    shared_->started = std::chrono::steady_clock::now();

    const threadpool pool = make_pool(threads);

    worker_ = std::jthread(
        [shared = shared_, model = std::move(model), start, current, type_id = std::move(type_id),
         generation, pool](const std::stop_token stop) mutable
        {
            const auto finish = [&](const search_status status, const std::string& error)
            {
                if (!error.empty()) {
                    infoln("State-space search failed: {}", error);
                    std::lock_guard<std::mutex> lock(shared->error_mtx);
                    shared->error = error;
                }
                shared->elapsed_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                                             std::chrono::steady_clock::now() - shared->started)
                                             .count(),
                                         std::memory_order_relaxed);
                shared->status.store(status, std::memory_order_relaxed);
                shared->done.store(true, std::memory_order_release);
            };

            try {
                infoln("State-space search started for {} ({} states)", type_id,
                       model->state_count());

                std::jthread sampler;
                if (log_enabled(2)) {
                    sampler = std::jthread(
                        [shared = shared.get(), stop](const std::stop_token sampler_stop)
                        {
                            while (!shared->done.load(std::memory_order_relaxed) &&
                                   !stop.stop_requested() && !sampler_stop.stop_requested()) {
                                for (int i = 0;
                                     i < 5 && !shared->done.load(std::memory_order_relaxed) &&
                                     !stop.stop_requested() && !sampler_stop.stop_requested();
                                     ++i) {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                                }
                                if (shared->done.load(std::memory_order_relaxed) ||
                                    stop.stop_requested() || sampler_stop.stop_requested()) {
                                    break;
                                }
                                const double elapsed =
                                    std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                                                  shared->started)
                                        .count();
                                traceln("State-space search: {} states explored, {:.1f}s",
                                        format_count(shared->states_explored.load(
                                            std::memory_order_relaxed)),
                                        elapsed);
                            }
                        });
                }

                state_space_observer observer(shared.get(), stop);
                exploration_result result = model->explore(start, current, pool, &observer);
                const bool cancelled = result.cancelled;
                sampler.request_stop();
                if (sampler.joinable()) {
                    sampler.join();
                }

                infoln("State-space search finished: {} states, {} links, {:.2f}s",
                       model->state_count(), format_count(result.links.size()),
                       std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                                     shared->started)
                           .count());

                {
                    std::lock_guard<std::mutex> lock(shared->result_mtx);
                    shared->state_result = state_space_job_result{
                        std::move(model), std::move(result), std::move(type_id), generation};
                }

                finish(cancelled ? search_status::cancelled : search_status::completed, {});
            }
            catch (const std::exception& exception) {
                finish(search_status::failed, exception.what());
            }
            catch (...) {
                finish(search_status::failed, "unknown error");
            }
        });
    return true;
}

auto search_controller::start_puzzle_space(puzzle_space_config config, const int threads) -> bool
{
    if (running()) {
        return false;
    }

    poll();
    if (has_pending_result()) {
        return false;
    }
    stop_worker();
    shared_ = std::make_shared<shared_state>();
    shared_->kind.store(search_kind::puzzle_space, std::memory_order_relaxed);
    shared_->status.store(search_status::running, std::memory_order_relaxed);
    shared_->started = std::chrono::steady_clock::now();

    const threadpool pool = make_pool(threads);

    worker_ = std::jthread(
        [shared = shared_, config = std::move(config), pool](const std::stop_token stop) mutable
        {
            const auto finish = [&](const search_status status, const std::string& error)
            {
                if (!error.empty()) {
                    infoln("Puzzle-space search failed: {}", error);
                    std::lock_guard<std::mutex> lock(shared->error_mtx);
                    shared->error = error;
                }
                shared->elapsed_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                                             std::chrono::steady_clock::now() - shared->started)
                                             .count(),
                                         std::memory_order_relaxed);
                shared->status.store(status, std::memory_order_relaxed);
                shared->done.store(true, std::memory_order_release);
            };

            try {
                puzzle_space_result result =
                    run_puzzle_space_search(config, &shared->progress, stop, pool);

                const bool cancelled = result.cancelled;
                const std::string error = result.error;
                {
                    std::lock_guard<std::mutex> lock(shared->result_mtx);
                    shared->puzzle_result = std::move(result);
                }

                finish(cancelled
                           ? search_status::cancelled
                           : (error.empty() ? search_status::completed : search_status::failed),
                       error);
            }
            catch (const std::exception& exception) {
                finish(search_status::failed, exception.what());
            }
            catch (...) {
                finish(search_status::failed, "unknown error");
            }
        });
    return true;
}

auto search_controller::cancel() -> void
{
    if (shared_) {
        const search_status status = shared_->status.load(std::memory_order_relaxed);
        if (status == search_status::running) {
            shared_->status.store(search_status::cancelling, std::memory_order_relaxed);
        }
    }
    if (worker_.joinable()) {
        worker_.request_stop();
    }
}

auto search_controller::poll() -> void
{
    if (!worker_.joinable() || !shared_->done.load(std::memory_order_acquire)) {
        return;
    }

    worker_.join();
    pool_.reset();
}

auto search_controller::running() const -> bool
{
    return worker_.joinable() && shared_ && !shared_->done.load(std::memory_order_acquire);
}

auto search_controller::kind() const -> search_kind
{
    if (!shared_) {
        return search_kind::none;
    }
    return shared_->kind.load(std::memory_order_relaxed);
}

auto search_controller::current_memory_bytes() const -> size_t
{
    if (!shared_) {
        return 0;
    }
    return shared_->progress.fixed_bytes.load(std::memory_order_relaxed) +
        shared_->progress.cache_bytes.load(std::memory_order_relaxed) +
        shared_->progress.active_bytes.load(std::memory_order_relaxed) +
        shared_->progress.result_bytes.load(std::memory_order_relaxed);
}

auto search_controller::active_thread_count() const -> int
{
    if (pool_) {
        return static_cast<int>(pool_->get_thread_count());
    }
    return 0;
}

auto search_controller::snapshot() const -> search_snapshot
{
    search_snapshot snapshot;
    if (!shared_) {
        return snapshot;
    }

    snapshot.kind = shared_->kind.load(std::memory_order_relaxed);
    snapshot.status = shared_->status.load(std::memory_order_relaxed);
    snapshot.phase = shared_->progress.phase.load(std::memory_order_relaxed);
    snapshot.partitions_total = shared_->progress.inventories.load(std::memory_order_relaxed);
    snapshot.wall_layouts = shared_->progress.wall_layouts.load(std::memory_order_relaxed);
    snapshot.partitions_counted =
        shared_->progress.counted_partitions.load(std::memory_order_relaxed);
    snapshot.placements_counted =
        shared_->progress.counted_placements.load(std::memory_order_relaxed);
    snapshot.states_explored = shared_->states_explored.load(std::memory_order_relaxed);
    snapshot.candidates_done = shared_->progress.candidates_done.load(std::memory_order_relaxed);
    snapshot.candidates_total = shared_->progress.candidates_total.load(std::memory_order_relaxed);
    snapshot.clusters_found = shared_->progress.clusters_found.load(std::memory_order_relaxed);

    // Aggregate the per-worker step progress. The number should answer "how far
    // is the build we are waiting for", so the aggregate is weighted by partition
    // size and dominated by the largest builds; the small helper partitions that
    // are swept next to a giant one must not make it wobble.
    size_t indexing_max = 0;
    size_t aggregating_max = 0;
    for (size_t i = 0; i < PUZZLE_SPACE_STAGE_WORKERS; ++i) {
        const u64 value = shared_->progress.stage_slots[i].load(std::memory_order_relaxed);
        if (value == 0) {
            continue;
        }
        const size_t states =
            shared_->progress.stage_slot_states[i].load(std::memory_order_relaxed);
        if (stage_slot_stage(value) == puzzle_space_stage::indexing) {
            indexing_max = std::max(indexing_max, states);
        } else if (stage_slot_stage(value) == puzzle_space_stage::aggregating) {
            aggregating_max = std::max(aggregating_max, states);
        }
    }

    size_t indexing_workers = 0;
    size_t indexing_significant = 0;
    size_t indexing_weight = 0;
    size_t indexing_sum = 0;
    size_t aggregating_workers = 0;
    size_t aggregating_significant = 0;
    size_t aggregating_weight = 0;
    size_t aggregating_sum = 0;
    for (size_t i = 0; i < PUZZLE_SPACE_STAGE_WORKERS; ++i) {
        const u64 value = shared_->progress.stage_slots[i].load(std::memory_order_relaxed);
        if (value == 0) {
            continue;
        }
        const size_t states =
            shared_->progress.stage_slot_states[i].load(std::memory_order_relaxed);
        const size_t progress = states * stage_slot_permille(value);
        switch (stage_slot_stage(value)) {
        case puzzle_space_stage::indexing:
            ++indexing_workers;
            // Ignore builds that are an order of magnitude smaller than the
            // largest one in flight: they finish in a fraction of the time.
            if (states * 10 < indexing_max) {
                break;
            }
            ++indexing_significant;
            indexing_weight += states;
            indexing_sum += progress;
            break;
        case puzzle_space_stage::aggregating:
            ++aggregating_workers;
            if (states * 10 < aggregating_max) {
                break;
            }
            ++aggregating_significant;
            aggregating_weight += states;
            aggregating_sum += progress;
            break;
        case puzzle_space_stage::none:
            break;
        }
    }
    if (indexing_significant > 0 && indexing_weight >= aggregating_weight) {
        snapshot.stage = puzzle_space_stage::indexing;
        snapshot.stage_workers = indexing_workers;
        snapshot.stage_done = indexing_sum;
        snapshot.stage_total = indexing_weight * 1000;
    } else if (aggregating_significant > 0) {
        snapshot.stage = puzzle_space_stage::aggregating;
        snapshot.stage_workers = aggregating_workers;
        snapshot.stage_done = aggregating_sum;
        snapshot.stage_total = aggregating_weight * 1000;
    } else {
        snapshot.stage = puzzle_space_stage::none;
    }
    snapshot.stage_partition_states =
        shared_->progress.stage_partition_states.load(std::memory_order_relaxed);
    snapshot.active_partitions =
        shared_->progress.active_partitions.load(std::memory_order_relaxed);

    std::int64_t elapsed = shared_->elapsed_ms.load(std::memory_order_relaxed);
    if (snapshot.status == search_status::running || snapshot.status == search_status::cancelling) {
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - shared_->started)
                      .count();
    }
    snapshot.elapsed_seconds = static_cast<double>(elapsed) / 1000.0;

    if (snapshot.kind == search_kind::puzzle_space) {
        snapshot.states_explored =
            shared_->progress.states_explored.load(std::memory_order_relaxed);
        snapshot.unique_candidates =
            shared_->progress.unique_candidates.load(std::memory_order_relaxed);
        snapshot.duplicate_candidates =
            shared_->progress.duplicate_candidates.load(std::memory_order_relaxed);
        snapshot.current_search_bytes =
            shared_->progress.fixed_bytes.load(std::memory_order_relaxed) +
            shared_->progress.cache_bytes.load(std::memory_order_relaxed) +
            shared_->progress.active_bytes.load(std::memory_order_relaxed) +
            shared_->progress.result_bytes.load(std::memory_order_relaxed);
        snapshot.estimated_peak_bytes =
            shared_->progress.projected_peak_bytes.load(std::memory_order_relaxed);
        snapshot.eta_seconds =
            static_cast<double>(shared_->progress.eta_seconds.load(std::memory_order_relaxed));

        switch (snapshot.phase) {
        case puzzle_space_phase::idle:
            snapshot.progress_fraction = 0.0f;
            break;
        case puzzle_space_phase::inventories:
            snapshot.progress_fraction = 0.01f;
            break;
        case puzzle_space_phase::starting_positions:
            snapshot.progress_fraction = 0.03f;
            break;
        case puzzle_space_phase::counting:
            snapshot.progress_fraction = snapshot.partitions_total > 0 ? 0.02f +
                    0.08f * static_cast<float>(snapshot.partitions_counted) /
                        static_cast<float>(snapshot.partitions_total)
                                                                       : 0.02f;
            break;
        case puzzle_space_phase::probing:
            snapshot.progress_fraction = 0.07f;
            break;
        case puzzle_space_phase::exploring:
            snapshot.progress_fraction = snapshot.candidates_total > 0 ? 0.10f +
                    0.90f * static_cast<float>(snapshot.candidates_done) /
                        static_cast<float>(snapshot.candidates_total)
                                                                       : 0.10f;
            break;
        case puzzle_space_phase::finishing:
            snapshot.progress_fraction = 0.995f;
            break;
        case puzzle_space_phase::done:
            snapshot.progress_fraction = 1.0f;
            break;
        case puzzle_space_phase::cancelled:
        case puzzle_space_phase::failed:
            break;
        }
    }

    snapshot.active_threads = active_thread_count();

    {
        std::lock_guard<std::mutex> lock(shared_->error_mtx);
        snapshot.error = shared_->error;
    }

    return snapshot;
}

auto search_controller::take_state_space_result() -> std::optional<state_space_job_result>
{
    if (!shared_) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(shared_->result_mtx);
    if (!shared_->state_result) {
        return std::nullopt;
    }
    std::optional<state_space_job_result> result = std::move(shared_->state_result);
    shared_->state_result.reset();
    return result;
}

auto search_controller::take_puzzle_space_result() -> std::optional<puzzle_space_result>
{
    if (!shared_) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(shared_->result_mtx);
    if (!shared_->puzzle_result) {
        return std::nullopt;
    }
    std::optional<puzzle_space_result> result = std::move(shared_->puzzle_result);
    shared_->puzzle_result.reset();
    return result;
}
