#include "eight_puzzle_model.hpp"

#include <array>
#include <cstdlib>
#include <format>
#include <random>

eight_puzzle_model::eight_puzzle_model()
{
    configure();
}

auto eight_puzzle_model::configure() -> void
{
    states.clear();
    indices.clear();
}

auto eight_puzzle_model::state_count() const -> size_t
{
    return states.size();
}

auto eight_puzzle_model::memory_bytes() const -> size_t
{
    constexpr size_t entry_overhead = 32;
    return states.capacity() * sizeof(std::uint64_t) +
        indices.size() * (sizeof(std::uint64_t) + sizeof(size_t) + entry_overhead);
}

auto eight_puzzle_model::is_goal(const state_id id) const -> bool
{
    return id < states.size() && states[id] == GOAL_CODE;
}

auto eight_puzzle_model::is_valid(const state_id id) const -> bool
{
    if (id >= states.size()) {
        return false;
    }

    std::uint16_t seen = 0;
    const std::uint64_t state = states[id];
    for (int cell = 0; cell < CELLS; ++cell) {
        const int value = value_from_code(state, cell);
        if (value < 0 || value > TILE_COUNT) {
            return false;
        }
        const std::uint16_t bit = static_cast<std::uint16_t>(1u << value);
        if ((seen & bit) != 0) {
            return false;
        }
        seen |= bit;
    }

    return seen == 0x1FFu;
}

auto eight_puzzle_model::describe(const state_id id) const -> std::string
{
    return std::format("P8:[{}x{}] B:[{}]", SIZE, SIZE, state_string(code(id)));
}

auto eight_puzzle_model::initial_state() -> state_id
{
    if (states.empty()) {
        return find_or_insert(DEFAULT_START_CODE);
    }
    return 0;
}

auto eight_puzzle_model::reset_to(const state_id id) -> state_id
{
    const std::uint64_t state = code(id);
    states.clear();
    indices.clear();
    return find_or_insert(state);
}

auto eight_puzzle_model::clone() const -> std::unique_ptr<state_model>
{
    return std::make_unique<eight_puzzle_model>(*this);
}

auto eight_puzzle_model::explore(const state_id start, const state_id current,
                                 const threadpool pool, exploration_observer* observer)
    -> exploration_result
{
    (void)pool;

    const std::uint64_t start_code = code(start);
    const std::uint64_t current_code = code(current);

    states.clear();
    indices.clear();

    const state_id start_id = insert_code(start_code).first;

    std::vector<state_id> queue;
    queue.reserve(1u << 18);
    queue.push_back(start_id);

    std::vector<spring> links;
    links.reserve(1u << 19);

    constexpr std::array<std::pair<int, int>, 4> directions = {std::pair{0, -1}, std::pair{1, 0},
                                                               std::pair{0, 1}, std::pair{-1, 0}};

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
        const std::uint64_t state = states[id];
        const int blank = blank_from_code(state);
        const int blank_x = blank % SIZE;
        const int blank_y = blank / SIZE;

        for (const auto& [dx, dy] : directions) {
            const int x = blank_x + dx;
            const int y = blank_y + dy;
            if (x < 0 || x >= SIZE || y < 0 || y >= SIZE) {
                continue;
            }

            const int target = y * SIZE + x;
            const std::uint64_t next_state = swap_cells(state, blank, target);
            const auto [next_id, inserted] = insert_code(next_state);
            if (inserted) {
                queue.push_back(next_id);
            }
            if (id < next_id) {
                links.emplace_back(id, next_id);
            }
        }
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

auto eight_puzzle_model::code(const state_id id) const -> std::uint64_t
{
    return states.at(id);
}

auto eight_puzzle_model::value(const state_id id, const int cell) const -> int
{
    return value_from_code(code(id), cell);
}

auto eight_puzzle_model::blank_cell(const state_id id) const -> int
{
    return blank_from_code(code(id));
}

auto eight_puzzle_model::manhattan(const state_id id) const -> int
{
    const std::uint64_t state = code(id);
    int distance = 0;
    for (int cell = 0; cell < CELLS; ++cell) {
        const int value = value_from_code(state, cell);
        if (value == 0) {
            continue;
        }
        const int goal = value - 1;
        distance += std::abs(cell % SIZE - goal % SIZE) + std::abs(cell / SIZE - goal / SIZE);
    }
    return distance;
}

auto eight_puzzle_model::is_solvable(const state_id id) const -> bool
{
    return is_solvable_code(code(id));
}

auto eight_puzzle_model::find_or_insert(const std::uint64_t state) -> state_id
{
    return insert_code(state).first;
}

auto eight_puzzle_model::try_move_tile(const state_id id, const int cell) -> std::optional<state_id>
{
    if (cell < 0 || cell >= CELLS) {
        return std::nullopt;
    }
    if (value(id, cell) == 0) {
        return std::nullopt;
    }

    const int blank = blank_cell(id);
    if (blank < 0 ||
        std::abs(cell % SIZE - blank % SIZE) + std::abs(cell / SIZE - blank / SIZE) != 1) {
        return std::nullopt;
    }

    return find_or_insert(swap_cells(code(id), blank, cell));
}

auto eight_puzzle_model::try_move_blank(const state_id id, const int dx, const int dy)
    -> std::optional<state_id>
{
    const int blank = blank_cell(id);
    if (blank < 0) {
        return std::nullopt;
    }
    const int x = blank % SIZE + dx;
    const int y = blank / SIZE + dy;
    if (x < 0 || x >= SIZE || y < 0 || y >= SIZE) {
        return std::nullopt;
    }

    return find_or_insert(swap_cells(code(id), blank, y * SIZE + x));
}

auto eight_puzzle_model::random_state() -> state_id
{
    std::mt19937 rng(std::random_device{}());
    constexpr std::array<std::pair<int, int>, 4> directions = {std::pair{0, -1}, std::pair{1, 0},
                                                               std::pair{0, 1}, std::pair{-1, 0}};

    std::uint64_t state = GOAL_CODE;
    int previous_blank = -1;

    for (int step = 0; step < 200; ++step) {
        const int blank = blank_from_code(state);
        const int blank_x = blank % SIZE;
        const int blank_y = blank / SIZE;

        std::array<int, 4> candidates{};
        int candidate_count = 0;
        for (const auto& [dx, dy] : directions) {
            const int x = blank_x + dx;
            const int y = blank_y + dy;
            if (x < 0 || x >= SIZE || y < 0 || y >= SIZE) {
                continue;
            }
            const int target = y * SIZE + x;
            if (target == previous_blank) {
                continue;
            }
            candidates[static_cast<size_t>(candidate_count++)] = target;
        }

        if (candidate_count == 0) {
            continue;
        }

        std::uniform_int_distribution<int> candidate_distribution(0, candidate_count - 1);
        const int target = candidates[static_cast<size_t>(candidate_distribution(rng))];
        previous_blank = blank;
        state = swap_cells(state, blank, target);
    }

    if (state == GOAL_CODE) {
        const int blank = blank_from_code(state);
        const int target = blank == CELLS - 1 ? blank - 1 : blank + 1;
        state = swap_cells(state, blank, target);
    }

    return find_or_insert(state);
}

auto eight_puzzle_model::solved_state() -> state_id
{
    return find_or_insert(GOAL_CODE);
}

auto eight_puzzle_model::default_state() -> state_id
{
    return find_or_insert(DEFAULT_START_CODE);
}

auto eight_puzzle_model::is_solvable_code(const std::uint64_t state) -> bool
{
    std::array<int, CELLS> tiles{};
    int count = 0;
    for (int cell = 0; cell < CELLS; ++cell) {
        const int value = value_from_code(state, cell);
        if (value != 0) {
            tiles[static_cast<size_t>(count++)] = value;
        }
    }

    int inversions = 0;
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (tiles[static_cast<size_t>(i)] > tiles[static_cast<size_t>(j)]) {
                ++inversions;
            }
        }
    }

    // The 3x3 puzzle has odd width, so a permutation is solvable iff its inversion count is even.
    return inversions % 2 == 0;
}

auto eight_puzzle_model::value_from_code(const std::uint64_t state, const int cell) -> int
{
    return static_cast<int>((state >> (4u * static_cast<unsigned int>(cell))) & 0xFu);
}

auto eight_puzzle_model::blank_from_code(const std::uint64_t state) -> int
{
    for (int cell = 0; cell < CELLS; ++cell) {
        if (value_from_code(state, cell) == 0) {
            return cell;
        }
    }
    return -1;
}

auto eight_puzzle_model::swap_cells(const std::uint64_t state, const int first, const int second)
    -> std::uint64_t
{
    const std::uint64_t first_value = static_cast<std::uint64_t>(value_from_code(state, first));
    const std::uint64_t second_value = static_cast<std::uint64_t>(value_from_code(state, second));

    const std::uint64_t first_mask = std::uint64_t{0xF} << (4u * static_cast<unsigned int>(first));
    const std::uint64_t second_mask = std::uint64_t{0xF}
        << (4u * static_cast<unsigned int>(second));

    return (state & ~first_mask & ~second_mask) |
        (second_value << (4u * static_cast<unsigned int>(first))) |
        (first_value << (4u * static_cast<unsigned int>(second)));
}

auto eight_puzzle_model::insert_code(const std::uint64_t state) -> std::pair<state_id, bool>
{
    if (const auto it = indices.find(state); it != indices.end()) {
        return {it->second, false};
    }

    const state_id id = states.size();
    states.push_back(state);
    indices.emplace(state, id);
    return {id, true};
}

auto eight_puzzle_model::state_string(const std::uint64_t state) const -> std::string
{
    std::string result;
    result.reserve(CELLS);
    for (int cell = 0; cell < CELLS; ++cell) {
        const int value = value_from_code(state, cell);
        result.push_back(value == 0 ? '_' : static_cast<char>('0' + value));
    }
    return result;
}
