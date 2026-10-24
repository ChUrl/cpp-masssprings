#include "puzzle_space.hpp"

#include "config.hpp"
#include "placement_space.hpp"
#include "util.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <ranges>
#include <thread>
#include <vector>

namespace
{
constexpr u32 INVALID_INDEX = std::numeric_limits<u32>::max();
constexpr size_t BATCH_STATE_BUDGET = 64'000'000;

/**
 * Neighbour lookup. Debug builds verify membership with the checked find();
 * release builds use the unchecked MPHF lookup because the placement set is
 * closed under moves, so every generated neighbour is a member.
 */
inline auto neighbour_id(const placement_table& table, const u8* key, bool& missing) -> u32
{
#ifdef NDEBUG
    (void)missing;
    return table.lookup(key);
#else
    const std::optional<u32> found = table.find(key);
    if (!found) {
        missing = true;
        return INVALID_INDEX;
    }
    return *found;
#endif
}

auto movable_block_count(const puzzle& board) -> size_t
{
    return board.block_count();
}

// (phase names are no longer used by the runner; progress exposes the enum directly)

auto run_parallel(threadpool pool, const size_t count, const std::function<void(size_t)>& work)
    -> void
{
    if (count == 0) {
        return;
    }
    if (pool && *pool) {
        // Dynamic work distribution: the partition sizes differ by orders of
        // magnitude, so contiguous blocks would leave most workers idle.
        const size_t workers = std::min(count, static_cast<size_t>((*pool)->get_thread_count()));
        std::atomic<size_t> next{0};
        (*pool)
            ->submit_loop(
                0, workers,
                [&](size_t)
                {
                    while (true) {
                        const size_t index = next.fetch_add(1, std::memory_order_relaxed);
                        if (index >= count) {
                            break;
                        }
                        work(index);
                    }
                },
                0)
            .wait();
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        work(i);
    }
}

class union_find
{
public:
    explicit union_find(const size_t count) : parent_(count)
    {
        std::iota(parent_.begin(), parent_.end(), u32{0});
    }

    auto find(u32 value) -> u32
    {
        while (parent_[value] != value) {
            parent_[value] = parent_[parent_[value]];
            value = parent_[value];
        }
        return value;
    }

    auto parent() -> std::vector<u32>&
    {
        return parent_;
    }

private:
    std::vector<u32> parent_;
};

struct phase_times
{
    std::atomic<std::int64_t> enumerate{0};
    std::atomic<std::int64_t> union_find{0};
    std::atomic<std::int64_t> components{0};
    std::atomic<std::int64_t> bfs{0};
    std::atomic<std::int64_t> aggregate{0};
    std::atomic<std::int64_t> repr{0};
};

struct explore_shared
{
    const placement_space& space;
    const puzzle_space_config& config;
    std::stop_token stop;
    std::mutex& result_mtx;
    std::vector<puzzle_space_cluster>& clusters;
    std::atomic<size_t>& done;
    std::atomic<size_t>& winnable;
    std::atomic<size_t>& unwinnable;
    std::atomic<size_t>& found;
    std::atomic<bool>& cancelled;
    puzzle_space_progress& progress;
    std::atomic<size_t>& work;       // in thirds of a placement
    std::atomic<size_t>& enumerated; // states enumerated or skipped
    std::chrono::steady_clock::time_point explore_start;
    phase_times& times;
    size_t bytes_per_state = 0;
};

auto store_max(std::atomic<size_t>& target, const size_t value) -> void
{
    size_t current = target.load(std::memory_order_relaxed);
    while (current < value &&
           !target.compare_exchange_weak(current, value, std::memory_order_relaxed)) {
    }
}

/**
 * Counts partitions that are in flight. The exploration phase runs a batch of
 * small partitions next to at most one giant one, so the UI can report both the
 * aggregated index progress and how many partitions are being worked on.
 */
class active_partition_scope
{
public:
    explicit active_partition_scope(std::atomic<size_t>& counter) : counter_(counter)
    {
        counter_.fetch_add(1, std::memory_order_relaxed);
    }

    ~active_partition_scope()
    {
        counter_.fetch_sub(1, std::memory_order_relaxed);
    }

    active_partition_scope(const active_partition_scope&) = delete;
    auto operator=(const active_partition_scope&) -> active_partition_scope& = delete;

private:
    std::atomic<size_t>& counter_;
};

auto clock_now() -> std::chrono::steady_clock::time_point
{
    return std::chrono::steady_clock::now();
}

auto add_time(std::atomic<std::int64_t>& target, const std::chrono::steady_clock::time_point start)
    -> void
{
    target.fetch_add(
        std::chrono::duration_cast<std::chrono::nanoseconds>(clock_now() - start).count(),
        std::memory_order_relaxed);
}

// Cancellation helper for the long single-threaded passes: records the stop and
// lets the caller bail out.
inline auto stop_requested(explore_shared& shared) -> bool
{
    if (shared.stop.stop_requested()) {
        shared.cancelled.store(true, std::memory_order_relaxed);
        return true;
    }
    return false;
}

// A step reports its completion as permille of its total.
inline auto stage_permille(const size_t done, const size_t total) -> size_t
{
    return total == 0 ? 0 : std::min<size_t>(1000, done * 1000 / total);
}

/**
 * Claims a worker's progress slot for one long step. The slot stores the step and
 * its permille progress and is cleared when the scope ends, so the snapshot can
 * aggregate the progress of all workers that are currently in a step.
 */
class stage_scope
{
public:
    stage_scope(puzzle_space_progress& progress, const size_t worker,
                const puzzle_space_stage stage, const size_t states)
        : slot_(progress.stage_slots[worker % PUZZLE_SPACE_STAGE_WORKERS]),
          states_(progress.stage_slot_states[worker % PUZZLE_SPACE_STAGE_WORKERS])
    {
        // Write the weight first so a reader that sees the slot always sees the
        // matching state count.
        states_.store(states, std::memory_order_relaxed);
        slot_.store(pack_stage_slot(stage, 0), std::memory_order_relaxed);
    }

    ~stage_scope()
    {
        slot_.store(0, std::memory_order_relaxed);
        states_.store(0, std::memory_order_relaxed);
    }

    stage_scope(const stage_scope&) = delete;
    auto operator=(const stage_scope&) -> stage_scope& = delete;

private:
    std::atomic<u64>& slot_;
    std::atomic<size_t>& states_;
};

// Publishes the progress of the current long step. A slot only ever moves
// forward, so overlapping reports cannot make the aggregate jump backwards.
auto publish_stage(puzzle_space_progress& progress, const size_t worker,
                   const puzzle_space_stage stage, const size_t permille) -> void
{
    std::atomic<u64>& slot = progress.stage_slots[worker % PUZZLE_SPACE_STAGE_WORKERS];
    const u64 value = pack_stage_slot(stage, permille);
    u64 current = slot.load(std::memory_order_relaxed);
    while (current < value &&
           !slot.compare_exchange_weak(current, value, std::memory_order_relaxed)) {
    }
}

struct index_progress_context
{
    puzzle_space_progress* progress = nullptr;
    size_t worker = 0;
};

auto report_index_progress(void* raw, const size_t done, const size_t total) -> void
{
    auto* context = static_cast<index_progress_context*>(raw);
    publish_stage(*context->progress, context->worker, puzzle_space_stage::indexing,
                  stage_permille(done, total));
}

/**
 * Publishes the smooth progress counters. Each placement contributes three
 * work units: enumeration, union, and distance/aggregation. The displayed
 * placement count and the progress bar therefore advance inside a partition,
 * not only when it finishes.
 */
auto publish_progress(explore_shared& shared) -> void
{
    const size_t done = shared.work.load(std::memory_order_relaxed) / 3;
    store_max(shared.progress.candidates_done, done);
    store_max(shared.progress.states_explored, shared.enumerated.load(std::memory_order_relaxed));

    const size_t total = shared.progress.candidates_total.load(std::memory_order_relaxed);
    if (done >= total) {
        shared.progress.eta_seconds.store(0, std::memory_order_relaxed);
        return;
    }

    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - shared.explore_start)
            .count();
    if (elapsed > 0.5 && done > 0) {
        const double rate = static_cast<double>(done) / elapsed;
        if (rate > 0.0) {
            shared.progress.eta_seconds.store(
                static_cast<std::int64_t>((static_cast<double>(total - done) / rate) + 0.5),
                std::memory_order_relaxed);
        }
    }
}

struct emit_context
{
    placement_table* table = nullptr;
    std::vector<u32>* goals = nullptr;
    explore_shared* shared = nullptr;
    size_t pending = 0;
    bool collect_goals = false;
};

auto emit_placement(void* raw, const u8* key, const bool goal) -> void
{
    auto* context = static_cast<emit_context*>(raw);
    const u32 id = context->table->insert(key);
    if (goal && context->collect_goals) {
        context->goals->push_back(id);
    }

    ++context->pending;
    if ((context->pending & 0x3fff) == 0) {
        context->shared->work.fetch_add(context->pending, std::memory_order_relaxed);
        context->shared->enumerated.fetch_add(context->pending, std::memory_order_relaxed);
        context->pending = 0;
        publish_progress(*context->shared);
    }
}

// ---------------------------------------------------------------------------
// Intra-partition parallelism for large partitions
// ---------------------------------------------------------------------------

constexpr size_t LARGE_PARTITION_THRESHOLD = 100'000;
constexpr u32 UNION_CHUNK = 4096;

auto uf_find_atomic(std::vector<u32>& parent, u32 value) -> u32
{
    while (true) {
        const u32 current = std::atomic_ref(parent[value]).load(std::memory_order_relaxed);
        if (current == value) {
            return value;
        }
        const u32 grandparent = std::atomic_ref(parent[current]).load(std::memory_order_relaxed);
        std::atomic_ref(parent[value]).store(grandparent, std::memory_order_relaxed);
        value = grandparent;
    }
}

auto uf_unite_atomic(std::vector<u32>& parent, u32 a, u32 b) -> void
{
    while (true) {
        a = uf_find_atomic(parent, a);
        b = uf_find_atomic(parent, b);
        if (a == b) {
            return;
        }
        if (a < b) {
            u32 expected = b;
            if (std::atomic_ref(parent[b]).compare_exchange_weak(expected, a,
                                                                 std::memory_order_relaxed)) {
                return;
            }
        } else {
            u32 expected = a;
            if (std::atomic_ref(parent[a]).compare_exchange_weak(expected, b,
                                                                 std::memory_order_relaxed)) {
                return;
            }
        }
    }
}

struct union_atomic_context
{
    const placement_table* table = nullptr;
    std::vector<u32>* parent = nullptr;
    u32 current = 0;
    size_t missing = 0;
};

auto union_move_atomic(void* raw, const u8* key) -> void
{
    auto* context = static_cast<union_atomic_context*>(raw);
    bool missing = false;
    const u32 next = neighbour_id(*context->table, key, missing);
    if (next == INVALID_INDEX) {
        ++context->missing;
        return;
    }
    uf_unite_atomic(*context->parent, context->current, next);
}

/**
 * Parallel move-union. Each worker claims chunks of states from an atomic
 * cursor; union-find itself is lock-free (atomic parent updates with path
 * halving).
 */
auto parallel_union(explore_shared& shared, const placement_partition& partition,
                    const placement_table& table, std::vector<u32>& parent,
                    const size_t state_count, threadpool pool) -> size_t
{
    if (!pool || !*pool || (*pool)->get_thread_count() < 2) {
        return 0;
    }

    std::atomic<u32> next{0};
    std::atomic<size_t> missing{0};
    const size_t workers = std::min(state_count, (*pool)->get_thread_count());

    run_parallel(pool, workers,
                 [&](size_t)
                 {
                     size_t local_missing = 0;
                     size_t pending = 0;
                     while (true) {
                         if (shared.stop.stop_requested()) {
                             shared.cancelled.store(true, std::memory_order_relaxed);
                             break;
                         }
                         const u32 start = next.fetch_add(UNION_CHUNK, std::memory_order_relaxed);
                         if (start >= state_count) {
                             break;
                         }
                         const u32 end =
                             static_cast<u32>(std::min<size_t>(state_count, start + UNION_CHUNK));
                         for (u32 state = start; state < end; ++state) {
                             union_atomic_context context{&table, &parent, state, 0};
                             shared.space.for_each_move(partition, table.key(state),
                                                        union_move_atomic, &context);
                             local_missing += context.missing;
                         }
                         pending += end - start;
                         if (pending >= 0x3fff) {
                             shared.work.fetch_add(pending, std::memory_order_relaxed);
                             pending = 0;
                             publish_progress(shared);
                         }
                     }
                     if (pending != 0) {
                         shared.work.fetch_add(pending, std::memory_order_relaxed);
                         publish_progress(shared);
                     }
                     if (local_missing != 0) {
                         missing.fetch_add(local_missing, std::memory_order_relaxed);
                     }
                 });

    return missing.load(std::memory_order_relaxed);
}

struct bfs_parallel_context
{
    const placement_table* table = nullptr;
    std::vector<std::int32_t>* distances = nullptr;
    std::vector<u32>* next = nullptr;
    u32 current = 0;
    std::int32_t next_distance = 0;
};

auto bfs_parallel_move(void* raw, const u8* key) -> void
{
    auto* context = static_cast<bfs_parallel_context*>(raw);
    bool missing = false;
    const u32 next = neighbour_id(*context->table, key, missing);
    if (next == INVALID_INDEX) {
        return;
    }
    std::atomic_ref<std::int32_t> distance((*context->distances)[next]);
    std::int32_t expected = -1;
    if (distance.compare_exchange_strong(expected, context->next_distance,
                                         std::memory_order_relaxed)) {
        context->next->push_back(next);
    }
}

struct bfs_serial_context
{
    const placement_table* table = nullptr;
    std::vector<std::int32_t>* distances = nullptr;
    std::vector<u32>* next = nullptr;
    std::int32_t next_distance = 0;
};

auto bfs_serial_move(void* raw, const u8* key) -> void
{
    auto* context = static_cast<bfs_serial_context*>(raw);
    bool missing = false;
    const u32 next = neighbour_id(*context->table, key, missing);
    if (next == INVALID_INDEX) {
        return;
    }
    if ((*context->distances)[next] < 0) {
        (*context->distances)[next] = context->next_distance;
        context->next->push_back(next);
    }
}

/**
 * Level-synchronous multi-source BFS. Callers must check that the pool has at
 * least two workers and fall back to serial_bfs() otherwise.
 */
auto parallel_bfs(explore_shared& shared, const placement_partition& partition,
                  const placement_table& table, std::vector<std::int32_t>& distances,
                  std::vector<u32> seeds, threadpool pool) -> void
{
    if (!pool || !*pool || (*pool)->get_thread_count() < 2) {
        return;
    }

    const size_t max_workers = (*pool)->get_thread_count();
    std::vector<u32> frontier = std::move(seeds);
    std::vector<std::vector<u32>> next(max_workers);

    shared.work.fetch_add(frontier.size(), std::memory_order_relaxed);
    publish_progress(shared);

    std::int32_t level = 0;
    while (!frontier.empty()) {
        if (stop_requested(shared)) {
            return;
        }

        for (std::vector<u32>& buffer : next) {
            buffer.clear();
        }

        const size_t workers = std::min(max_workers, frontier.size());
        const std::int32_t next_distance = level + 1;
        run_parallel(pool, workers,
                     [&](const size_t worker)
                     {
                         std::vector<u32>& output = next[worker];
                         size_t processed = 0;
                         for (size_t i = worker; i < frontier.size(); i += workers) {
                             if ((++processed & 0x3fff) == 0 && shared.stop.stop_requested()) {
                                 break;
                             }
                             const u32 state = frontier[i];
                             bfs_parallel_context context{&table, &distances, &output, state,
                                                          next_distance};
                             shared.space.for_each_move(partition, table.key(state),
                                                        bfs_parallel_move, &context);
                         }
                     });

        // A cancelled level must not pay for merging the partial frontier.
        if (stop_requested(shared)) {
            return;
        }

        frontier.clear();
        for (const std::vector<u32>& buffer : next) {
            frontier.insert(frontier.end(), buffer.begin(), buffer.end());
        }
        shared.work.fetch_add(frontier.size(), std::memory_order_relaxed);
        publish_progress(shared);
        ++level;
    }
}

/**
 * Serial fallback with the same level-synchronous semantics, used when the pool
 * cannot provide enough workers.
 */
auto serial_bfs(explore_shared& shared, const placement_partition& partition,
                const placement_table& table, std::vector<std::int32_t>& distances,
                std::vector<u32> seeds) -> void
{
    std::vector<u32> frontier = std::move(seeds);
    std::vector<u32> next;

    shared.work.fetch_add(frontier.size(), std::memory_order_relaxed);
    publish_progress(shared);

    std::int32_t level = 0;
    while (!frontier.empty()) {
        if (stop_requested(shared)) {
            return;
        }
        next.clear();
        const std::int32_t next_distance = level + 1;
        for (size_t i = 0; i < frontier.size(); ++i) {
            if ((i & 0x3fff) == 0 && stop_requested(shared)) {
                return;
            }
            bfs_serial_context context{&table, &distances, &next, next_distance};
            shared.space.for_each_move(partition, table.key(frontier[i]), bfs_serial_move,
                                       &context);
        }
        frontier.swap(next);
        shared.work.fetch_add(frontier.size(), std::memory_order_relaxed);
        publish_progress(shared);
        ++level;
    }
}

// ---------------------------------------------------------------------------
// Two-bit component/distance sweep used by batch-mode partitions
// ---------------------------------------------------------------------------

inline auto bit_test(const std::vector<u64>& bits, const size_t index) -> bool
{
    return ((bits[index >> 6] >> (index & 63)) & 1u) != 0;
}

inline auto bit_set(std::vector<u64>& bits, const size_t index) -> void
{
    bits[index >> 6] |= u64(1) << (index & 63);
}

struct keyed_state
{
    u32 id = 0;
    std::array<u8, 12> key{};
};

struct keyed_walk_context
{
    const placement_table* table = nullptr;
    std::vector<u64>* seen = nullptr;
    std::vector<keyed_state>* next = nullptr;
    size_t bytes = 0;
};

auto visit_keyed(void* raw, const u8* key) -> void
{
    auto* context = static_cast<keyed_walk_context*>(raw);
    const u32 id = context->table->lookup(key);
    if (bit_test(*context->seen, id)) {
        return;
    }
    bit_set(*context->seen, id);
    keyed_state entry;
    entry.id = id;
    std::memcpy(entry.key.data(), key, context->bytes);
    context->next->push_back(entry);
}

struct keyed_seed_context
{
    explore_shared& shared;
    const placement_partition& partition;
    const placement_table& table;
    std::vector<u64>& component_seen;
    std::vector<u64>& distance_seen;
    std::vector<puzzle_space_cluster>& local;
    size_t& visited;
    size_t bytes = 0;
    bool cancelled = false;
    // Reused across the components of this partition: a component walk does not
    // allocate its frontiers from scratch.
    std::vector<keyed_state> frontier;
    std::vector<keyed_state> next;
    std::vector<keyed_state> component_goals;
};

auto explore_keyed(keyed_seed_context& context, const keyed_state& seed) -> void
{
    explore_shared& shared = context.shared;
    const placement_partition& partition = context.partition;
    const placement_table& table = context.table;
    const size_t bytes = context.bytes;

    std::vector<keyed_state>& frontier = context.frontier;
    std::vector<keyed_state>& next = context.next;
    std::vector<keyed_state>& component_goals = context.component_goals;
    frontier.clear();
    next.clear();
    component_goals.clear();
    frontier.push_back(seed);
    bit_set(context.component_seen, seed.id);

    size_t component_size = 0;
    bool have_min = false;
    std::array<u8, 12> min_key{};
    std::array<u64, 4> min_repr{};

    // Pass 1: discover the component and accumulate its size, canonical minimum
    // and goal states. Keys travel with the frontier; there is no global key
    // array. The seed is one of the goals, so the number of levels of this walk is
    // an upper bound for the distance to the goal set. The walk stops with the
    // last level still in `frontier`.
    std::int32_t level = 0;
    while (!frontier.empty()) {
        next.clear();
        for (size_t i = 0; i < frontier.size(); ++i) {
            if ((i & 0x3fff) == 0 && shared.stop.stop_requested()) {
                context.cancelled = true;
                return;
            }
            const keyed_state& entry = frontier[i];
            ++component_size;
            if (shared.space.is_goal(entry.key.data())) {
                component_goals.push_back(entry);
            }
            const std::array<u64, 4> repr = shared.space.state_repr(partition, entry.key.data());
            if (!have_min || placement_repr_less(repr, min_repr)) {
                have_min = true;
                min_key = entry.key;
                min_repr = repr;
            }
            keyed_walk_context walk{&table, &context.component_seen, &next, bytes};
            shared.space.for_each_move(partition, entry.key.data(), visit_keyed, &walk);
        }
        shared.work.fetch_add(frontier.size(), std::memory_order_relaxed);
        publish_progress(shared);
        if (next.empty()) {
            break;
        }
        frontier.swap(next);
        ++level;
    }

    context.visited += component_size;

    // No state can be farther from the goal set than from the seed, so a component
    // that is too flat for this walk cannot produce a cluster.
    if (level < shared.config.min_moves) {
        return;
    }

    std::int32_t max_distance = level;
    if (component_goals.size() > 1) {
        // Pass 2: multi-source level BFS from the goals of this component. It also
        // stops with the last level still in `frontier`.
        for (const keyed_state& goal : component_goals) {
            bit_set(context.distance_seen, goal.id);
        }
        frontier = component_goals;
        next.clear();
        max_distance = 0;
        while (!frontier.empty()) {
            next.clear();
            for (size_t i = 0; i < frontier.size(); ++i) {
                if ((i & 0x3fff) == 0 && shared.stop.stop_requested()) {
                    context.cancelled = true;
                    return;
                }
                const keyed_state& entry = frontier[i];
                keyed_walk_context walk{&table, &context.distance_seen, &next, bytes};
                shared.space.for_each_move(partition, entry.key.data(), visit_keyed, &walk);
            }
            shared.work.fetch_add(frontier.size(), std::memory_order_relaxed);
            publish_progress(shared);
            if (next.empty()) {
                break;
            }
            frontier.swap(next);
            ++max_distance;
        }
    }

    // The hardest state is the canonically smallest (by puzzle representation) of
    // the states at the last level. Using the representation instead of the rank
    // id keeps the result independent of the hash index layout and of the order in
    // which states were enumerated.
    bool have_hardest = false;
    std::array<u8, 12> hardest_key{};
    std::array<u64, 4> hardest_repr{};
    for (const keyed_state& entry : frontier) {
        const std::array<u64, 4> repr = shared.space.state_repr(partition, entry.key.data());
        if (!have_hardest || placement_repr_less(repr, hardest_repr)) {
            have_hardest = true;
            hardest_key = entry.key;
            hardest_repr = repr;
        }
    }

    if (max_distance >= shared.config.min_moves && have_hardest) {
        context.local.push_back(puzzle_space_cluster{
            shared.space.to_puzzle(partition, min_key.data()),
            shared.space.to_puzzle(partition, hardest_key.data()),
            max_distance,
            component_size,
            partition.blocks,
            partition.wall_pieces,
        });
    }
}

/**
 * Batch sweep without a global key array. The partition is enumerated a second
 * time (cheap, no insertion) and every unvisited goal starts a component walk
 * whose frontier carries the state keys.
 */
auto sweep_keyed(explore_shared& shared, const placement_partition& partition,
                 const placement_table& table, const std::vector<u32>& goals,
                 const size_t state_count, std::vector<puzzle_space_cluster>& local,
                 bool& cancelled) -> size_t
{
    const size_t bit_words = (state_count + 63) / 64;
    std::vector<u64> component_seen(bit_words, 0);
    std::vector<u64> distance_seen(bit_words, 0);
    size_t visited = 0;

    keyed_seed_context context{
        shared,        partition, table,   component_seen,
        distance_seen, local,     visited, shared.space.key_bytes(partition.blocks),
        false,         {},        {},      {},
    };

    // The goals were collected (and remapped to rank ids) while the index was
    // built, so the partition is not enumerated a second time. Every goal starts
    // one component walk; goals of an already visited component are skipped.
    for (const u32 goal : goals) {
        if (context.cancelled) {
            break;
        }
        if (bit_test(context.component_seen, goal)) {
            continue;
        }
        keyed_state seed;
        seed.id = goal;
        std::memcpy(seed.key.data(), table.key(goal), context.bytes);
        explore_keyed(context, seed);
    }
    if (context.cancelled || shared.stop.stop_requested()) {
        shared.cancelled.store(true, std::memory_order_relaxed);
        cancelled = true;
        return visited;
    }

    // Unwinnable states are never visited by the goal-seeded sweep.
    shared.work.fetch_add(2 * (state_count - visited), std::memory_order_relaxed);
    publish_progress(shared);
    return visited;
}

auto finish_partition(explore_shared& shared, std::vector<puzzle_space_cluster>& local,
                      const size_t visited, const size_t state_count) -> void
{
    {
        std::lock_guard<std::mutex> lock(shared.result_mtx);
        shared.clusters.insert(shared.clusters.end(), local.begin(), local.end());
        shared.found.fetch_add(local.size(), std::memory_order_relaxed);
        shared.progress.result_bytes.store(
            shared.clusters.size() * (sizeof(puzzle_space_cluster) + 2 * sizeof(puzzle) + 32),
            std::memory_order_relaxed);
    }

    shared.done.fetch_add(state_count, std::memory_order_relaxed);
    shared.winnable.fetch_add(visited, std::memory_order_relaxed);
    shared.unwinnable.fetch_add(state_count - visited, std::memory_order_relaxed);
    shared.progress.unique_candidates.store(shared.winnable.load(std::memory_order_relaxed),
                                            std::memory_order_relaxed);
    shared.progress.duplicate_candidates.store(shared.unwinnable.load(std::memory_order_relaxed),
                                               std::memory_order_relaxed);
    publish_progress(shared);
}

class live_memory_guard
{
public:
    live_memory_guard(explore_shared& shared, const size_t states)
        : shared_(shared), states_(states), bytes_(states * shared.bytes_per_state)
    {
        const size_t live =
            shared_.progress.live_states.fetch_add(states_, std::memory_order_relaxed) + states_;
        store_max(shared_.progress.peak_live_states, live);
        store_max(shared_.progress.cache_states, live);
        store_max(shared_.progress.max_active_states, live);

        const size_t live_bytes =
            shared_.progress.cache_bytes.fetch_add(bytes_, std::memory_order_relaxed) + bytes_;
        store_max(shared_.progress.projected_peak_bytes, live_bytes * 11 / 10);
    }

    ~live_memory_guard()
    {
        shared_.progress.live_states.fetch_sub(states_, std::memory_order_relaxed);
        shared_.progress.cache_bytes.fetch_sub(bytes_, std::memory_order_relaxed);
    }

    live_memory_guard(const live_memory_guard&) = delete;
    auto operator=(const live_memory_guard&) -> live_memory_guard& = delete;

private:
    explore_shared& shared_;
    size_t states_ = 0;
    size_t bytes_ = 0;
};

struct built_partition
{
    built_partition(const size_t expected, const size_t key_bytes) : table(expected, key_bytes)
    {}

    placement_table table;
    std::vector<u32> goals;
    size_t state_count = 0;
    bool parallel = false;
    // Keeps the partition's memory accounted for until it has been explored.
    std::unique_ptr<live_memory_guard> live;
};

/**
 * Enumerates a partition and builds its perfect hash index. The table is handed
 * back instead of being explored right away: index construction is the only
 * serial step of a partition, so the scheduler overlaps it with other work.
 */
auto build_partition(explore_shared& shared, const size_t index, const size_t worker,
                     const bool allow_parallel) -> std::unique_ptr<built_partition>
{
    if (stop_requested(shared)) {
        return nullptr;
    }

    const placement_partition& partition = shared.space.partition(index);
    if (partition.placements == 0) {
        return nullptr;
    }

    // Parts that can be discarded without exploring them: no placement reaches the
    // goal, a component can never be harder than placements - 1 moves, or the
    // partition fills the board completely so no legal move exists at all.
    const bool too_small = partition.placements <= static_cast<size_t>(shared.config.min_moves);
    const bool board_full =
        shared.config.min_moves >= 1 && partition.area >= shared.space.free_cells(partition);
    if (!partition.has_goal || too_small || board_full) {
        shared.work.fetch_add(partition.placements * 3, std::memory_order_relaxed);
        shared.enumerated.fetch_add(partition.placements, std::memory_order_relaxed);
        publish_progress(shared);

        shared.done.fetch_add(partition.placements, std::memory_order_relaxed);
        shared.unwinnable.fetch_add(partition.placements, std::memory_order_relaxed);
        shared.progress.duplicate_candidates.store(
            shared.unwinnable.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return nullptr;
    }

    const bool use_parallel = allow_parallel && partition.placements >= LARGE_PARTITION_THRESHOLD;
    const size_t key_bytes = shared.space.key_bytes(partition.blocks);
    auto built = std::make_unique<built_partition>(partition.placements, key_bytes);
    built->parallel = use_parallel;
    built->live = std::make_unique<live_memory_guard>(shared, partition.placements);
    // The goal states of the partition are collected while its index is built and
    // remapped to rank ids by build_index(), so neither exploration path has to
    // enumerate the partition again just to find its seeds.
    // Goal states are a fraction of the placements (about a fifth for the
    // shipped rulesets); over-reserving avoids a reallocation in the middle of
    // the enumeration, which would copy the ids collected so far.
    built->goals.reserve(std::max<size_t>(16, partition.placements / 4));

    const auto enumerate_start = clock_now();
    emit_context emit{&built->table, &built->goals, &shared, 0, true};
    bool cancelled = false;
    built->state_count =
        shared.space.enumerate(index, shared.stop, emit_placement, &emit, cancelled);
    if (cancelled) {
        shared.cancelled.store(true, std::memory_order_relaxed);
        return nullptr;
    }
    if (emit.pending != 0) {
        shared.work.fetch_add(emit.pending, std::memory_order_relaxed);
        shared.enumerated.fetch_add(emit.pending, std::memory_order_relaxed);
        emit.pending = 0;
        publish_progress(shared);
    }
    add_time(shared.times.enumerate, enumerate_start);
    if (built->state_count != partition.placements) {
        warnln("Puzzle-space partition {} enumerated {} states, counted {}", partition.blocks,
               built->state_count, partition.placements);
    }
    if (built->state_count == 0) {
        return nullptr;
    }

    if (use_parallel) {
        // Only the giant partitions are reported: the batch workers would
        // otherwise overwrite the size with their much smaller partitions.
        shared.progress.stage_partition_states.store(built->state_count, std::memory_order_relaxed);
    }
    bool index_built = false;
    {
        const stage_scope indexing(shared.progress, worker, puzzle_space_stage::indexing,
                                   built->state_count);
        index_progress_context index_progress{&shared.progress, worker};
        index_built = built->table.build_index(&built->goals, shared.stop, report_index_progress,
                                               &index_progress);
    }
    if (!index_built) {
        if (stop_requested(shared)) {
            return nullptr;
        }
        warnln("Puzzle-space partition {} could not build its perfect hash index",
               partition.blocks);
        return nullptr;
    }
    return built;
}

/**
 * Explores a partition whose index was built by build_partition(). Large
 * partitions use the pool for union/BFS, batch partitions sweep serially.
 */
auto explore_partition(explore_shared& shared, const size_t index, built_partition& built,
                       threadpool pool, const size_t worker) -> void
{
    const placement_partition& partition = shared.space.partition(index);
    const size_t state_count = built.state_count;

    if (!built.parallel) {
        // Batch-mode path: the MPHF and the goal ids are built, the sweep walks
        // one component at a time and carries keys only through its frontiers.
        const auto sweep_start = clock_now();
        std::vector<puzzle_space_cluster> local;
        bool sweep_cancelled = false;
        const size_t visited = sweep_keyed(shared, partition, built.table, built.goals, state_count,
                                           local, sweep_cancelled);
        add_time(shared.times.components, sweep_start);
        if (sweep_cancelled || shared.cancelled.load(std::memory_order_relaxed)) {
            return;
        }
        finish_partition(shared, local, visited, state_count);
        return;
    }

    // 1. Union every legal edge. The candidate set is closed under moves, so
    //    every generated neighbour is a member of this partition.
    const auto union_start = clock_now();
    union_find sets(state_count);
    const size_t missing =
        parallel_union(shared, partition, built.table, sets.parent(), state_count, pool);
    if (missing != 0) {
        warnln("Puzzle-space partition {} had {} moves outside the placement set", partition.blocks,
               missing);
    }
    add_time(shared.times.union_find, union_start);
    if (shared.cancelled.load(std::memory_order_relaxed)) {
        return;
    }

    // 2. Assign compact component ids in place.
    const auto components_start = clock_now();
    for (u32 state = 0; state < state_count; ++state) {
        if ((state & 0x3fff) == 0 && stop_requested(shared)) {
            return;
        }
        sets.parent()[state] = sets.find(state);
    }
    std::vector<u32> root_index(state_count, INVALID_INDEX);
    u32 component_count = 0;
    for (u32 state = 0; state < state_count; ++state) {
        if ((state & 0x3fff) == 0 && stop_requested(shared)) {
            return;
        }
        if (sets.parent()[state] == state) {
            root_index[state] = component_count++;
        }
    }
    for (u32 state = 0; state < state_count; ++state) {
        if ((state & 0x3fff) == 0 && stop_requested(shared)) {
            return;
        }
        sets.parent()[state] = root_index[sets.parent()[state]];
    }
    std::vector<u32>().swap(root_index);
    add_time(shared.times.components, components_start);

    // 3. Multi-source BFS from every goal state of the partition.
    const auto bfs_start = clock_now();
    std::vector<std::int32_t> distances(state_count, -1);
    std::vector<u32> seeds;
    seeds.reserve(built.goals.size());
    for (const u32 goal : built.goals) {
        if (distances[goal] < 0) {
            distances[goal] = 0;
            seeds.push_back(goal);
        }
    }

    if (pool && *pool && (*pool)->get_thread_count() >= 2) {
        parallel_bfs(shared, partition, built.table, distances, std::move(seeds), pool);
    } else {
        serial_bfs(shared, partition, built.table, distances, std::move(seeds));
    }
    if (shared.cancelled.load(std::memory_order_relaxed)) {
        return;
    }
    add_time(shared.times.bfs, bfs_start);

    // 4. Aggregate components. The first pass collects size and hardness; the
    //    second computes the canonical minimum only for components that will be
    //    reported. Unvisited states (unwinnable components) contribute the
    //    remaining third of their work here.
    const auto aggregate_start = clock_now();
    std::vector<size_t> component_size(component_count, 0);
    std::vector<int> component_max(component_count, -1);
    size_t unvisited_pending = 0;
    size_t visited = 0;
    const stage_scope aggregating(shared.progress, worker, puzzle_space_stage::aggregating,
                                  state_count);
    for (u32 state = 0; state < state_count; ++state) {
        if ((state & 0x3fff) == 0) {
            if (stop_requested(shared)) {
                return;
            }
            publish_stage(shared.progress, worker, puzzle_space_stage::aggregating,
                          stage_permille(state, state_count));
        }
        const int distance = distances[state];
        if (distance < 0) {
            ++unvisited_pending;
            if ((unvisited_pending & 0x3fff) == 0) {
                shared.work.fetch_add(unvisited_pending, std::memory_order_relaxed);
                unvisited_pending = 0;
                publish_progress(shared);
            }
            continue;
        }
        ++visited;
        const u32 component = sets.parent()[state];
        ++component_size[component];
        if (distance > component_max[component]) {
            component_max[component] = distance;
        }
    }
    if (unvisited_pending != 0) {
        shared.work.fetch_add(unvisited_pending, std::memory_order_relaxed);
        publish_progress(shared);
    }
    add_time(shared.times.aggregate, aggregate_start);

    const auto repr_start = clock_now();
    std::vector<u32> component_min(component_count, INVALID_INDEX);
    std::vector<std::array<u64, 4>> component_min_repr(component_count);
    std::vector<u32> component_hardest_canonical(component_count, INVALID_INDEX);
    std::vector<std::array<u64, 4>> component_hardest_repr(component_count);
    for (u32 state = 0; state < state_count; ++state) {
        if ((state & 0x3fff) == 0) {
            if (stop_requested(shared)) {
                return;
            }
            publish_stage(shared.progress, worker, puzzle_space_stage::aggregating,
                          stage_permille(state, state_count));
        }
        if (distances[state] < 0) {
            continue;
        }
        const u32 component = sets.parent()[state];
        if (component_max[component] < shared.config.min_moves) {
            continue;
        }
        const std::array<u64, 4> repr = shared.space.state_repr(partition, built.table.key(state));
        if (component_min[component] == INVALID_INDEX ||
            placement_repr_less(repr, component_min_repr[component])) {
            component_min[component] = state;
            component_min_repr[component] = repr;
        }
        // Canonical hardest state: smallest representation at the component's
        // maximum distance, independent of the index layout.
        if (distances[state] == component_max[component] &&
            (component_hardest_canonical[component] == INVALID_INDEX ||
             placement_repr_less(repr, component_hardest_repr[component]))) {
            component_hardest_canonical[component] = state;
            component_hardest_repr[component] = repr;
        }
    }
    add_time(shared.times.repr, repr_start);

    std::vector<puzzle_space_cluster> local;
    for (u32 component = 0; component < component_count; ++component) {
        if (component_size[component] == 0 || component_max[component] < shared.config.min_moves) {
            continue;
        }
        local.push_back(puzzle_space_cluster{
            shared.space.to_puzzle(partition, built.table.key(component_min[component])),
            shared.space.to_puzzle(partition,
                                   built.table.key(component_hardest_canonical[component])),
            component_max[component],
            component_size[component],
            partition.blocks,
            partition.wall_pieces,
        });
    }

    finish_partition(shared, local, visited, state_count);
}

// Runs one partition end to end; used for the partitions of a batch.
auto process_partition(explore_shared& shared, const size_t index, threadpool pool,
                       const bool allow_parallel, const size_t worker) -> void
{
    const active_partition_scope active(shared.progress.active_partitions);
    std::unique_ptr<built_partition> built = build_partition(shared, index, worker, allow_parallel);
    if (!built) {
        return;
    }
    explore_partition(shared, index, *built, pool, worker);
}

/**
 * Starts a batch of partitions on the pool and returns without waiting. Used to
 * keep the workers busy while the search thread builds the index of one giant
 * partition (the only step of a partition that cannot be parallelised).
 */
auto submit_batch(explore_shared& shared, const std::vector<size_t>& parts, threadpool pool)
    -> std::optional<BS::multi_future<void>>
{
    if (!pool || !*pool || parts.empty()) {
        return std::nullopt;
    }
    const size_t workers = std::min(parts.size(), size_t((*pool)->get_thread_count()));
    auto cursor = std::make_shared<std::atomic<size_t>>(0);
    return (*pool)->submit_loop(
        0, workers,
        [&shared, &parts, cursor](const size_t slot)
        {
            while (true) {
                const size_t i = cursor->fetch_add(1, std::memory_order_relaxed);
                if (i >= parts.size()) {
                    break;
                }
                process_partition(shared, parts[i], std::nullopt, false, slot);
            }
        },
        0);
}

/**
 * Keeps the pool busy with the smallest remaining partitions while a giant
 * partition's serial index build runs on the search thread. Workers claim
 * partitions from the back of the queue (where the cheapest ones are) until the
 * range is exhausted or the build finishes; at most one partition per worker is
 * in flight, so the extra memory stays bounded by the pool size.
 */
class helper_drain
{
public:
    helper_drain(explore_shared& shared, const std::vector<size_t>& order, const size_t front,
                 const size_t back, const size_t max_states, threadpool pool)
        : shared_(shared), order_(order), limit_(back), next_(back)
    {
        // Only take partitions that fit the batch budget on their own; the rest
        // are left for the front of the queue.
        while (limit_ > front + 1 &&
               shared.space.partition(order[limit_ - 1]).placements <= max_states) {
            --limit_;
        }
        if (!pool || !*pool || limit_ >= back) {
            return;
        }
        const size_t workers = (*pool)->get_thread_count();
        future_ = (*pool)->submit_loop(
            0, workers,
            [this](const size_t slot)
            {
                while (!stop_.load(std::memory_order_relaxed)) {
                    size_t index = next_.load(std::memory_order_relaxed);
                    bool claimed = false;
                    while (index > limit_) {
                        if (next_.compare_exchange_weak(index, index - 1,
                                                        std::memory_order_relaxed)) {
                            claimed = true;
                            break;
                        }
                    }
                    if (!claimed) {
                        break;
                    }
                    process_partition(shared_, order_[index - 1], std::nullopt, false, slot);
                }
            },
            0);
    }

    helper_drain(const helper_drain&) = delete;
    auto operator=(const helper_drain&) -> helper_drain& = delete;

    // Stops the drain and waits for the claimed partitions; returns the new back
    // index (everything at or above it has been processed).
    auto finish() -> size_t
    {
        stop_.store(true, std::memory_order_relaxed);
        if (future_) {
            future_->wait();
        }
        return next_.load(std::memory_order_relaxed);
    }

private:
    explore_shared& shared_;
    const std::vector<size_t>& order_;
    size_t limit_ = 0;
    std::atomic<size_t> next_{0};
    std::atomic<bool> stop_{false};
    std::optional<BS::multi_future<void>> future_;
};

/**
 * Processes a batch: one partition per worker, each with the serial sweep. A
 * partition large enough for intra-partition parallelism is handed to the whole
 * pool on its own.
 */
auto process_batch(explore_shared& shared, const std::vector<size_t>& batch, threadpool pool,
                   const bool can_parallelize, const size_t thread_count) -> void
{
    std::vector<size_t> pending = batch; // already sorted by descending size
    while (!pending.empty()) {
        if (stop_requested(shared)) {
            return;
        }
        if (pending.size() == 1) {
            if (can_parallelize &&
                shared.space.partition(pending.front()).placements >= LARGE_PARTITION_THRESHOLD) {
                process_partition(shared, pending.front(), pool, true, thread_count);
            } else {
                run_parallel(
                    pool, 1, [&](const size_t slot)
                    { process_partition(shared, pending.front(), std::nullopt, false, slot); });
            }
            return;
        }

        const size_t largest = shared.space.partition(pending.front()).placements;
        if (can_parallelize && pending.size() < thread_count / 2 && largest >= 1'000'000) {
            // The largest partition needs the whole pool for its union/BFS, but
            // its index build is serial. Run the rest of the batch on the pool
            // while that build happens.
            const size_t head = pending.front();
            const std::vector<size_t> helpers(pending.begin() + 1, pending.end());
            std::optional<BS::multi_future<void>> helpers_future;
            if (!helpers.empty()) {
                helpers_future = submit_batch(shared, helpers, pool);
            }
            const active_partition_scope active(shared.progress.active_partitions);
            std::unique_ptr<built_partition> built =
                build_partition(shared, head, thread_count, true);
            if (helpers_future) {
                helpers_future->wait();
            }
            if (built) {
                explore_partition(shared, head, *built, pool, thread_count);
            }
            return;
        }

        run_parallel(pool, pending.size(), [&](const size_t slot)
                     { process_partition(shared, pending[slot], std::nullopt, false, slot); });
        return;
    }
}

} // namespace

auto sort_puzzle_space_clusters(std::vector<puzzle_space_cluster>& clusters,
                                const puzzle_space_sort sort) -> void
{
    std::ranges::sort(clusters,
                      [sort](const puzzle_space_cluster& a, const puzzle_space_cluster& b)
                      {
                          if (sort == puzzle_space_sort::most_blocks_fewest_states_most_moves) {
                              if (a.block_count != b.block_count) {
                                  return a.block_count > b.block_count;
                              }
                              if (a.state_count != b.state_count) {
                                  return a.state_count < b.state_count;
                              }
                              if (a.max_moves != b.max_moves) {
                                  return a.max_moves > b.max_moves;
                              }
                              return a.cluster_id < b.cluster_id;
                          }

                          if (a.max_moves != b.max_moves) {
                              return a.max_moves > b.max_moves;
                          }
                          if (a.state_count != b.state_count) {
                              return a.state_count > b.state_count;
                          }
                          return a.cluster_id < b.cluster_id;
                      });
}

auto make_puzzle_space_config(const puzzle& board, const size_t max_blocks, const int min_moves,
                              const size_t min_blocks, const wall_search_options& wall_options)
    -> puzzle_space_config_result
{
    puzzle_space_config_result result;

    if (const auto reason = board.try_get_invalid_reason()) {
        result.error = *reason;
        return result;
    }
    if (!board.get_goal()) {
        result.error = "The board has no goal";
        return result;
    }

    const std::optional<block> target = board.try_get_target_block();
    if (!target) {
        result.error = "The board has no target block";
        return result;
    }
    if (target->get_immovable()) {
        result.error = "The target block is immovable";
        return result;
    }

    const int width = board.get_width();
    const int height = board.get_height();
    const int target_width = target->get_width();
    const int target_height = target->get_height();
    if (target_width > width || target_height > height) {
        result.error = "The target block does not fit on the board";
        return result;
    }

    if (board.get_goal_x() >= width || board.get_goal_y() >= height) {
        result.error = "The goal is outside the board";
        return result;
    }
    if (board.wall_at(static_cast<u8>(board.get_goal_x()), static_cast<u8>(board.get_goal_y()))) {
        result.error = "The goal is covered by a wall";
        return result;
    }

    blockset2 inventory;
    for (const block b : board.block_view()) {
        if (b.get_target() || b.get_immovable()) {
            continue;
        }
        inventory.insert(block(0, 0, b.get_width(), b.get_height(), false, false));
    }
    if (inventory.empty()) {
        result.error = "Add movable non-target blocks to define an inventory";
        return result;
    }

    const size_t available_blocks = movable_block_count(board);
    size_t blocks = max_blocks == 0 ? available_blocks : max_blocks;
    if (blocks < 1) {
        result.error = "At least one block is required";
        return result;
    }
    blocks = std::min(blocks, static_cast<size_t>(puzzle::MAX_BLOCKS));

    // A partition always contains the target plus at least one movable block,
    // so a minimum below 2 cannot change the result.
    size_t blocks_min = std::max<size_t>(1, min_blocks);
    if (blocks < 2) {
        // A target-only space has no partitions; keep the configuration valid
        // so it still reports an empty result instead of an error.
        blocks_min = blocks;
    } else if (blocks_min > blocks) {
        result.error = std::format("Minimum blocks ({}) cannot exceed maximum blocks ({})",
                                   blocks_min, blocks);
        return result;
    }

    const int goal_x = board.get_goal_x();
    const int goal_y = board.get_goal_y();
    u8 x0 = 0;
    u8 y0 = 0;
    u8 x1 = static_cast<u8>(width - target_width);
    u8 y1 = static_cast<u8>(height - target_height);

    if (board.get_restricted() && target_width != target_height) {
        if (target_width > target_height) {
            // Horizontal target: it can only move along its row.
            if (goal_y + target_height > height) {
                result.error = "The target does not fit in the goal row";
                return result;
            }
            y0 = y1 = static_cast<u8>(goal_y);
        } else {
            // Vertical target: it can only move along its column.
            if (goal_x + target_width > width) {
                result.error = "The target does not fit in the goal column";
                return result;
            }
            x0 = x1 = static_cast<u8>(goal_x);
        }
    }

    puzzle_space_config config;
    config.width = static_cast<u8>(width);
    config.height = static_cast<u8>(height);
    config.goal_x = static_cast<u8>(goal_x);
    config.goal_y = static_cast<u8>(goal_y);
    config.restricted = board.get_restricted();
    if (wall_options.inventory) {
        const std::vector<block> rectangles = board.wall_rectangles();
        if (rectangles.empty()) {
            result.error = "The board has no wall shapes to use as an inventory";
            return result;
        }
        blockset2 wall_inventory;
        for (const block shape : rectangles) {
            wall_inventory.insert(block(0, 0, shape.get_width(), shape.get_height(), false, false));
        }
        const size_t cell_limit = static_cast<size_t>(width) * height;
        const size_t max_walls = std::min(wall_options.max_walls, cell_limit);
        if (wall_options.min_walls > max_walls) {
            result.error = std::format("Minimum walls ({}) cannot exceed maximum walls ({})",
                                       wall_options.min_walls, max_walls);
            return result;
        }
        config.wall_inventory_enabled = true;
        config.wall_inventory = std::move(wall_inventory);
        config.min_walls = wall_options.min_walls;
        config.max_walls = max_walls;
        config.walls = 0;
    } else {
        config.walls = board.wall_mask();
    }
    config.target = *target;
    config.inventory = std::move(inventory);
    config.target_positions = {x0, y0, x1, y1};
    config.max_blocks = blocks;
    config.min_blocks = blocks_min;
    config.min_moves = std::max(0, min_moves);

    result.config = std::move(config);
    return result;
}

auto run_puzzle_space_search(const puzzle_space_config& config, puzzle_space_progress* progress,
                             const std::stop_token stop, const threadpool pool,
                             const size_t batch_state_budget) -> puzzle_space_result
{
    puzzle_space_progress local_progress;
    if (progress == nullptr) {
        progress = &local_progress;
    }

    std::stop_source internal_stop;
    std::stop_callback forward_stop(stop, [&] { internal_stop.request_stop(); });
    const std::stop_token token = internal_stop.get_token();

    puzzle_space_result result;
    result.config = config;
    const auto search_start = std::chrono::steady_clock::now();

    const auto set_phase = [&](const puzzle_space_phase phase)
    { progress->phase.store(phase, std::memory_order_relaxed); };

    if (config.max_blocks < 1 || config.min_blocks < 1 || config.min_blocks > config.max_blocks ||
        config.inventory.empty() ||
        (config.wall_inventory_enabled &&
         (config.wall_inventory.empty() || config.min_walls > config.max_walls))) {
        result.error = "Invalid puzzle-space configuration";
        set_phase(puzzle_space_phase::failed);
        return result;
    }

    infoln("Puzzle-space search started: {}x{} board, goal ({},{}), {}, target {}x{}, blocks "
           "{}..{}, min {} moves",
           config.width, config.height, config.goal_x, config.goal_y,
           config.restricted ? "restricted" : "free", config.target.get_width(),
           config.target.get_height(), config.min_blocks, config.max_blocks, config.min_moves);

    set_phase(puzzle_space_phase::inventories);
    placement_space space(config, token);
    if (token.stop_requested()) {
        result.cancelled = true;
        set_phase(puzzle_space_phase::cancelled);
        return result;
    }
    if (!space.error().empty()) {
        result.error = space.error();
        set_phase(puzzle_space_phase::failed);
        return result;
    }
    std::vector<placement_partition>& partitions = space.partitions();
    progress->inventories.store(partitions.size(), std::memory_order_relaxed);
    progress->wall_layouts.store(space.wall_layout_count(), std::memory_order_relaxed);
    progress->starting_positions.store(0, std::memory_order_relaxed);
    progress->fixed_bytes.store(space.fixed_bytes(), std::memory_order_relaxed);

    traceln("Puzzle-space phase inventories: {} partitions, {} fixed bytes", partitions.size(),
            format_memory(space.fixed_bytes()));

    // ---------------------------------------------------------------------
    // Exact counting pass
    // ---------------------------------------------------------------------
    const auto count_start = std::chrono::steady_clock::now();
    set_phase(puzzle_space_phase::counting);

    std::atomic<size_t> total_placements{0};
    std::atomic<size_t> partitions_with_goal{0};
    std::atomic<bool> count_cancelled{false};
    progress->counted_partitions.store(0, std::memory_order_relaxed);
    progress->counted_placements.store(0, std::memory_order_relaxed);

    run_parallel(pool, partitions.size(),
                 [&](const size_t index)
                 {
                     if (token.stop_requested()) {
                         count_cancelled.store(true, std::memory_order_relaxed);
                         return;
                     }
                     bool cancelled = false;
                     const auto [count, has_goal] = space.count(index, token, cancelled);
                     if (cancelled) {
                         count_cancelled.store(true, std::memory_order_relaxed);
                         return;
                     }
                     partitions[index].placements = count;
                     partitions[index].has_goal = has_goal;
                     total_placements.fetch_add(count, std::memory_order_relaxed);
                     if (has_goal) {
                         partitions_with_goal.fetch_add(1, std::memory_order_relaxed);
                     }
                     progress->counted_placements.fetch_add(count, std::memory_order_relaxed);
                     progress->counted_partitions.fetch_add(1, std::memory_order_relaxed);
                 });

    progress->candidates_total.store(total_placements.load(std::memory_order_relaxed),
                                     std::memory_order_relaxed);
    progress->probe_candidates.store(partitions.size(), std::memory_order_relaxed);
    progress->probe_unique.store(partitions_with_goal.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
    progress->probe_eta_seconds.store(0, std::memory_order_relaxed);
    progress->eta_seconds.store(0, std::memory_order_relaxed);

    traceln(
        "Puzzle-space phase counting: {:.2f}ms ({} placements, {} partitions with goals)",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - count_start)
            .count(),
        format_count(total_placements.load()), partitions_with_goal.load());

    if (count_cancelled.load(std::memory_order_relaxed) || token.stop_requested()) {
        result.cancelled = true;
        set_phase(puzzle_space_phase::cancelled);
        return result;
    }

    if (total_placements.load(std::memory_order_relaxed) == 0) {
        set_phase(puzzle_space_phase::done);
        return result;
    }

    // ---------------------------------------------------------------------
    // Memory projection and batch plan
    // ---------------------------------------------------------------------
    const size_t key_bytes = space.key_bytes(config.max_blocks);
    // Calibrated for the two-bit sweep: keys + index + two bitsets + the
    // component/frontier working set + the collected goal ids (4 B per goal, and
    // at most about a fifth of the states are goals for the shipped rulesets).
    // The giant-partition path uses a little more (union-find, distances), which
    // the 10% projection margin covers.
    const size_t bytes_per_state = key_bytes + 16;

    std::vector<size_t> order(partitions.size());
    std::iota(order.begin(), order.end(), size_t{0});
    std::ranges::sort(order, [&](const size_t a, const size_t b)
                      { return partitions[a].placements > partitions[b].placements; });

    // Partitions are processed largest first. Consecutive partitions are grouped
    // into a batch while they fit BATCH_STATE_BUDGET; a partition that alone
    // exceeds the budget runs with intra-partition parallelism and keeps the
    // pool busy with the smallest remaining partitions while its serial index is
    // built.
    const bool can_parallelize = pool && *pool && (*pool)->get_thread_count() > 1;
    const size_t thread_count = can_parallelize ? (*pool)->get_thread_count() : 1;
    const size_t total = total_placements.load(std::memory_order_relaxed);
    const size_t batch_budget = batch_state_budget > 0 ? batch_state_budget : BATCH_STATE_BUDGET;

    const size_t largest_partition = order.empty() ? 0 : partitions[order.front()].placements;
    // While a giant partition builds its index, the pool works on smaller ones,
    // one per worker. The projection covers the giant plus one batch worth of
    // helpers; the overlay is the authoritative live number.
    const size_t peak_states = std::min(total, largest_partition + batch_budget);

    const size_t fixed_bytes = space.fixed_bytes();
    const size_t estimated_peak = (fixed_bytes + peak_states * bytes_per_state) * 11 / 10;

    // The live figures start at zero and are published per running partition.
    // The one-time estimate also seeds the projected peak until the observed
    // in-flight peak overtakes it.
    progress->cache_bytes.store(0, std::memory_order_relaxed);
    progress->active_bytes.store(0, std::memory_order_relaxed);
    progress->cache_states.store(0, std::memory_order_relaxed);
    progress->max_active_states.store(0, std::memory_order_relaxed);
    progress->live_states.store(0, std::memory_order_relaxed);
    progress->peak_live_states.store(0, std::memory_order_relaxed);
    progress->probe_peak_bytes.store(estimated_peak, std::memory_order_relaxed);
    progress->projected_peak_bytes.store(estimated_peak, std::memory_order_relaxed);
    progress->estimated_peak_bytes.store(estimated_peak, std::memory_order_relaxed);

    infoln("Puzzle-space search: {} partitions, {} placements, {} per state, projected peak {}",
           partitions.size(), format_count(total), format_memory(bytes_per_state),
           format_gb(estimated_peak));

    // ---------------------------------------------------------------------
    // Exploration: one partition at a time per worker, no shared cache
    // ---------------------------------------------------------------------
    const auto explore_start = std::chrono::steady_clock::now();
    set_phase(puzzle_space_phase::exploring);
    for (std::atomic<u64>& slot : progress->stage_slots) {
        slot.store(0, std::memory_order_relaxed);
    }
    for (std::atomic<size_t>& states : progress->stage_slot_states) {
        states.store(0, std::memory_order_relaxed);
    }

    std::mutex result_mtx;
    std::vector<puzzle_space_cluster> clusters;
    std::atomic<size_t> done{0};
    std::atomic<size_t> winnable{0};
    std::atomic<size_t> unwinnable{0};
    std::atomic<size_t> found{0};
    std::atomic<bool> explore_cancelled{false};
    std::atomic<size_t> work{0};
    std::atomic<size_t> enumerated{0};
    phase_times times;

    explore_shared shared{
        space,     config,   token,      result_mtx,    clusters,
        done,      winnable, unwinnable, found,         explore_cancelled,
        *progress, work,     enumerated, explore_start, times,
    };
    shared.bytes_per_state = bytes_per_state;

    size_t front = 0;
    size_t back = order.size();
    while (front < back) {
        if (stop_requested(shared)) {
            break;
        }

        size_t end = front;
        size_t sum = 0;
        while (end < back) {
            const size_t placements = partitions[order[end]].placements;
            if (end > front && sum + placements > batch_budget) {
                break;
            }
            sum += placements;
            ++end;
            if (sum >= batch_budget) {
                break;
            }
        }
        if (end == front) {
            ++end;
        }

        const bool alone = end == front + 1;
        if (!can_parallelize || !alone || sum <= batch_budget) {
            // Normal batch: every partition gets its own worker, so the index
            // builds of the whole batch run concurrently.
            const std::vector<size_t> batch(order.begin() + static_cast<std::ptrdiff_t>(front),
                                            order.begin() + static_cast<std::ptrdiff_t>(end));
            process_batch(shared, batch, pool, can_parallelize, thread_count);
            front = end;
            continue;
        }

        // This partition alone exceeds the batch budget: its index build is
        // serial, so keep the pool busy with the smallest remaining partitions
        // for as long as the build takes.
        const size_t head = order[front];
        std::optional<helper_drain> drain;
        drain.emplace(shared, order, front, back, batch_budget, pool);

        {
            const active_partition_scope active(progress->active_partitions);
            std::unique_ptr<built_partition> built =
                build_partition(shared, head, thread_count, true);
            back = drain->finish();
            if (built) {
                explore_partition(shared, head, *built, pool, thread_count);
            }
        }

        front += 1;
    }

    if (explore_cancelled.load(std::memory_order_relaxed) || token.stop_requested()) {
        result.cancelled = true;
        set_phase(puzzle_space_phase::cancelled);
        infoln(
            "Puzzle-space search cancelled after {:.2f}s",
            std::chrono::duration<double>(std::chrono::steady_clock::now() - search_start).count());
        return result;
    }

    // ---------------------------------------------------------------------
    // Finish
    // ---------------------------------------------------------------------
    set_phase(puzzle_space_phase::finishing);
    result.clusters = std::move(clusters);
    sort_puzzle_space_clusters(result.clusters, puzzle_space_sort::most_moves);

    progress->candidates_done.store(total_placements.load(std::memory_order_relaxed),
                                    std::memory_order_relaxed);
    progress->states_explored.store(total_placements.load(std::memory_order_relaxed),
                                    std::memory_order_relaxed);
    progress->clusters_found.store(result.clusters.size(), std::memory_order_relaxed);
    progress->result_bytes.store(result.clusters.size() *
                                     (sizeof(puzzle_space_cluster) + 2 * sizeof(puzzle) + 32),
                                 std::memory_order_relaxed);
    progress->eta_seconds.store(0, std::memory_order_relaxed);

    const double total_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - search_start).count();
    infoln("Puzzle-space search finished: {} clusters with at least {} moves, {} placements, "
           "{:.2f}s",
           result.clusters.size(), config.min_moves, format_count(done.load()), total_seconds);

    if (log_enabled(2)) {
        const auto ms = [](const std::atomic<std::int64_t>& value)
        { return static_cast<double>(value.load(std::memory_order_relaxed)) / 1'000'000.0; };
        const double total_ms = ms(times.enumerate) + ms(times.union_find) + ms(times.components) +
            ms(times.bfs) + ms(times.aggregate) + ms(times.repr);
        const auto share = [&](const std::atomic<std::int64_t>& value)
        { return total_ms > 0.0 ? 100.0 * ms(value) / total_ms : 0.0; };
        traceln("Puzzle-space phase totals (CPU): enumerate {:.0f}ms ({:.0f}%), union {:.0f}ms "
                "({:.0f}%), components {:.0f}ms ({:.0f}%), bfs {:.0f}ms ({:.0f}%), aggregate "
                "{:.0f}ms ({:.0f}%), repr {:.0f}ms ({:.0f}%)",
                ms(times.enumerate), share(times.enumerate), ms(times.union_find),
                share(times.union_find), ms(times.components), share(times.components),
                ms(times.bfs), share(times.bfs), ms(times.aggregate), share(times.aggregate),
                ms(times.repr), share(times.repr));
    }

    set_phase(puzzle_space_phase::done);
    return result;
}
