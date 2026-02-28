#include "state_manager.hpp"
#include "graph_distances.hpp"
#include "util.hpp"

#include <fstream>
#include <ios>

#ifdef TRACY
    #include <tracy/Tracy.hpp>
#endif

auto state_manager::synced_try_insert_state(const puzzle& state) -> size_t
{
    if (state_indices.contains(state)) {
        return state_indices.at(state);
    }

    const size_t index = state_pool.size();
    state_pool.emplace_back(state);
    state_indices.emplace(state, index);
    visit_counts[index] = 0;

    // Queue an update to the physics engine state to keep in sync
    physics.add_mass_cmd();

    return index;
}

auto state_manager::synced_insert_link(size_t first_index, size_t second_index) -> void
{
    links.emplace_back(first_index, second_index);

    // Queue an update to the physics engine state to keep in sync
    physics.add_spring_cmd(first_index, second_index);
}

auto state_manager::synced_insert_statespace(const std::vector<puzzle>& states,
                                             const std::vector<std::pair<size_t, size_t>>& _links)
    -> void
{
    if (!state_pool.empty() || !state_indices.empty() || !links.empty()) {
        warnln("Inserting statespace but collections haven't been cleared");
    }

    for (const puzzle& state : states) {
        const size_t index = state_pool.size();
        state_pool.emplace_back(state);
        state_indices.emplace(state, index);
        visit_counts[index] = 0;
    }
    for (const auto& [from, to] : _links) {
        links.emplace_back(from, to);
    }

    // Queue an update to the physics engine state to keep in sync
    physics.add_mass_springs_cmd(state_pool.size(), links);
}

auto state_manager::synced_clear_statespace() -> void
{
    // Those are invalid without any states
    current_state_index = -1;
    previous_state_index = -1;
    starting_state_index = -1;

    state_pool.clear();
    state_indices.clear();
    links.clear();
    node_target_distances.clear();
    winning_indices.clear();
    winning_path.clear();
    path_indices.clear();

    move_history = std::stack<size_t>();
    visit_counts.clear();

    // Queue an update to the physics engine state to keep in sync
    physics.clear_cmd();
}

auto state_manager::parse_preset_file(const std::string& _preset_file) -> bool
{
    preset_file = _preset_file;

    std::ifstream file(preset_file);
    if (!file) {
        infoln("Preset file \"{}\" couldn't be loaded.", preset_file);
        return false;
    }

    std::string line;
    std::vector<std::string> comment_lines;
    std::vector<std::string> preset_lines;
    while (std::getline(file, line)) {
        if (line.starts_with("F") || line.starts_with("R")) {
            preset_lines.push_back(line);
        } else if (line.starts_with("#")) {
            comment_lines.push_back(line);
        }
    }

    if (preset_lines.empty() || comment_lines.size() != preset_lines.size()) {
        infoln("Preset file \"{}\" couldn't be loaded.", preset_file);
        return false;
    }

    preset_states.clear();
    for (const auto& preset : preset_lines) {
        const puzzle& p = puzzle(preset);

        if (const std::optional<std::string>& reason = p.try_get_invalid_reason()) {
            preset_states = {puzzle(4, 5, 9, 9, false)};
            infoln("Preset file \"{}\" contained invalid presets: {}", preset_file, *reason);
            return false;
        }
        preset_states.emplace_back(p);
    }
    preset_comments = comment_lines;

    infoln("Loaded {} presets from \"{}\".", preset_lines.size(), preset_file);

    return true;
}

auto state_manager::append_preset_file(const std::string& preset_name) -> bool
{
    infoln(R"(Saving preset "{}" to "{}")", preset_name, preset_file);

    if (get_current_state().try_get_invalid_reason()) {
        return false;
    }

    std::ofstream file(preset_file, std::ios_base::app | std::ios_base::out);
    if (!file) {
        infoln("Preset file \"{}\" couldn't be loaded.", preset_file);
        return false;
    }

    file << "\n# " << preset_name << "\n" << get_current_state().state << std::flush;

    infoln("Refreshing presets...");
    if (parse_preset_file(preset_file)) {
        load_preset(preset_states.size() - 1);
    }

    return true;
}

auto state_manager::load_preset(const size_t preset) -> void
{
    clear_graph_and_add_current(preset_states.at(preset));
    current_preset = preset;
}

auto state_manager::load_previous_preset() -> void
{
    load_preset((preset_states.size() + current_preset - 1) % preset_states.size());
}

auto state_manager::load_next_preset() -> void
{
    load_preset((current_preset + 1) % preset_states.size());
}

auto state_manager::update_current_state(const puzzle& p) -> void
{
    if (!p.valid()) {
        return;
    }

    const size_t size_before = state_pool.size();

    // If state is a duplicate, index will be the existing index,
    // if state is new, index will be state_pool.size() - 1
    const size_t index = synced_try_insert_state(p);

    // Because synced_insert_link does not check for duplicates we do it here,
    // if the size grows, it was not a duplicate, and we can add the spring
    if (state_pool.size() > size_before) {
        // The order is important, as the position of the second mass will be updated depending on
        // the first
        synced_insert_link(current_state_index, index);
    }

    previous_state_index = current_state_index;
    current_state_index = index;

    if (current_state_index != previous_state_index) {
        move_history.push(previous_state_index);
    }

    if (p.won()) {
        winning_indices.insert(current_state_index);
    }

    // Adds the element with 0 if it doesn't exist
    visit_counts[current_state_index]++;
    total_moves++;

    // Recalculate distances only if the graph changed
    if (state_pool.size() > size_before) {
        populate_node_target_distances();
    }
    populate_winning_path();
}

auto state_manager::edit_starting_state(const puzzle& p) -> void
{
    clear_graph_and_add_current(p);

    move_history = std::stack<size_t>();
    total_moves = 0;
    for (int& visits : visit_counts | std::views::values) {
        visits = 0;
    }
    visit_counts[current_state_index]++;
}

auto state_manager::goto_starting_state() -> void
{
    update_current_state(get_state(starting_state_index));

    // Reset previous movement data since we're starting over (because we're fucking stupid)
    previous_state_index = current_state_index;
    for (int& visits : visit_counts | std::views::values) {
        visits = 0;
    }
    visit_counts[current_state_index]++;
    move_history = std::stack<size_t>();
    total_moves = 0;
}

auto state_manager::goto_optimal_next_state() -> void
{
    if (node_target_distances.empty()) {
        return;
    }

    // Already there
    if (node_target_distances.distances[current_state_index] == 0) {
        return;
    }

    const size_t parent_index = node_target_distances.parents[current_state_index];
    update_current_state(get_state(parent_index));
}

auto state_manager::goto_previous_state() -> void
{
    if (move_history.empty()) {
        return;
    }

    update_current_state(get_state(move_history.top()));

    // Pop twice because update_current_state adds the state again...
    move_history.pop();
    move_history.pop();
}

auto state_manager::goto_most_distant_state() -> void
{
    if (node_target_distances.empty()) {
        return;
    }

    int max_distance = 0;
    size_t max_distance_index = 0;
    for (size_t i = 0; i < node_target_distances.distances.size(); ++i) {
        if (node_target_distances.distances.at(i) > max_distance) {
            max_distance = node_target_distances.distances.at(i);
            max_distance_index = i;
        }
    }

    update_current_state(get_state(max_distance_index));
}

auto state_manager::goto_closest_target_state() -> void
{
    if (node_target_distances.empty()) {
        return;
    }

    update_current_state(get_state(node_target_distances.nearest_targets.at(current_state_index)));
}

auto state_manager::populate_graph() -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    // Need to make a copy before clearing the state_pool
    const puzzle p = get_current_state();

    // Clear the graph first so we don't add duplicates somehow
    synced_clear_statespace();

    // Explore the entire statespace starting from the current state
    const auto& [states, _links] = p.explore_state_space();
    synced_insert_statespace(states, _links);

    current_state_index = state_indices.at(p);
    previous_state_index = current_state_index;
    starting_state_index = current_state_index;

    // Search for cool stuff
    populate_winning_indices();
    populate_node_target_distances();
    populate_winning_path();
}

auto state_manager::clear_graph_and_add_current(const puzzle& p) -> void
{
    // Do we need to make a copy before clearing the state_pool?
    const puzzle _p = p; // NOLINT(*-unnecessary-copy-initialization)

    synced_clear_statespace();

    // Re-add the current state
    current_state_index = synced_try_insert_state(_p);

    // These states are no longer in the graph
    previous_state_index = current_state_index;
    starting_state_index = current_state_index;

    visit_counts[current_state_index]++;
}

auto state_manager::clear_graph_and_add_current() -> void
{
    clear_graph_and_add_current(get_current_state());
}

auto state_manager::populate_winning_indices() -> void
{
    winning_indices.clear();
    for (const auto& [state, index] : state_indices) {
        if (state.won()) {
            winning_indices.insert(index);
        }
    }
}

auto state_manager::populate_node_target_distances() -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    if (links.empty() || winning_indices.empty()) {
        return;
    }

    const std::vector<size_t> targets(winning_indices.begin(), winning_indices.end());
    node_target_distances.calculate_distances(state_pool.size(), links, targets);
}

auto state_manager::populate_winning_path() -> void
{
    if (node_target_distances.empty()) {
        return;
    }

    winning_path = node_target_distances.get_shortest_path(current_state_index);

    path_indices.clear();
    for (const size_t index : winning_path) {
        path_indices.insert(index);
    }
}

auto state_manager::get_index(const puzzle& state) const -> size_t
{
    return state_indices.at(state);
}

auto state_manager::get_current_index() const -> size_t
{
    return current_state_index;
}

auto state_manager::get_starting_index() const -> size_t
{
    return starting_state_index;
}

auto state_manager::get_state(const size_t index) const -> const puzzle&
{
    return state_pool.at(index);
}

auto state_manager::get_current_state() const -> const puzzle&
{
    return get_state(current_state_index);
}

auto state_manager::get_starting_state() const -> const puzzle&
{
    return get_state(starting_state_index);
}

auto state_manager::get_state_count() const -> size_t
{
    return state_pool.size();
}

auto state_manager::get_target_count() const -> size_t
{
    return winning_indices.size();
}

auto state_manager::get_link_count() const -> size_t
{
    return links.size();
}

auto state_manager::get_path_length() const -> size_t
{
    return winning_path.size();
}

auto state_manager::get_links() const -> const std::vector<std::pair<size_t, size_t>>&
{
    return links;
}

auto state_manager::get_winning_indices() const -> const std::unordered_set<size_t>&
{
    return winning_indices;
}

auto state_manager::get_visit_counts() const -> const std::unordered_map<size_t, int>&
{
    return visit_counts;
}

auto state_manager::get_winning_path() const -> const std::vector<size_t>&
{
    return winning_path;
}

auto state_manager::get_path_indices() const -> const std::unordered_set<size_t>&
{
    return path_indices;
}

auto state_manager::get_current_visits() const -> int
{
    return visit_counts.at(current_state_index);
}

auto state_manager::get_current_preset() const -> size_t
{
    return current_preset;
}

auto state_manager::get_preset_count() const -> size_t
{
    return preset_states.size();
}

auto state_manager::get_current_preset_comment() const -> const std::string&
{
    return preset_comments.at(current_preset);
}

auto state_manager::has_history() const -> bool
{
    return !move_history.empty();
}

auto state_manager::has_distances() const -> bool
{
    return !node_target_distances.empty();
}

auto state_manager::get_total_moves() const -> size_t
{
    return total_moves;
}

auto state_manager::was_edited() const -> bool
{
    return preset_states.at(current_preset) != get_state(starting_state_index);
}