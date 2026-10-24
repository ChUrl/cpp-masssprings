#include "sliding_block_model.hpp"

#include <utility>

sliding_block_model::sliding_block_model() = default;

auto sliding_block_model::state_count() const -> size_t
{
    return states.size();
}

auto sliding_block_model::memory_bytes() const -> size_t
{
    constexpr size_t entry_overhead = 32;
    return states.capacity() * sizeof(puzzle) +
        indices.size() * (sizeof(puzzle) + sizeof(size_t) + entry_overhead);
}

auto sliding_block_model::is_goal(const state_id id) const -> bool
{
    return states.at(id).goal_reached();
}

auto sliding_block_model::is_valid(const state_id id) const -> bool
{
    return states.at(id).valid();
}

auto sliding_block_model::describe(const state_id id) const -> std::string
{
    return states.at(id).string_repr();
}

auto sliding_block_model::clone() const -> std::unique_ptr<state_model>
{
    return std::make_unique<sliding_block_model>(*this);
}

auto sliding_block_model::initial_state() -> state_id
{
    if (states.empty()) {
        return insert_puzzle(puzzle(4, 5, 0, 0, true, false));
    }
    return 0;
}

auto sliding_block_model::reset_to(const state_id id) -> state_id
{
    const puzzle state = states.at(id);
    states.clear();
    indices.clear();
    return insert_puzzle(state);
}

auto sliding_block_model::explore(const state_id start, const state_id current,
                                  const threadpool pool, exploration_observer* observer)
    -> exploration_result
{
    const puzzle start_puzzle = states.at(start);
    const puzzle current_puzzle = states.at(current);

    state_space_result explored = start_puzzle.explore_state_space_with_indices(pool, observer);

    states = std::move(explored.states);
    indices = std::move(explored.indices);

    exploration_result result;
    result.links = std::move(explored.links);
    result.start = indices.at(start_puzzle);
    if (const auto current_it = indices.find(current_puzzle); current_it != indices.end()) {
        result.current = current_it->second;
    } else {
        result.current = result.start;
    }
    result.complete = !explored.cancelled;
    result.cancelled = explored.cancelled;
    return result;
}

auto sliding_block_model::get(const state_id id) const -> const puzzle&
{
    return states.at(id);
}

auto sliding_block_model::find_or_insert(const puzzle& state) -> state_id
{
    if (const auto it = indices.find(state); it != indices.end()) {
        return it->second;
    }
    return insert_puzzle(state);
}

auto sliding_block_model::set_state(const puzzle& state) -> state_id
{
    states.clear();
    indices.clear();
    return insert_puzzle(state);
}

auto sliding_block_model::insert_puzzle(const puzzle& state) -> state_id
{
    const state_id id = states.size();
    states.emplace_back(state);
    indices.emplace(state, id);
    return id;
}

auto sliding_block_model::try_move(const state_id id, const int x, const int y, const dir direction)
    -> std::optional<state_id>
{
    const std::optional<puzzle> next = states.at(id).try_move_block_at(x, y, direction);
    if (!next) {
        return std::nullopt;
    }
    return find_or_insert(*next);
}

auto sliding_block_model::try_add_block(const state_id id, const block b) -> std::optional<state_id>
{
    const std::optional<puzzle> next = states.at(id).try_add_block(b);
    if (!next) {
        return std::nullopt;
    }
    return find_or_insert(*next);
}

auto sliding_block_model::try_remove_block(const state_id id, const int x, const int y)
    -> std::optional<state_id>
{
    const std::optional<puzzle> next = states.at(id).try_remove_block(x, y);
    if (!next) {
        return std::nullopt;
    }
    return find_or_insert(*next);
}

auto sliding_block_model::try_set_goal(const state_id id, const int x, const int y)
    -> std::optional<state_id>
{
    const std::optional<puzzle> next = states.at(id).try_set_goal(x, y);
    if (!next) {
        return std::nullopt;
    }
    return find_or_insert(*next);
}

auto sliding_block_model::try_toggle_target(const state_id id, const int x, const int y)
    -> std::optional<state_id>
{
    const std::optional<puzzle> next = states.at(id).try_toggle_target(x, y);
    if (!next) {
        return std::nullopt;
    }
    return find_or_insert(*next);
}

auto sliding_block_model::try_toggle_wall(const state_id id, const int x, const int y)
    -> std::optional<state_id>
{
    const std::optional<puzzle> next = states.at(id).try_toggle_wall(x, y);
    if (!next) {
        return std::nullopt;
    }
    return find_or_insert(*next);
}

auto sliding_block_model::try_add_wall(const state_id id, const int x, const int y)
    -> std::optional<state_id>
{
    const std::optional<puzzle> next = states.at(id).try_add_wall(x, y);
    if (!next) {
        return std::nullopt;
    }
    return find_or_insert(*next);
}

auto sliding_block_model::try_add_wall_block(const state_id id, const block b)
    -> std::optional<state_id>
{
    const std::optional<puzzle> next = states.at(id).try_add_wall_block(b);
    if (!next) {
        return std::nullopt;
    }
    return find_or_insert(*next);
}

auto sliding_block_model::try_remove_wall(const state_id id, const int x, const int y)
    -> std::optional<state_id>
{
    const std::optional<puzzle> next = states.at(id).try_remove_wall(x, y);
    if (!next) {
        return std::nullopt;
    }
    return find_or_insert(*next);
}

auto sliding_block_model::toggle_restricted(const state_id id) -> std::optional<state_id>
{
    return find_or_insert(states.at(id).toggle_restricted());
}

auto sliding_block_model::clear_goal(const state_id id) -> std::optional<state_id>
{
    return find_or_insert(states.at(id).clear_goal());
}

auto sliding_block_model::try_add_column(const state_id id) -> std::optional<state_id>
{
    const std::optional<puzzle> next = states.at(id).try_add_column();
    if (!next) {
        return std::nullopt;
    }
    return find_or_insert(*next);
}

auto sliding_block_model::try_remove_column(const state_id id) -> std::optional<state_id>
{
    const std::optional<puzzle> next = states.at(id).try_remove_column();
    if (!next) {
        return std::nullopt;
    }
    return find_or_insert(*next);
}

auto sliding_block_model::try_add_row(const state_id id) -> std::optional<state_id>
{
    const std::optional<puzzle> next = states.at(id).try_add_row();
    if (!next) {
        return std::nullopt;
    }
    return find_or_insert(*next);
}

auto sliding_block_model::try_remove_row(const state_id id) -> std::optional<state_id>
{
    const std::optional<puzzle> next = states.at(id).try_remove_row();
    if (!next) {
        return std::nullopt;
    }
    return find_or_insert(*next);
}
