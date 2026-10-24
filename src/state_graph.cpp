#include "state_graph.hpp"

#include "cpu_layout_engine.hpp"

#include <algorithm>
#include <limits>

namespace
{
constexpr size_t INVALID_INDEX = static_cast<size_t>(-1);
constexpr std::uint32_t INVALID_PARENT = std::numeric_limits<std::uint32_t>::max();
} // namespace

state_graph::state_graph(cpu_layout_engine& _physics) : physics(_physics)
{}

auto state_graph::clear() -> void
{
    starting_state_index = INVALID_INDEX;
    current_state_index = INVALID_INDEX;
    previous_state_index = INVALID_INDEX;

    links.clear();
    node_target_distances.clear();
    winning_indices.clear();
    winning_path.clear();
    path_indices.clear();
    move_history.clear();
    visit_counts.clear();
    known_state_count = 0;
    graph_complete = true;
    directed = false;

    physics.clear_cmd();
}

auto state_graph::reset_to(const state_id id, const state_model& model) -> void
{
    clear();

    known_state_count = model.state_count();
    visit_counts.assign(known_state_count, 0);
    directed = model.directed_edges();

    current_state_index = id;
    previous_state_index = id;
    starting_state_index = id;

    if (current_state_index < visit_counts.size()) {
        visit_counts[current_state_index] = 1;
    }

    winning_indices.clear();
    if (model.is_goal(id)) {
        winning_indices.insert(id);
    }

    for (size_t i = 0; i < known_state_count; ++i) {
        physics.add_mass_cmd();
    }

    total_moves = 0;
}

auto state_graph::add_state() -> size_t
{
    const size_t index = known_state_count;
    ++known_state_count;
    visit_counts.resize(known_state_count, 0);
    physics.add_mass_cmd();
    return index;
}

auto state_graph::add_link(const size_t first_index, const size_t second_index) -> void
{
    links.emplace_back(first_index, second_index);
    physics.add_spring_cmd(first_index, second_index);
}

auto state_graph::update_current(const state_id id, const state_model& model) -> void
{
    const size_t count = model.state_count();
    const bool graph_changed = count > known_state_count;

    if (graph_changed) {
        // The model has already inserted the new state(s). Add the corresponding physics masses
        // before the spring, otherwise add_spring() indexes past the mass arrays and corrupts the
        // heap.
        const size_t added = count - known_state_count;
        for (size_t i = 0; i < added; ++i) {
            physics.add_mass_cmd();
        }

        if (known_state_count > 0 && current_state_index != INVALID_INDEX &&
            current_state_index != id) {
            add_link(current_state_index, id);
        }
        known_state_count = count;
    }

    visit_counts.resize(count, 0);

    previous_state_index = current_state_index;
    current_state_index = id;

    if (current_state_index != previous_state_index && previous_state_index != INVALID_INDEX) {
        move_history.emplace_back(previous_state_index);
    }

    if (model.is_goal(current_state_index)) {
        winning_indices.insert(current_state_index);
    }

    visit_counts[current_state_index]++;
    total_moves++;

    if (graph_changed) {
        populate_node_target_distances();
    }
    populate_winning_path();
}

auto state_graph::populate_graph(state_model& model, const state_id start, const state_id current,
                                 const threadpool pool, exploration_observer* observer) -> void
{
    exploration_result result = model.explore(start, current, pool, observer);
    apply_exploration_result(std::move(result), model);
}

auto state_graph::apply_exploration_result(exploration_result result, const state_model& model)
    -> void
{
    clear();

    links = std::move(result.links);
    known_state_count = model.state_count();
    visit_counts.assign(known_state_count, 0);

    physics.add_mass_springs_cmd(known_state_count, links);

    current_state_index = result.current;
    previous_state_index = result.current;
    starting_state_index = result.start;
    graph_complete = result.complete;
    directed = result.directed;

    if (current_state_index < visit_counts.size()) {
        visit_counts[current_state_index] = 1;
    }
}

auto state_graph::populate_winning_indices(const state_model& model) -> void
{
    winning_indices.clear();
    for (size_t index = 0; index < model.state_count(); ++index) {
        if (model.is_goal(index)) {
            winning_indices.insert(index);
        }
    }
}

auto state_graph::populate_node_target_distances() -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    if (links.empty() || winning_indices.empty()) {
        node_target_distances.clear();
        return;
    }

    const std::vector<size_t> targets(winning_indices.begin(), winning_indices.end());
    node_target_distances.calculate_distances(known_state_count, links, targets, directed);
}

auto state_graph::populate_winning_path() -> void
{
    if (node_target_distances.empty() || current_state_index == INVALID_INDEX) {
        winning_path.clear();
        path_indices.clear();
        return;
    }

    winning_path = node_target_distances.get_shortest_path(current_state_index);

    path_indices.clear();
    for (const size_t index : winning_path) {
        path_indices.insert(index);
    }
}

auto state_graph::reset_history() -> void
{
    move_history.clear();
    total_moves = 0;
}

auto state_graph::reset_visits() -> void
{
    std::fill(visit_counts.begin(), visit_counts.end(), 0);
    if (current_state_index < visit_counts.size()) {
        visit_counts[current_state_index]++;
    }
}

auto state_graph::pop_history_back() -> void
{
    if (!move_history.empty()) {
        move_history.pop_back();
    }
}

auto state_graph::get_move_history() const -> const std::vector<size_t>&
{
    return move_history;
}

auto state_graph::optimal_next(const size_t index) const -> std::optional<size_t>
{
    if (node_target_distances.empty() || index >= node_target_distances.distances.size()) {
        return std::nullopt;
    }
    if (node_target_distances.distances[index] == 0) {
        return std::nullopt;
    }

    const std::uint32_t parent = node_target_distances.parents[index];
    if (parent == INVALID_PARENT) {
        return std::nullopt;
    }
    return static_cast<size_t>(parent);
}

auto state_graph::nearest_target(const size_t index) const -> std::optional<size_t>
{
    if (node_target_distances.empty() || index >= node_target_distances.nearest_targets.size()) {
        return std::nullopt;
    }

    const std::uint32_t target = node_target_distances.nearest_targets[index];
    if (target == INVALID_PARENT) {
        return std::nullopt;
    }
    return static_cast<size_t>(target);
}

auto state_graph::most_distant() const -> std::optional<size_t>
{
    if (node_target_distances.empty()) {
        return std::nullopt;
    }

    int max_distance = -1;
    size_t max_index = INVALID_INDEX;
    for (size_t i = 0; i < node_target_distances.distances.size(); ++i) {
        if (node_target_distances.distances[i] > max_distance) {
            max_distance = node_target_distances.distances[i];
            max_index = i;
        }
    }

    if (max_index == INVALID_INDEX) {
        return std::nullopt;
    }
    return max_index;
}

auto state_graph::memory_bytes() const -> size_t
{
    constexpr size_t entry_overhead = 24;
    return links.capacity() * sizeof(spring) +
        node_target_distances.distances.capacity() * sizeof(int) +
        node_target_distances.parents.capacity() * sizeof(std::uint32_t) +
        node_target_distances.nearest_targets.capacity() * sizeof(std::uint32_t) +
        winning_indices.size() * (sizeof(size_t) + entry_overhead) +
        winning_path.capacity() * sizeof(size_t) +
        path_indices.size() * (sizeof(size_t) + entry_overhead) +
        move_history.capacity() * sizeof(size_t) + visit_counts.capacity() * sizeof(int);
}

auto state_graph::get_current_index() const -> size_t
{
    return current_state_index;
}

auto state_graph::get_starting_index() const -> size_t
{
    return starting_state_index;
}

auto state_graph::get_previous_index() const -> size_t
{
    return previous_state_index;
}

auto state_graph::get_link_count() const -> size_t
{
    return links.size();
}

auto state_graph::get_target_count() const -> size_t
{
    return winning_indices.size();
}

auto state_graph::get_path_length() const -> size_t
{
    return winning_path.size();
}

auto state_graph::get_links() const -> const std::vector<spring>&
{
    return links;
}

auto state_graph::get_winning_indices() const -> const boost::unordered_flat_set<size_t>&
{
    return winning_indices;
}

auto state_graph::get_visit_counts() const -> const std::vector<int>&
{
    return visit_counts;
}

auto state_graph::get_winning_path() const -> const std::vector<size_t>&
{
    return winning_path;
}

auto state_graph::get_path_indices() const -> const boost::unordered_flat_set<size_t>&
{
    return path_indices;
}

auto state_graph::get_current_visits() const -> int
{
    if (current_state_index >= visit_counts.size()) {
        return 0;
    }
    return visit_counts[current_state_index];
}

auto state_graph::has_history() const -> bool
{
    return !move_history.empty();
}

auto state_graph::has_distances() const -> bool
{
    return !node_target_distances.empty();
}

auto state_graph::get_distances() const -> const std::vector<int>&
{
    return node_target_distances.distances;
}

auto state_graph::get_total_moves() const -> size_t
{
    return static_cast<size_t>(total_moves);
}

auto state_graph::is_complete() const -> bool
{
    return graph_complete;
}
