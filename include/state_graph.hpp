#ifndef STATE_GRAPH_HPP_
#define STATE_GRAPH_HPP_

#include "config.hpp"
#include "cpu_spring_system.hpp"
#include "graph_distances.hpp"
#include "state_model.hpp"
#include "util.hpp"

#include <boost/unordered/unordered_flat_set.hpp>

#include <cstddef>
#include <optional>
#include <vector>

class cpu_layout_engine;

/**
 * Generic state-graph bookkeeping.
 *
 * This class knows about node indices, links, distances, winning states, move history and the
 * physics engine, but nothing about the concrete puzzle state. All puzzle-specific information
 * comes from the state_model passed into the methods that need it.
 */
class state_graph
{
private:
    cpu_layout_engine& physics;

    std::vector<spring> links;
    graph_distances node_target_distances;
    boost::unordered_flat_set<size_t> winning_indices;
    std::vector<size_t> winning_path;
    boost::unordered_flat_set<size_t> path_indices;

    std::vector<size_t> move_history;
    std::vector<int> visit_counts;

    size_t starting_state_index = static_cast<size_t>(-1);
    size_t current_state_index = static_cast<size_t>(-1);
    size_t previous_state_index = static_cast<size_t>(-1);

    size_t known_state_count = 0;
    bool graph_complete = true;
    bool directed = false;
    int total_moves = 0;

public:
    explicit state_graph(cpu_layout_engine& _physics);
    NO_COPY_NO_MOVE(state_graph);

    // Clears all graph data and queues a clear command for the physics engine.
    auto clear() -> void;

    // Resets the graph to a single state. The model must already contain only that state.
    auto reset_to(state_id id, const state_model& model) -> void;

    // Inserts a new state into the graph and physics engine.
    auto add_state() -> size_t;

    // Inserts a link into the graph and physics engine.
    auto add_link(size_t first_index, size_t second_index) -> void;

    // Moves to an existing state. If the model grew since the last call, a link from the previous
    // current state is inserted automatically.
    auto update_current(state_id id, const state_model& model) -> void;

    // Rebuilds the graph from the model's exploration result.
    auto populate_graph(state_model& model, state_id start, state_id current, threadpool pool,
                        exploration_observer* observer = nullptr) -> void;

    // Installs an exploration result produced elsewhere (e.g. by a background job). The model must
    // already contain the explored states.
    auto apply_exploration_result(exploration_result result, const state_model& model) -> void;

    auto populate_winning_indices(const state_model& model) -> void;
    auto populate_node_target_distances() -> void;
    auto populate_winning_path() -> void;

    auto reset_history() -> void;
    auto reset_visits() -> void;
    auto pop_history_back() -> void;

    [[nodiscard]] auto get_move_history() const -> const std::vector<size_t>&;

    [[nodiscard]] auto optimal_next(size_t index) const -> std::optional<size_t>;
    [[nodiscard]] auto nearest_target(size_t index) const -> std::optional<size_t>;
    [[nodiscard]] auto most_distant() const -> std::optional<size_t>;

    // Rough size of the graph's own vectors and sets (physics arrays excluded).
    [[nodiscard]] auto memory_bytes() const -> size_t;

    // Access
    [[nodiscard]] auto get_current_index() const -> size_t;
    [[nodiscard]] auto get_starting_index() const -> size_t;
    [[nodiscard]] auto get_previous_index() const -> size_t;
    [[nodiscard]] auto get_link_count() const -> size_t;
    [[nodiscard]] auto get_target_count() const -> size_t;
    [[nodiscard]] auto get_path_length() const -> size_t;
    [[nodiscard]] auto get_links() const -> const std::vector<spring>&;
    [[nodiscard]] auto get_winning_indices() const -> const boost::unordered_flat_set<size_t>&;
    [[nodiscard]] auto get_visit_counts() const -> const std::vector<int>&;
    [[nodiscard]] auto get_winning_path() const -> const std::vector<size_t>&;
    [[nodiscard]] auto get_path_indices() const -> const boost::unordered_flat_set<size_t>&;
    [[nodiscard]] auto get_current_visits() const -> int;
    [[nodiscard]] auto has_history() const -> bool;
    [[nodiscard]] auto has_distances() const -> bool;
    [[nodiscard]] auto get_distances() const -> const std::vector<int>&;
    [[nodiscard]] auto get_total_moves() const -> size_t;
    [[nodiscard]] auto is_complete() const -> bool;
};

#endif
