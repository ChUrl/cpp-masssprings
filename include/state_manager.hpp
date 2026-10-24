#ifndef STATE_MANAGER_HPP_
#define STATE_MANAGER_HPP_

#include "cpu_layout_engine.hpp"
#include "preset.hpp"
#include "puzzle_context.hpp"
#include "puzzle_registry.hpp"
#include "puzzle_space.hpp"
#include "search_controller.hpp"
#include "state_graph.hpp"
#include "state_model.hpp"

#include <boost/unordered/unordered_flat_set.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

enum class search_mode
{
    state_space,
    puzzle_space,
};

class state_manager
{
private:
    cpu_layout_engine& physics;
    puzzle_registry& registry;

    std::string preset_file;
    size_t current_preset = 0;
    std::vector<preset_entry> presets;

    puzzle_context active_puzzle;
    state_graph graph;

    bool edited = false;
    search_mode mode = search_mode::state_space;

    search_controller search;
    std::optional<puzzle_space_result> puzzle_space_results;
    size_t puzzle_space_selection = 0;
    puzzle_space_sort puzzle_space_sort_mode = puzzle_space_sort::most_moves;
    std::optional<size_t> pending_result_load;

    bool custom_state = false;
    std::string custom_label;

    std::string search_origin_comment;
    std::string search_origin_status;
    bool search_origin_custom = false;

public:
    state_manager(cpu_layout_engine& _physics, puzzle_registry& _registry,
                  const std::string& _preset_file);

    NO_COPY_NO_MOVE(state_manager);

private:
    auto switch_to_preset(const preset_entry& preset) -> bool;
    // Moves to the next/previous preset of the active puzzle type, wrapping at
    // both ends. Does nothing when the type has no presets.
    auto step_preset(int direction) -> void;

public:
    // Puzzle context
    [[nodiscard]] auto context() -> puzzle_context&;
    [[nodiscard]] auto context() const -> const puzzle_context&;
    [[nodiscard]] auto model() -> state_model&;
    [[nodiscard]] auto model() const -> const state_model&;
    [[nodiscard]] auto presenter() -> puzzle_presenter&;
    [[nodiscard]] auto presenter() const -> const puzzle_presenter&;
    [[nodiscard]] auto codec() -> puzzle_codec&;
    [[nodiscard]] auto codec() const -> const puzzle_codec&;
    [[nodiscard]] auto generation() const -> std::uint64_t;
    [[nodiscard]] auto current_puzzle_type() const -> const std::string&;
    [[nodiscard]] auto available_puzzle_types() const -> std::vector<std::string>;

    auto switch_puzzle(const std::string& type) -> bool;

    // Presets
    auto save_current_to_preset_file(const std::string& preset_comment) -> void;
    auto reload_preset_file() -> void;
    auto load_preset(size_t preset) -> void;
    auto load_previous_preset() -> void;
    auto load_next_preset() -> void;

    // Preset cycling in the menu is limited to the presets of the active puzzle
    // type; the type itself is only changed through the puzzle spinner.
    [[nodiscard]] auto get_type_preset_count() const -> size_t;
    [[nodiscard]] auto get_type_preset_position() const -> size_t;

    // Update current_state
    auto update_current_state(state_id id) -> void;
    auto edit_starting_state(state_id id) -> void;
    auto goto_starting_state() -> void;
    auto goto_optimal_next_state() -> void;
    auto goto_previous_state() -> void;
    auto goto_most_distant_state() -> void;
    auto goto_closest_target_state() -> void;

    // Update graph
    auto populate_graph() -> void;
    auto clear_graph_and_add_current() -> void;
    auto populate_winning_indices() -> void;
    auto populate_node_target_distances() -> void;
    auto populate_winning_path() -> void;

    // Asynchronous searches
    auto start_state_space_search() -> bool;
    auto start_puzzle_space_search(size_t max_blocks, int min_moves, size_t min_blocks = 2,
                                   const wall_search_options& walls = {}) -> bool;
    auto cancel_search() -> void;
    auto poll_search() -> void;
    [[nodiscard]] auto get_search_snapshot() const -> ::search_snapshot;
    [[nodiscard]] auto search_is_running() const -> bool;
    [[nodiscard]] auto get_search_kind() const -> ::search_kind;
    [[nodiscard]] auto get_search_mode() const -> search_mode;
    auto set_search_mode(search_mode next) -> void;
    [[nodiscard]] auto search_thread_count() const -> int;

    // Rough tracked memory estimate.
    [[nodiscard]] auto memory_usage() const -> ::memory_usage;
    [[nodiscard]] auto memory_bytes() const -> size_t;

    // Puzzle-space results
    [[nodiscard]] auto build_puzzle_space_config(size_t max_blocks, int min_moves,
                                                 size_t min_blocks = 2,
                                                 const wall_search_options& walls = {}) const
        -> puzzle_space_config_result;
    [[nodiscard]] auto get_puzzle_space_results() const -> const puzzle_space_result*;
    auto save_puzzle_space_file(const std::string& path) -> std::optional<std::string>;
    auto load_puzzle_space_file(const std::string& path) -> std::optional<std::string>;
    [[nodiscard]] auto get_puzzle_space_selection() const -> size_t;
    auto set_puzzle_space_selection(size_t index) -> void;
    [[nodiscard]] auto get_puzzle_space_sort() const -> puzzle_space_sort;
    auto set_puzzle_space_sort(puzzle_space_sort sort) -> void;
    auto load_puzzle_space_result(size_t index) -> bool;
    auto request_load_puzzle_space_result(size_t index) -> void;

    // Load an arbitrary sliding-block state that is not a preset.
    auto load_sliding_block(const puzzle& state, std::string label) -> bool;
    [[nodiscard]] auto has_custom_state() const -> bool;
    [[nodiscard]] auto get_custom_label() const -> const std::string&;
    [[nodiscard]] auto get_search_origin_comment() const -> const std::string&;
    [[nodiscard]] auto get_search_origin_status() const -> const std::string&;
    [[nodiscard]] auto search_origin_is_custom() const -> bool;

    // Current state helpers
    [[nodiscard]] auto current_state_id() const -> state_id;
    [[nodiscard]] auto get_current_state_id() const -> state_id;
    [[nodiscard]] auto validate_current() const -> std::optional<std::string>;
    [[nodiscard]] auto describe_current() const -> std::string;

    // Index mapping
    [[nodiscard]] auto get_current_index() const -> size_t;
    [[nodiscard]] auto get_starting_index() const -> size_t;

    // Access
    [[nodiscard]] auto get_state_count() const -> size_t;
    [[nodiscard]] auto get_target_count() const -> size_t;
    [[nodiscard]] auto get_link_count() const -> size_t;
    [[nodiscard]] auto get_path_length() const -> size_t;
    [[nodiscard]] auto get_links() const -> const std::vector<spring>&;
    [[nodiscard]] auto get_winning_indices() const -> const boost::unordered_flat_set<size_t>&;
    [[nodiscard]] auto get_visit_counts() const -> const std::vector<int>&;
    [[nodiscard]] auto get_winning_path() const -> const std::vector<size_t>&;
    [[nodiscard]] auto get_path_indices() const -> const boost::unordered_flat_set<size_t>&;
    [[nodiscard]] auto get_current_visits() const -> int;
    [[nodiscard]] auto get_current_preset() const -> size_t;
    [[nodiscard]] auto get_preset_count() const -> size_t;
    [[nodiscard]] auto get_current_preset_comment() const -> const std::string&;
    [[nodiscard]] auto has_history() const -> bool;
    [[nodiscard]] auto has_distances() const -> bool;
    [[nodiscard]] auto get_distances() const -> const std::vector<int>&;
    [[nodiscard]] auto get_total_moves() const -> size_t;
    [[nodiscard]] auto is_graph_complete() const -> bool;
    [[nodiscard]] auto was_edited() const -> bool;
};

#endif
