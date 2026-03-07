#ifndef STATE_MANAGER_HPP_
#define STATE_MANAGER_HPP_

#include "graph_distances.hpp"
#include "load_save.hpp"
#include "cpu_layout_engine.hpp"
#include "puzzle.hpp"

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>

class state_manager
{
private:
    cpu_layout_engine& physics;

    std::string preset_file;
    size_t current_preset = 0;
    std::vector<puzzle> preset_states = {puzzle(4, 5, 0, 0, true, false)};
    std::vector<std::string> preset_comments = {"# Empty"};

    // State storage (store states twice for bidirectional lookup).
    // Everything else should only store indices to state_pool.

    std::vector<puzzle> state_pool;  // Indices are equal to mass_springs mass indices
    puzzlemap<size_t> state_indices; // Maps states to indices
    std::vector<spring> links;       // Indices are equal to mass_springs springs indices

    graph_distances node_target_distances; // Buffered and reused if the graph doesn't change
    boost::unordered_flat_set<size_t> winning_indices; // Indices of all states where the board is solved
    std::vector<size_t> winning_path; // Ordered list of node indices leading to the nearest solved state
    boost::unordered_flat_set<size_t> path_indices; // For faster lookup if a vertex is part of the path in renderer

    std::vector<size_t> move_history;                    // Moves between the starting state and the current state
    boost::unordered_flat_map<size_t, int> visit_counts; // How often each state was visited

    size_t starting_state_index = 0;
    size_t current_state_index = 0;
    size_t previous_state_index = 0;

    int total_moves = 0;
    bool edited = false;

public:
    state_manager(cpu_layout_engine& _physics, const std::string& _preset_file)
        : physics(_physics), preset_file(_preset_file)
    {
        reload_preset_file();
    }

    NO_COPY_NO_MOVE(state_manager);

private:
    /**
     * Inserts a board state into the state_manager and the physics system.
     * States should only be inserted using this function to keep both systems in sync.
     * The function checks for duplicates before insertion.
     *
     * @param state State to insert
     * @return Index of insertion (or existing index if duplicate)
     */
    auto synced_try_insert_state(const puzzle& state) -> size_t;

    /**
     * Inserts a state link into the state_manager and the physics system.
     * Links should only be inserted using this function to keep both systems in sync.
     * The function does not check for duplicates before insertion.
     *
     * @param first_index Index of the first linked state
     * @param second_index Index of the second linked state
     */
    auto synced_insert_link(size_t first_index, size_t second_index) -> void;

    /**
     * Inserts an entire statespace into the state_manager and the physics system.
     * If inserting many states and links in bulk, this function should always be used
     * to not stress the physics command mutex.
     * The function does not check for duplicates before insertion.
     *
     * @param states List of states to insert
     * @param _links List of links to insert
     */
    auto synced_insert_statespace(const std::vector<puzzle>& states, const std::vector<spring>& _links) -> void;

    /**
     * Clears all states and links (and related) from the state_manager and the physics system.
     * Note that this leaves any dangling indices (e.g., current_state_index) in an invalid state.
     */
    auto synced_clear_statespace() -> void;

public:
    // Presets
    auto save_current_to_preset_file(const std::string& preset_comment) -> void;
    auto reload_preset_file() -> void;
    auto load_preset(size_t preset) -> void;
    auto load_previous_preset() -> void;
    auto load_next_preset() -> void;

    // Update current_state
    auto update_current_state(const puzzle& p) -> void;
    auto edit_starting_state(const puzzle& p) -> void;
    auto goto_starting_state() -> void;
    auto goto_optimal_next_state() -> void;
    auto goto_previous_state() -> void;
    auto goto_most_distant_state() -> void;
    auto goto_closest_target_state() -> void;

    // Update graph
    auto populate_graph() -> void;
    auto clear_graph_and_add_current(const puzzle& p) -> void;
    auto clear_graph_and_add_current() -> void;
    auto populate_winning_indices() -> void;
    auto populate_node_target_distances() -> void;
    auto populate_winning_path() -> void;

    // Index mapping
    [[nodiscard]] auto get_index(const puzzle& state) const -> size_t;
    [[nodiscard]] auto get_current_index() const -> size_t;
    [[nodiscard]] auto get_starting_index() const -> size_t;
    [[nodiscard]] auto get_state(size_t index) const -> const puzzle&;
    [[nodiscard]] auto get_current_state() const -> const puzzle&;
    [[nodiscard]] auto get_starting_state() const -> const puzzle&;

    // Access
    [[nodiscard]] auto get_state_count() const -> size_t;
    [[nodiscard]] auto get_target_count() const -> size_t;
    [[nodiscard]] auto get_link_count() const -> size_t;
    [[nodiscard]] auto get_path_length() const -> size_t;
    [[nodiscard]] auto get_links() const -> const std::vector<spring>&;
    [[nodiscard]] auto get_winning_indices() const -> const boost::unordered_flat_set<size_t>&;
    [[nodiscard]] auto get_visit_counts() const -> const boost::unordered_flat_map<size_t, int>&;
    [[nodiscard]] auto get_winning_path() const -> const std::vector<size_t>&;
    [[nodiscard]] auto get_path_indices() const -> const boost::unordered_flat_set<size_t>&;
    [[nodiscard]] auto get_current_visits() const -> int;
    [[nodiscard]] auto get_current_preset() const -> size_t;
    [[nodiscard]] auto get_preset_count() const -> size_t;
    [[nodiscard]] auto get_current_preset_comment() const -> const std::string&;
    [[nodiscard]] auto has_history() const -> bool;
    [[nodiscard]] auto has_distances() const -> bool;
    [[nodiscard]] auto get_distances() const -> std::vector<int>;
    [[nodiscard]] auto get_total_moves() const -> size_t;
    [[nodiscard]] auto was_edited() const -> bool;
};

#endif