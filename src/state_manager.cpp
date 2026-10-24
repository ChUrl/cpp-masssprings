#include "state_manager.hpp"

#include "load_save.hpp"
#include "puzzle_registry.hpp"
#include "puzzle_space_io.hpp"
#include "settings.hpp"
#include "sliding_block_model.hpp"
#include "util.hpp"

#include <algorithm>
#include <chrono>
#include <format>

state_manager::state_manager(cpu_layout_engine& _physics, puzzle_registry& _registry,
                             const std::string& _preset_file)
    : physics(_physics), registry(_registry), preset_file(_preset_file), graph(_physics)
{
    reload_preset_file();

    if (!active_puzzle.valid()) {
        switch_puzzle("sliding_block");
    }
}

auto state_manager::switch_to_preset(const preset_entry& preset) -> bool
{
    std::unique_ptr<puzzle_kind> kind = registry.create(preset.type);
    if (!kind) {
        warnln("Unknown puzzle type \"{}\" in preset file.", preset.type);
        return false;
    }

    const std::optional<state_id> id = kind->codec->parse(preset.payload, *kind->model);
    if (!id) {
        warnln("Failed to parse preset \"{}\" as {}.", preset.comment, preset.type);
        return false;
    }

    if (search.running() && search.kind() == ::search_kind::state_space) {
        search.cancel();
    }
    active_puzzle.replace(std::move(kind));
    graph.reset_to(*id, active_puzzle.model());
    edited = false;
    custom_state = false;
    custom_label.clear();
    if (mode == search_mode::puzzle_space && !active_puzzle.presenter().supports_puzzle_space()) {
        mode = search_mode::state_space;
    }
    return true;
}

auto state_manager::context() -> puzzle_context&
{
    return active_puzzle;
}

auto state_manager::context() const -> const puzzle_context&
{
    return active_puzzle;
}

auto state_manager::model() -> state_model&
{
    return active_puzzle.model();
}

auto state_manager::model() const -> const state_model&
{
    return active_puzzle.model();
}

auto state_manager::presenter() -> puzzle_presenter&
{
    return active_puzzle.presenter();
}

auto state_manager::presenter() const -> const puzzle_presenter&
{
    return active_puzzle.presenter();
}

auto state_manager::codec() -> puzzle_codec&
{
    return active_puzzle.codec();
}

auto state_manager::codec() const -> const puzzle_codec&
{
    return active_puzzle.codec();
}

auto state_manager::generation() const -> std::uint64_t
{
    return active_puzzle.generation();
}

auto state_manager::current_puzzle_type() const -> const std::string&
{
    return active_puzzle.type_id();
}

auto state_manager::available_puzzle_types() const -> std::vector<std::string>
{
    return registry.ids();
}

auto state_manager::switch_puzzle(const std::string& type) -> bool
{
    // Prefer the first preset of the requested type, so the board and the preset name match.
    for (size_t i = 0; i < presets.size(); ++i) {
        if (presets[i].type == type && switch_to_preset(presets[i])) {
            current_preset = i;
            return true;
        }
    }

    std::unique_ptr<puzzle_kind> kind = registry.create(type);
    if (!kind) {
        warnln("Unknown puzzle type \"{}\".", type);
        return false;
    }

    if (search.running() && search.kind() == ::search_kind::state_space) {
        search.cancel();
    }
    active_puzzle.replace(std::move(kind));

    const state_id id = active_puzzle.model().initial_state();
    graph.reset_to(id, active_puzzle.model());

    edited = false;
    custom_state = false;
    custom_label.clear();
    current_preset = 0;
    if (mode == search_mode::puzzle_space && !active_puzzle.presenter().supports_puzzle_space()) {
        mode = search_mode::state_space;
    }
    return true;
}

auto state_manager::save_current_to_preset_file(const std::string& preset_comment) -> void
{
    if (validate_current()) {
        return;
    }

    const std::string payload = codec().format(model(), get_current_state_id());
    if (append_preset_file(preset_file, current_puzzle_type(), preset_comment, payload)) {
        current_preset = presets.size();
        reload_preset_file();
    }
}

auto state_manager::reload_preset_file() -> void
{
    presets = parse_preset_file(preset_file);

    if (!presets.empty()) {
        if (current_preset >= presets.size()) {
            current_preset = 0;
        }
        load_preset(current_preset);
    }
}

auto state_manager::load_preset(const size_t preset) -> void
{
    if (preset >= presets.size()) {
        return;
    }

    if (switch_to_preset(presets[preset])) {
        current_preset = preset;
        edited = false;
    }
}

auto state_manager::load_previous_preset() -> void
{
    step_preset(-1);
}

auto state_manager::load_next_preset() -> void
{
    step_preset(1);
}

auto state_manager::step_preset(const int direction) -> void
{
    // Only the presets of the active puzzle type are cycled; the type is chosen
    // through the puzzle spinner.
    const std::string& type = current_puzzle_type();
    std::vector<size_t> indices;
    for (size_t i = 0; i < presets.size(); ++i) {
        if (presets[i].type == type) {
            indices.push_back(i);
        }
    }
    if (indices.empty()) {
        return;
    }

    const auto current = std::ranges::find(indices, current_preset);
    size_t position = 0;
    if (current != indices.end()) {
        position = static_cast<size_t>(current - indices.begin());
    } else if (direction < 0) {
        position = 0; // not on a preset of this type: enter the list from the back
    } else {
        position = indices.size() - 1;
    }

    const size_t next =
        (position + indices.size() + static_cast<size_t>(direction < 0 ? -1 : 1)) % indices.size();
    load_preset(indices[next]);
}

auto state_manager::get_type_preset_count() const -> size_t
{
    const std::string& type = current_puzzle_type();
    return static_cast<size_t>(std::ranges::count_if(presets, [&type](const preset_entry& preset)
                                                     { return preset.type == type; }));
}

auto state_manager::get_type_preset_position() const -> size_t
{
    const std::string& type = current_puzzle_type();
    size_t position = 0;
    for (size_t i = 0; i < presets.size(); ++i) {
        if (presets[i].type != type) {
            continue;
        }
        if (i == current_preset) {
            return position;
        }
        ++position;
    }
    return 0;
}

auto state_manager::update_current_state(const state_id id) -> void
{
    if (id >= model().state_count() || !model().is_valid(id)) {
        return;
    }
    // The background job captured the old starting/current state; applying it after navigation
    // would silently jump back.
    if (search.running() && search.kind() == ::search_kind::state_space) {
        search.cancel();
    }
    graph.update_current(id, model());
}

auto state_manager::edit_starting_state(const state_id id) -> void
{
    if (id >= model().state_count()) {
        return;
    }
    if (search.running() && search.kind() == ::search_kind::state_space) {
        search.cancel();
    }

    const state_id reset = model().reset_to(id);
    graph.reset_to(reset, model());
    graph.reset_history();
    graph.reset_visits();
    edited = true;
}

auto state_manager::goto_starting_state() -> void
{
    update_current_state(graph.get_starting_index());
    graph.reset_history();
    graph.reset_visits();
}

auto state_manager::goto_optimal_next_state() -> void
{
    if (const auto next = graph.optimal_next(graph.get_current_index())) {
        update_current_state(*next);
    }
}

auto state_manager::goto_previous_state() -> void
{
    const auto& history = graph.get_move_history();
    if (history.empty()) {
        return;
    }

    const size_t previous = history.back();
    update_current_state(previous);

    // update_current_state added the old current state to the history; remove both entries.
    graph.pop_history_back();
    graph.pop_history_back();
}

auto state_manager::goto_most_distant_state() -> void
{
    if (const auto index = graph.most_distant()) {
        update_current_state(*index);
    }
}

auto state_manager::goto_closest_target_state() -> void
{
    if (const auto index = graph.nearest_target(graph.get_current_index())) {
        update_current_state(*index);
    }
}

auto state_manager::populate_graph() -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    const state_id start = graph.get_starting_index();
    const state_id current = graph.get_current_index();

    const auto start_time = std::chrono::high_resolution_clock::now();
    graph.populate_graph(model(), start, current, physics.get_thread_pool());
    const auto explored_at = std::chrono::high_resolution_clock::now();

    infoln("State space has size {} with {} transitions.", get_state_count(), get_link_count());

    populate_winning_indices();
    const auto winning_at = std::chrono::high_resolution_clock::now();
    populate_node_target_distances();
    const auto distances_at = std::chrono::high_resolution_clock::now();
    populate_winning_path();
    const auto path_at = std::chrono::high_resolution_clock::now();

    const auto ms_since = [](const auto& from, const auto& to)
    { return std::chrono::duration<double, std::milli>(to - from).count(); };
    const double explore_ms = ms_since(start_time, explored_at);
    const double winning_ms = ms_since(explored_at, winning_at);
    const double distance_ms = ms_since(winning_at, distances_at);
    const double path_ms = ms_since(distances_at, path_at);
    infoln("Graph population: explore={:.1f}ms winning={:.1f}ms distances={:.1f}ms "
           "path={:.1f}ms total={:.1f}ms",
           explore_ms, winning_ms, distance_ms, path_ms, ms_since(start_time, path_at));
}

auto state_manager::clear_graph_and_add_current() -> void
{
    if (search.running() && search.kind() == ::search_kind::state_space) {
        search.cancel();
    }
    const state_id reset = model().reset_to(graph.get_current_index());
    graph.reset_to(reset, model());
}

auto state_manager::populate_winning_indices() -> void
{
    graph.populate_winning_indices(model());
}

auto state_manager::populate_node_target_distances() -> void
{
    graph.populate_node_target_distances();
}

auto state_manager::populate_winning_path() -> void
{
    graph.populate_winning_path();
}

auto state_manager::start_state_space_search() -> bool
{
    pending_result_load.reset();
    if (search.running() || !active_puzzle.valid()) {
        return false;
    }

    search_origin_comment = get_current_preset_comment();
    search_origin_status = presenter().status_text(model(), get_current_state_id());
    search_origin_custom = custom_state;

    std::unique_ptr<state_model> cloned = model().clone();
    if (!cloned) {
        return false;
    }

    return search.start_state_space(std::move(cloned), graph.get_starting_index(),
                                    graph.get_current_index(), current_puzzle_type(), generation(),
                                    get_settings().search_threads);
}

auto state_manager::start_puzzle_space_search(const size_t max_blocks, const int min_moves,
                                              const size_t min_blocks,
                                              const wall_search_options& walls) -> bool
{
    pending_result_load.reset();
    if (search.running()) {
        return false;
    }

    puzzle_space_config_result config =
        build_puzzle_space_config(max_blocks, min_moves, min_blocks, walls);
    if (!config.config) {
        warnln("Puzzle-space search: {}", config.error);
        return false;
    }

    search_origin_comment = get_current_preset_comment();
    search_origin_status = presenter().status_text(model(), get_current_state_id());
    search_origin_custom = custom_state;

    const bool started =
        search.start_puzzle_space(std::move(*config.config), get_settings().search_threads);
    if (started) {
        puzzle_space_results.reset();
        puzzle_space_selection = 0;
    }
    return started;
}

auto state_manager::cancel_search() -> void
{
    search.cancel();
}

auto state_manager::search_thread_count() const -> int
{
    const int active = search.active_thread_count();
    if (active > 0 && search.running()) {
        return active;
    }
    return resolve_search_threads(get_settings().search_threads);
}

auto state_manager::memory_usage() const -> ::memory_usage
{
    ::memory_usage usage = physics.memory_usage();
    usage.model = model().memory_bytes();
    usage.graph = graph.memory_bytes();

    if (search.running()) {
        usage.search += search.current_memory_bytes();
    }
    if (puzzle_space_results) {
        usage.results += puzzle_space_results->clusters.size() * sizeof(puzzle_space_cluster);
        usage.results += sizeof(puzzle_space_config);
    }
    return usage;
}

auto state_manager::memory_bytes() const -> size_t
{
    return memory_usage().total();
}

auto state_manager::poll_search() -> void
{
    // Deferred UI actions: loading a result must not happen from inside a presenter callback.
    if (pending_result_load) {
        const size_t index = *pending_result_load;
        pending_result_load.reset();
        load_puzzle_space_result(index);
    }

    search.poll();

    if (auto result = search.take_state_space_result()) {
        if (!result->exploration.cancelled && result->generation == generation()) {
            std::unique_ptr<puzzle_kind> kind =
                registry.create_with_model(result->type_id, std::move(result->model));
            if (kind) {
                active_puzzle.replace(std::move(kind));
                graph.apply_exploration_result(std::move(result->exploration),
                                               active_puzzle.model());
                populate_winning_indices();
                populate_node_target_distances();
                populate_winning_path();
            } else {
                warnln("Failed to install explored {} model.", result->type_id);
            }
        }
    }

    if (auto result = search.take_puzzle_space_result()) {
        if (!result->cancelled && result->error.empty()) {
            puzzle_space_results = std::move(*result);
            puzzle_space_selection = 0;
            sort_puzzle_space_clusters(puzzle_space_results->clusters, puzzle_space_sort_mode);
            if (current_puzzle_type() == "sliding_block" &&
                !puzzle_space_results->clusters.empty()) {
                load_puzzle_space_result(0);
            }
        } else if (!result->error.empty()) {
            warnln("Puzzle-space search failed: {}", result->error);
        }
    }
}

auto state_manager::get_search_snapshot() const -> ::search_snapshot
{
    return search.snapshot();
}

auto state_manager::search_is_running() const -> bool
{
    return search.running();
}

auto state_manager::get_search_kind() const -> ::search_kind
{
    return search.kind();
}

auto state_manager::get_search_mode() const -> search_mode
{
    return mode;
}

auto state_manager::set_search_mode(const search_mode next) -> void
{
    mode = next;
}

auto state_manager::build_puzzle_space_config(const size_t max_blocks, const int min_moves,
                                              const size_t min_blocks,
                                              const wall_search_options& walls) const
    -> puzzle_space_config_result
{
    const auto* sliding = dynamic_cast<const sliding_block_model*>(&model());
    if (sliding == nullptr) {
        return {std::nullopt, "Puzzle-space search is only available for sliding-block puzzles"};
    }

    const state_id id = get_current_state_id();
    if (id >= sliding->state_count()) {
        return {std::nullopt, "The current board is invalid"};
    }

    return make_puzzle_space_config(sliding->get(id), max_blocks, min_moves, min_blocks, walls);
}

auto state_manager::get_puzzle_space_results() const -> const puzzle_space_result*
{
    return puzzle_space_results ? &*puzzle_space_results : nullptr;
}

auto state_manager::save_puzzle_space_file(const std::string& path) -> std::optional<std::string>
{
    if (!puzzle_space_results) {
        return "No explored puzzle space to save";
    }
    return ::save_puzzle_space_file(path, *puzzle_space_results);
}

auto state_manager::load_puzzle_space_file(const std::string& path) -> std::optional<std::string>
{
    if (search.running()) {
        return "Cancel the current search before loading a puzzle space";
    }

    puzzle_space_io_result io = ::load_puzzle_space_file(path);
    if (!io.result) {
        return io.error.empty() ? "Failed to load puzzle space" : io.error;
    }

    puzzle_space_results = std::move(*io.result);
    puzzle_space_selection = 0;
    sort_puzzle_space_clusters(puzzle_space_results->clusters, puzzle_space_sort_mode);
    mode = search_mode::puzzle_space;

    if (!puzzle_space_results->clusters.empty()) {
        load_puzzle_space_result(0);
    }
    return std::nullopt;
}

auto state_manager::get_puzzle_space_selection() const -> size_t
{
    return puzzle_space_selection;
}

auto state_manager::set_puzzle_space_selection(const size_t index) -> void
{
    if (!puzzle_space_results || puzzle_space_results->clusters.empty()) {
        puzzle_space_selection = 0;
        return;
    }
    puzzle_space_selection = std::min(index, puzzle_space_results->clusters.size() - 1);
}

auto state_manager::get_puzzle_space_sort() const -> puzzle_space_sort
{
    return puzzle_space_sort_mode;
}

auto state_manager::set_puzzle_space_sort(const puzzle_space_sort sort) -> void
{
    if (sort == puzzle_space_sort_mode) {
        return;
    }
    puzzle_space_sort_mode = sort;

    if (!puzzle_space_results || puzzle_space_results->clusters.empty()) {
        puzzle_space_selection = 0;
        return;
    }

    sort_puzzle_space_clusters(puzzle_space_results->clusters, puzzle_space_sort_mode);
    puzzle_space_selection = 0;
    request_load_puzzle_space_result(0);
}

auto state_manager::load_puzzle_space_result(const size_t index) -> bool
{
    pending_result_load.reset();
    if (!puzzle_space_results || index >= puzzle_space_results->clusters.size()) {
        return false;
    }

    const puzzle_space_cluster& cluster = puzzle_space_results->clusters[index];
    puzzle_space_selection = index;
    const std::string label = std::format("# Result {}", index + 1);
    return load_sliding_block(cluster.hardest, label);
}

auto state_manager::request_load_puzzle_space_result(const size_t index) -> void
{
    pending_result_load = index;
}

auto state_manager::load_sliding_block(const puzzle& state, std::string label) -> bool
{
    if (search.running() && search.kind() == ::search_kind::state_space) {
        search.cancel();
    }

    // Loading another sliding-block state can happen in place. This keeps the presenter (and the
    // puzzle-space search parameters stored in it) alive while the result spinner is used.
    if (auto* sliding = dynamic_cast<sliding_block_model*>(&model())) {
        const state_id id = sliding->set_state(state);
        graph.reset_to(id, model());
        presenter().reset();
    } else {
        std::unique_ptr<puzzle_kind> kind = registry.create("sliding_block");
        if (!kind) {
            return false;
        }

        auto* created = dynamic_cast<sliding_block_model*>(kind->model.get());
        if (created == nullptr) {
            return false;
        }

        const state_id id = created->find_or_insert(state);
        active_puzzle.replace(std::move(kind));
        graph.reset_to(id, active_puzzle.model());
    }

    edited = false;
    custom_state = true;
    custom_label = std::move(label);
    return true;
}

auto state_manager::has_custom_state() const -> bool
{
    return custom_state;
}

auto state_manager::get_custom_label() const -> const std::string&
{
    return custom_label;
}

auto state_manager::get_search_origin_comment() const -> const std::string&
{
    return search_origin_comment;
}

auto state_manager::get_search_origin_status() const -> const std::string&
{
    return search_origin_status;
}

auto state_manager::search_origin_is_custom() const -> bool
{
    return search_origin_custom;
}

auto state_manager::current_state_id() const -> state_id
{
    return graph.get_current_index();
}

auto state_manager::get_current_state_id() const -> state_id
{
    return graph.get_current_index();
}

auto state_manager::validate_current() const -> std::optional<std::string>
{
    return codec().validate(model(), get_current_state_id());
}

auto state_manager::describe_current() const -> std::string
{
    return model().describe(get_current_state_id());
}

auto state_manager::get_current_index() const -> size_t
{
    return graph.get_current_index();
}

auto state_manager::get_starting_index() const -> size_t
{
    return graph.get_starting_index();
}

auto state_manager::get_state_count() const -> size_t
{
    return model().state_count();
}

auto state_manager::get_target_count() const -> size_t
{
    return graph.get_target_count();
}

auto state_manager::get_link_count() const -> size_t
{
    return graph.get_link_count();
}

auto state_manager::get_path_length() const -> size_t
{
    return graph.get_path_length();
}

auto state_manager::get_links() const -> const std::vector<spring>&
{
    return graph.get_links();
}

auto state_manager::get_winning_indices() const -> const boost::unordered_flat_set<size_t>&
{
    return graph.get_winning_indices();
}

auto state_manager::get_visit_counts() const -> const std::vector<int>&
{
    return graph.get_visit_counts();
}

auto state_manager::get_winning_path() const -> const std::vector<size_t>&
{
    return graph.get_winning_path();
}

auto state_manager::get_path_indices() const -> const boost::unordered_flat_set<size_t>&
{
    return graph.get_path_indices();
}

auto state_manager::get_current_visits() const -> int
{
    return graph.get_current_visits();
}

auto state_manager::get_current_preset() const -> size_t
{
    return current_preset;
}

auto state_manager::get_preset_count() const -> size_t
{
    return presets.size();
}

auto state_manager::get_current_preset_comment() const -> const std::string&
{
    if (custom_state) {
        return custom_label;
    }
    if (presets.empty() || current_preset >= presets.size() ||
        presets[current_preset].type != current_puzzle_type()) {
        static std::string fallback;
        fallback = std::format("# {}", current_puzzle_type());
        return fallback;
    }
    return presets[current_preset].comment;
}

auto state_manager::has_history() const -> bool
{
    return graph.has_history();
}

auto state_manager::has_distances() const -> bool
{
    return graph.has_distances();
}

auto state_manager::get_distances() const -> const std::vector<int>&
{
    return graph.get_distances();
}

auto state_manager::get_total_moves() const -> size_t
{
    return graph.get_total_moves();
}

auto state_manager::is_graph_complete() const -> bool
{
    return graph.is_complete();
}

auto state_manager::was_edited() const -> bool
{
    return edited;
}
