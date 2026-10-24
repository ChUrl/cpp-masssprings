#ifndef SUDOKU4_MODEL_HPP_
#define SUDOKU4_MODEL_HPP_

#include "state_model.hpp"

#include <boost/unordered/unordered_flat_map.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/**
 * 4x4 Sudoku with 2x2 boxes.
 *
 * A state stores the sixteen cell values as four bits each in a u64. A cell is 0 when empty and
 * 1..4 otherwise. Givens are stored separately in the model; they can never be changed by a move.
 *
 * Moves fill an empty non-given cell with a digit or clear a filled non-given cell. The clear move
 * makes the transition graph undirected, so the generic shortest-path code can be reused and the
 * user can correct a wrong entry without resetting the whole puzzle.
 */
class sudoku4_model : public state_model
{
public:
    static constexpr int SIZE = 4;
    static constexpr int BOX = 2;
    static constexpr int CELLS = SIZE * SIZE;
    static constexpr int DIGITS = 4;

private:
    std::uint16_t given_mask_ = 0;
    std::uint64_t given_code_ = 0;

    std::vector<std::uint64_t> states;
    boost::unordered_flat_map<std::uint64_t, size_t> indices;

public:
    sudoku4_model();

    // Configures the givens and clears all stored states. Throws on inconsistent givens.
    auto configure(std::uint16_t given_mask, std::uint64_t given_code) -> void;

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

    [[nodiscard]] auto code(state_id id) const -> std::uint64_t;
    [[nodiscard]] auto value(state_id id, int cell) const -> int;
    [[nodiscard]] auto empty_count(state_id id) const -> int;
    [[nodiscard]] auto is_given(int cell) const -> bool;
    [[nodiscard]] auto given_value(int cell) const -> int;
    [[nodiscard]] auto given_mask() const -> std::uint16_t;
    [[nodiscard]] auto given_code() const -> std::uint64_t;

    [[nodiscard]] auto find_or_insert(std::uint64_t state) -> state_id;
    [[nodiscard]] auto try_set(state_id id, int cell, int digit) -> std::optional<state_id>;

    [[nodiscard]] static auto is_consistent_code(std::uint64_t state, std::uint16_t given_mask,
                                                 std::uint64_t given_code) -> bool;

private:
    [[nodiscard]] static auto value_from_code(std::uint64_t state, int cell) -> int;
    [[nodiscard]] static auto set_cell(std::uint64_t state, int cell, int digit) -> std::uint64_t;
    [[nodiscard]] auto can_place(std::uint64_t state, int cell, int digit) const -> bool;
    [[nodiscard]] auto insert_code(std::uint64_t state) -> std::pair<state_id, bool>;
    [[nodiscard]] auto givens_string() const -> std::string;
    [[nodiscard]] auto state_string(std::uint64_t state) const -> std::string;
};

#endif
