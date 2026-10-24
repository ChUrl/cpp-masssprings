#include "hanoi_model.hpp"

#include <algorithm>
#include <format>
#include <random>
#include <stdexcept>

hanoi_model::hanoi_model()
{
    configure(DEFAULT_DISKS, 2);
}

hanoi_model::hanoi_model(const int disks, const int target_peg)
{
    configure(disks, target_peg);
}

auto hanoi_model::configure(const int disks, const int target_peg) -> void
{
    if (disks < MIN_DISKS || disks > MAX_DISKS) {
        throw std::invalid_argument("Hanoi disk count out of bounds");
    }
    if (target_peg < 0 || target_peg >= PEG_COUNT) {
        throw std::invalid_argument("Hanoi target peg out of bounds");
    }

    disk_count_ = disks;
    target_peg_ = target_peg;
    states.clear();
    indices.clear();
}

auto hanoi_model::disk_count() const -> int
{
    return disk_count_;
}

auto hanoi_model::target_peg() const -> int
{
    return target_peg_;
}

auto hanoi_model::optimal_moves() const -> std::uint32_t
{
    return (1u << static_cast<unsigned int>(disk_count_)) - 1u;
}

auto hanoi_model::state_count() const -> size_t
{
    return states.size();
}

auto hanoi_model::memory_bytes() const -> size_t
{
    constexpr size_t entry_overhead = 32;
    return states.capacity() * sizeof(std::uint32_t) +
        indices.size() * (sizeof(std::uint32_t) + sizeof(size_t) + entry_overhead);
}

auto hanoi_model::is_goal(const state_id id) const -> bool
{
    if (id >= states.size()) {
        return false;
    }

    const std::uint32_t state = states[id];
    for (int disk = 0; disk < disk_count_; ++disk) {
        if (peg_from_code(state, disk) != target_peg_) {
            return false;
        }
    }
    return true;
}

auto hanoi_model::is_valid(const state_id id) const -> bool
{
    if (id >= states.size()) {
        return false;
    }

    const std::uint32_t state = states[id];
    for (int disk = 0; disk < disk_count_; ++disk) {
        if (peg_from_code(state, disk) >= PEG_COUNT) {
            return false;
        }
    }
    return true;
}

auto hanoi_model::describe(const state_id id) const -> std::string
{
    return std::format("H:[{}] T:[{}] B:[{}]", disk_count_, target_peg_, state_string(code(id)));
}

auto hanoi_model::initial_state() -> state_id
{
    if (states.empty()) {
        return find_or_insert(0);
    }
    return 0;
}

auto hanoi_model::reset_to(const state_id id) -> state_id
{
    const std::uint32_t state = code(id);
    states.clear();
    indices.clear();
    return find_or_insert(state);
}

auto hanoi_model::clone() const -> std::unique_ptr<state_model>
{
    return std::make_unique<hanoi_model>(*this);
}

auto hanoi_model::explore(const state_id start, const state_id current, const threadpool pool,
                          exploration_observer* observer) -> exploration_result
{
    (void)pool;

    const std::uint32_t start_code = code(start);
    const std::uint32_t current_code = code(current);

    states.clear();
    indices.clear();

    const state_id start_id = insert_code(start_code).first;

    std::vector<state_id> queue;
    queue.reserve(1u << 16);
    queue.push_back(start_id);

    std::vector<spring> links;
    links.reserve(1u << 17);

    size_t head = 0;
    while (head < queue.size()) {
        if (observer != nullptr && (head & 0xFFFu) == 0 &&
            !observer->on_states_explored(queue.size())) {
            exploration_result result;
            result.links = std::move(links);
            result.start = start_id;
            result.current = start_id;
            result.complete = false;
            result.cancelled = true;
            return result;
        }

        const state_id id = queue[head++];
        const std::uint32_t state = states[id];

        for_each_legal_move(state,
                            [&](const std::uint32_t next_state)
                            {
                                const auto [next_id, inserted] = insert_code(next_state);
                                if (inserted) {
                                    queue.push_back(next_id);
                                }

                                if (id < next_id) {
                                    links.emplace_back(id, next_id);
                                }
                            });
    }

    // The current state is normally reached by the BFS. Inserting it again is harmless and keeps
    // the model usable if a caller supplies a state outside the explored component.
    const state_id current_id = insert_code(current_code).first;

    exploration_result result;
    result.links = std::move(links);
    result.start = start_id;
    result.current = current_id;
    result.complete = true;
    result.directed = false;
    return result;
}

auto hanoi_model::code(const state_id id) const -> std::uint32_t
{
    return states.at(id);
}

auto hanoi_model::peg(const state_id id, const int disk) const -> int
{
    return peg_from_code(code(id), disk);
}

auto hanoi_model::top_disk(const state_id id, const int peg_index) const -> int
{
    if (peg_index < 0 || peg_index >= PEG_COUNT) {
        return -1;
    }

    const std::uint32_t state = code(id);
    for (int disk = 0; disk < disk_count_; ++disk) {
        if (peg_from_code(state, disk) == peg_index) {
            return disk;
        }
    }
    return -1;
}

auto hanoi_model::disks_on_target(const state_id id) const -> int
{
    const std::uint32_t state = code(id);
    int count = 0;
    for (int disk = 0; disk < disk_count_; ++disk) {
        if (peg_from_code(state, disk) == target_peg_) {
            ++count;
        }
    }
    return count;
}

auto hanoi_model::find_or_insert(const std::uint32_t state) -> state_id
{
    return insert_code(state).first;
}

auto hanoi_model::try_move(const state_id id, const int from_peg, const int to_peg)
    -> std::optional<state_id>
{
    if (from_peg < 0 || from_peg >= PEG_COUNT || to_peg < 0 || to_peg >= PEG_COUNT ||
        from_peg == to_peg) {
        return std::nullopt;
    }

    const int moving = top_disk(id, from_peg);
    if (moving < 0) {
        return std::nullopt;
    }

    const int destination = top_disk(id, to_peg);
    if (destination >= 0 && destination < moving) {
        return std::nullopt;
    }

    return find_or_insert(with_peg(code(id), moving, to_peg));
}

auto hanoi_model::random_state() -> state_id
{
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> peg_distribution(0, PEG_COUNT - 1);

    std::uint32_t state = 0;
    for (int disk = 0; disk < disk_count_; ++disk) {
        state = with_peg(state, disk, peg_distribution(rng));
    }

    bool solved = true;
    for (int disk = 0; disk < disk_count_; ++disk) {
        if (peg_from_code(state, disk) != target_peg_) {
            solved = false;
            break;
        }
    }
    if (solved) {
        state = with_peg(state, 0, (peg_from_code(state, 0) + 1) % PEG_COUNT);
    }

    return find_or_insert(state);
}

auto hanoi_model::solved_state() -> state_id
{
    std::uint32_t state = 0;
    for (int disk = 0; disk < disk_count_; ++disk) {
        state = with_peg(state, disk, target_peg_);
    }
    return find_or_insert(state);
}

auto hanoi_model::start_state() -> state_id
{
    return find_or_insert(0);
}

auto hanoi_model::peg_mask(const int disk) -> std::uint32_t
{
    return 3u << (2u * static_cast<unsigned int>(disk));
}

auto hanoi_model::peg_from_code(const std::uint32_t state, const int disk) -> int
{
    return static_cast<int>((state >> (2u * static_cast<unsigned int>(disk))) & 3u);
}

auto hanoi_model::with_peg(const std::uint32_t state, const int disk, const int peg_index) const
    -> std::uint32_t
{
    const std::uint32_t mask = peg_mask(disk);
    return (state & ~mask) |
        (static_cast<std::uint32_t>(peg_index) << (2u * static_cast<unsigned int>(disk)));
}

auto hanoi_model::insert_code(const std::uint32_t state) -> std::pair<state_id, bool>
{
    if (const auto it = indices.find(state); it != indices.end()) {
        return {it->second, false};
    }

    const state_id id = states.size();
    states.push_back(state);
    indices.emplace(state, id);
    return {id, true};
}

auto hanoi_model::state_string(const std::uint32_t state) const -> std::string
{
    std::string result;
    result.reserve(static_cast<size_t>(disk_count_));
    for (int disk = 0; disk < disk_count_; ++disk) {
        result.push_back(static_cast<char>('0' + peg_from_code(state, disk)));
    }
    return result;
}
