#ifndef SLIDING_BLOCK_MODEL_HPP_
#define SLIDING_BLOCK_MODEL_HPP_

#include "puzzle.hpp"
#include "state_model.hpp"

#include <optional>
#include <string>
#include <vector>

/**
 * The existing sliding-block puzzle, wrapped in the generic state_model interface.
 */
class sliding_block_model : public state_model
{
private:
    std::vector<puzzle> states;
    puzzlemap<size_t> indices;

public:
    sliding_block_model();

    [[nodiscard]] auto state_count() const -> size_t override;
    [[nodiscard]] auto memory_bytes() const -> size_t override;
    [[nodiscard]] auto is_goal(state_id id) const -> bool override;
    [[nodiscard]] auto is_valid(state_id id) const -> bool override;
    [[nodiscard]] auto describe(state_id id) const -> std::string override;
    [[nodiscard]] auto clone() const -> std::unique_ptr<state_model> override;

    [[nodiscard]] auto initial_state() -> state_id override;
    [[nodiscard]] auto reset_to(state_id id) -> state_id override;
    [[nodiscard]] auto explore(state_id start, state_id current, threadpool pool,
                               exploration_observer* observer = nullptr)
        -> exploration_result override;

    [[nodiscard]] auto get(state_id id) const -> const puzzle&;
    [[nodiscard]] auto find_or_insert(const puzzle& state) -> state_id;
    // Replaces all stored states with the given state and returns its new id.
    auto set_state(const puzzle& state) -> state_id;

    [[nodiscard]] auto try_move(state_id id, int x, int y, dir direction)
        -> std::optional<state_id>;
    [[nodiscard]] auto try_add_block(state_id id, block b) -> std::optional<state_id>;
    [[nodiscard]] auto try_remove_block(state_id id, int x, int y) -> std::optional<state_id>;
    [[nodiscard]] auto try_set_goal(state_id id, int x, int y) -> std::optional<state_id>;
    [[nodiscard]] auto try_toggle_target(state_id id, int x, int y) -> std::optional<state_id>;
    [[nodiscard]] auto try_toggle_wall(state_id id, int x, int y) -> std::optional<state_id>;
    [[nodiscard]] auto try_add_wall(state_id id, int x, int y) -> std::optional<state_id>;
    [[nodiscard]] auto try_add_wall_block(state_id id, block b) -> std::optional<state_id>;
    [[nodiscard]] auto try_remove_wall(state_id id, int x, int y) -> std::optional<state_id>;
    [[nodiscard]] auto toggle_restricted(state_id id) -> std::optional<state_id>;
    [[nodiscard]] auto clear_goal(state_id id) -> std::optional<state_id>;
    [[nodiscard]] auto try_add_column(state_id id) -> std::optional<state_id>;
    [[nodiscard]] auto try_remove_column(state_id id) -> std::optional<state_id>;
    [[nodiscard]] auto try_add_row(state_id id) -> std::optional<state_id>;
    [[nodiscard]] auto try_remove_row(state_id id) -> std::optional<state_id>;

private:
    auto insert_puzzle(const puzzle& state) -> state_id;
};

#endif
