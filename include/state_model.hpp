#ifndef STATE_MODEL_HPP_
#define STATE_MODEL_HPP_

#include "config.hpp"
#include "cpu_spring_system.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// A state is addressed by an index into the model's own storage. The graph layer only ever sees
// these indices, so it does not need to know how a concrete puzzle is represented.
using state_id = size_t;

struct exploration_result
{
    std::vector<spring> links;
    state_id start = 0;
    state_id current = 0;
    bool complete = true;
    bool directed = false;
    bool cancelled = false;
};

/**
 * Optional observer for a running exploration. Implementations are called periodically from the
 * worker thread with the number of states discovered so far. Returning false asks the exploration
 * to stop as soon as possible.
 */
class exploration_observer
{
public:
    virtual ~exploration_observer() = default;
    virtual auto on_states_explored(size_t states) -> bool = 0;
};

/**
 * Puzzle-specific state storage and rules.
 *
 * Implementations own their concrete state representation, the hash map used for deduplication,
 * and the exploration algorithm. Keeping the state values inside the model means the generic
 * graph layer never has to store or hash puzzle-specific data.
 */
class state_model
{
public:
    virtual ~state_model() = default;

    [[nodiscard]] virtual auto state_count() const -> size_t = 0;
    // Rough size of the model's own maps and vectors, excluding the object itself.
    [[nodiscard]] virtual auto memory_bytes() const -> size_t
    {
        return 0;
    }
    [[nodiscard]] virtual auto is_goal(state_id id) const -> bool = 0;
    [[nodiscard]] virtual auto is_valid(state_id id) const -> bool = 0;
    [[nodiscard]] virtual auto describe(state_id id) const -> std::string = 0;

    // Deep copy of the model and its current storage. Used to explore in the background without
    // mutating the model the UI is currently rendering.
    [[nodiscard]] virtual auto clone() const -> std::unique_ptr<state_model> = 0;

    // Creates the default starting state (if necessary) and returns its id.
    [[nodiscard]] virtual auto initial_state() -> state_id = 0;

    // Rebuilds the model so that it contains only the given state. Returns the new id (usually 0).
    [[nodiscard]] virtual auto reset_to(state_id id) -> state_id = 0;

    // Explores the state space from start. Implementations may return a bounded/local subgraph and
    // set complete to false. The current state id must be valid in the returned graph. If the
    // observer returns false, implementations stop early and set cancelled.
    [[nodiscard]] virtual auto explore(state_id start, state_id current, threadpool pool,
                                       exploration_observer* observer = nullptr)
        -> exploration_result = 0;

    // Whether transitions are directed. Ternary Lights Out moves are not involutions, so the
    // graph must be treated as directed for distance/solution calculations.
    [[nodiscard]] virtual auto directed_edges() const -> bool
    {
        return false;
    }

    // Optional puzzle-specific navigation. Models that can solve themselves can override these;
    // otherwise the generic graph distances are used.
    [[nodiscard]] virtual auto has_solution_path() const -> bool
    {
        return false;
    }

    [[nodiscard]] virtual auto solution_path(state_id id) const -> std::vector<state_id>
    {
        return {};
    }

    [[nodiscard]] virtual auto next_optimal(state_id id) const -> std::optional<state_id>
    {
        return std::nullopt;
    }
};

#endif
