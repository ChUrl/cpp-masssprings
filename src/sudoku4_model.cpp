#include "sudoku4_model.hpp"

#include <array>
#include <format>
#include <stdexcept>

sudoku4_model::sudoku4_model()
{
    configure(0, 0);
}

auto sudoku4_model::configure(const std::uint16_t given_mask, const std::uint64_t given_code)
    -> void
{
    std::uint64_t masked_givens = 0;
    for (int cell = 0; cell < CELLS; ++cell) {
        if ((given_mask & (1u << cell)) == 0) {
            continue;
        }

        const int value = value_from_code(given_code, cell);
        if (value < 1 || value > DIGITS) {
            throw std::invalid_argument("Sudoku given must be a digit from 1 to 4");
        }
        masked_givens |= static_cast<std::uint64_t>(value)
            << (4u * static_cast<unsigned int>(cell));
    }

    if (!is_consistent_code(masked_givens, given_mask, masked_givens)) {
        throw std::invalid_argument("Sudoku givens conflict");
    }

    given_mask_ = given_mask;
    given_code_ = masked_givens;
    states.clear();
    indices.clear();
}

auto sudoku4_model::state_count() const -> size_t
{
    return states.size();
}

auto sudoku4_model::memory_bytes() const -> size_t
{
    constexpr size_t entry_overhead = 32;
    return states.capacity() * sizeof(std::uint64_t) +
        indices.size() * (sizeof(std::uint64_t) + sizeof(size_t) + entry_overhead);
}

auto sudoku4_model::is_goal(const state_id id) const -> bool
{
    return is_valid(id) && empty_count(id) == 0;
}

auto sudoku4_model::is_valid(const state_id id) const -> bool
{
    return id < states.size() && is_consistent_code(states[id], given_mask_, given_code_);
}

auto sudoku4_model::describe(const state_id id) const -> std::string
{
    return std::format("S4:[{}x{}] G:[{}] B:[{}]", SIZE, SIZE, givens_string(),
                       state_string(code(id)));
}

auto sudoku4_model::initial_state() -> state_id
{
    if (states.empty()) {
        return find_or_insert(given_code_);
    }
    return 0;
}

auto sudoku4_model::reset_to(const state_id id) -> state_id
{
    const std::uint64_t state = code(id);
    states.clear();
    indices.clear();
    return find_or_insert(state);
}

auto sudoku4_model::clone() const -> std::unique_ptr<state_model>
{
    return std::make_unique<sudoku4_model>(*this);
}

auto sudoku4_model::explore(const state_id start, const state_id current, const threadpool pool,
                            exploration_observer* observer) -> exploration_result
{
    (void)pool;

    const std::uint64_t start_code = code(start);
    const std::uint64_t current_code = code(current);

    states.clear();
    indices.clear();

    const state_id start_id = insert_code(start_code).first;

    std::vector<state_id> queue;
    queue.reserve(1u << 14);
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
        const std::uint64_t state = states[id];

        std::array<int, SIZE> row_masks{};
        std::array<int, SIZE> col_masks{};
        std::array<int, SIZE> box_masks{};
        for (int cell = 0; cell < CELLS; ++cell) {
            const int value = value_from_code(state, cell);
            if (value == 0) {
                continue;
            }
            const int row = cell / SIZE;
            const int col = cell % SIZE;
            const int box = (row / BOX) * BOX + col / BOX;
            const int bit = 1 << value;
            row_masks[static_cast<size_t>(row)] |= bit;
            col_masks[static_cast<size_t>(col)] |= bit;
            box_masks[static_cast<size_t>(box)] |= bit;
        }

        for (int cell = 0; cell < CELLS; ++cell) {
            if (is_given(cell)) {
                continue;
            }

            const int row = cell / SIZE;
            const int col = cell % SIZE;
            const int box = (row / BOX) * BOX + col / BOX;
            const int value = value_from_code(state, cell);

            if (value != 0) {
                const std::uint64_t next_state = set_cell(state, cell, 0);
                const auto [next_id, inserted] = insert_code(next_state);
                if (inserted) {
                    queue.push_back(next_id);
                }
                if (id < next_id) {
                    links.emplace_back(id, next_id);
                }
                continue;
            }

            for (int digit = 1; digit <= DIGITS; ++digit) {
                const int bit = 1 << digit;
                if ((row_masks[static_cast<size_t>(row)] & bit) != 0 ||
                    (col_masks[static_cast<size_t>(col)] & bit) != 0 ||
                    (box_masks[static_cast<size_t>(box)] & bit) != 0) {
                    continue;
                }

                const std::uint64_t next_state = set_cell(state, cell, digit);
                const auto [next_id, inserted] = insert_code(next_state);
                if (inserted) {
                    queue.push_back(next_id);
                }
                if (id < next_id) {
                    links.emplace_back(id, next_id);
                }
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

auto sudoku4_model::code(const state_id id) const -> std::uint64_t
{
    return states.at(id);
}

auto sudoku4_model::value(const state_id id, const int cell) const -> int
{
    return value_from_code(code(id), cell);
}

auto sudoku4_model::empty_count(const state_id id) const -> int
{
    const std::uint64_t state = code(id);
    int count = 0;
    for (int cell = 0; cell < CELLS; ++cell) {
        if (value_from_code(state, cell) == 0) {
            ++count;
        }
    }
    return count;
}

auto sudoku4_model::is_given(const int cell) const -> bool
{
    return cell >= 0 && cell < CELLS && (given_mask_ & (1u << cell)) != 0;
}

auto sudoku4_model::given_value(const int cell) const -> int
{
    return is_given(cell) ? value_from_code(given_code_, cell) : 0;
}

auto sudoku4_model::given_mask() const -> std::uint16_t
{
    return given_mask_;
}

auto sudoku4_model::given_code() const -> std::uint64_t
{
    return given_code_;
}

auto sudoku4_model::find_or_insert(const std::uint64_t state) -> state_id
{
    return insert_code(state).first;
}

auto sudoku4_model::try_set(const state_id id, const int cell, const int digit)
    -> std::optional<state_id>
{
    if (id >= states.size() || cell < 0 || cell >= CELLS || is_given(cell) || digit < 0 ||
        digit > DIGITS) {
        return std::nullopt;
    }

    const int current = value(id, cell);
    if (digit == 0) {
        if (current == 0) {
            return std::nullopt;
        }
        return find_or_insert(set_cell(code(id), cell, 0));
    }

    if (current != 0 || !can_place(code(id), cell, digit)) {
        return std::nullopt;
    }

    return find_or_insert(set_cell(code(id), cell, digit));
}

auto sudoku4_model::is_consistent_code(const std::uint64_t state, const std::uint16_t given_mask,
                                       const std::uint64_t given_code) -> bool
{
    std::array<int, SIZE> row_masks{};
    std::array<int, SIZE> col_masks{};
    std::array<int, SIZE> box_masks{};

    for (int cell = 0; cell < CELLS; ++cell) {
        const int value = value_from_code(state, cell);
        if (value < 0 || value > DIGITS) {
            return false;
        }

        if ((given_mask & (1u << cell)) != 0) {
            const int expected = value_from_code(given_code, cell);
            if (expected < 1 || expected > DIGITS || value != expected) {
                return false;
            }
        }

        if (value == 0) {
            continue;
        }

        const int row = cell / SIZE;
        const int col = cell % SIZE;
        const int box = (row / BOX) * BOX + col / BOX;
        const int bit = 1 << value;
        if ((row_masks[static_cast<size_t>(row)] & bit) != 0 ||
            (col_masks[static_cast<size_t>(col)] & bit) != 0 ||
            (box_masks[static_cast<size_t>(box)] & bit) != 0) {
            return false;
        }

        row_masks[static_cast<size_t>(row)] |= bit;
        col_masks[static_cast<size_t>(col)] |= bit;
        box_masks[static_cast<size_t>(box)] |= bit;
    }

    return true;
}

auto sudoku4_model::value_from_code(const std::uint64_t state, const int cell) -> int
{
    return static_cast<int>((state >> (4u * static_cast<unsigned int>(cell))) & 0xFu);
}

auto sudoku4_model::set_cell(const std::uint64_t state, const int cell, const int digit)
    -> std::uint64_t
{
    const std::uint64_t mask = std::uint64_t{0xF} << (4u * static_cast<unsigned int>(cell));
    return (state & ~mask) |
        (static_cast<std::uint64_t>(digit) << (4u * static_cast<unsigned int>(cell)));
}

auto sudoku4_model::can_place(const std::uint64_t state, const int cell, const int digit) const
    -> bool
{
    const int row = cell / SIZE;
    const int col = cell % SIZE;
    const int box = (row / BOX) * BOX + col / BOX;

    for (int other = 0; other < CELLS; ++other) {
        if (other == cell || value_from_code(state, other) != digit) {
            continue;
        }

        const int other_row = other / SIZE;
        const int other_col = other % SIZE;
        const int other_box = (other_row / BOX) * BOX + other_col / BOX;
        if (other_row == row || other_col == col || other_box == box) {
            return false;
        }
    }

    return true;
}

auto sudoku4_model::insert_code(const std::uint64_t state) -> std::pair<state_id, bool>
{
    if (const auto it = indices.find(state); it != indices.end()) {
        return {it->second, false};
    }

    const state_id id = states.size();
    states.push_back(state);
    indices.emplace(state, id);
    return {id, true};
}

auto sudoku4_model::givens_string() const -> std::string
{
    std::string result;
    result.reserve(CELLS);
    for (int cell = 0; cell < CELLS; ++cell) {
        const int value = given_value(cell);
        result.push_back(value == 0 ? '_' : static_cast<char>('0' + value));
    }
    return result;
}

auto sudoku4_model::state_string(const std::uint64_t state) const -> std::string
{
    std::string result;
    result.reserve(CELLS);
    for (int cell = 0; cell < CELLS; ++cell) {
        const int value = value_from_code(state, cell);
        result.push_back(value == 0 ? '_' : static_cast<char>('0' + value));
    }
    return result;
}
