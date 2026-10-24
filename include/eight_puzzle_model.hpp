#ifndef EIGHT_PUZZLE_MODEL_HPP_
#define EIGHT_PUZZLE_MODEL_HPP_

#include "state_model.hpp"

#include <boost/unordered/unordered_flat_map.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/**
 * The classic 3x3 sliding-tile puzzle with eight numbered tiles and one blank.
 *
 * A state packs the nine cell values into four bits each (36 bits). The blank is 0; the solved
 * state is 1..8 followed by the blank. Only half of the 9! permutations are reachable from a given
 * start, so a full exploration contains 181,440 states.
 */
namespace eight_puzzle_detail
{
constexpr auto pack(std::array<int, 9> values) -> std::uint64_t
{
    std::uint64_t code = 0;
    for (int cell = 0; cell < 9; ++cell) {
        code |= static_cast<std::uint64_t>(values[static_cast<size_t>(cell)])
            << (4u * static_cast<unsigned int>(cell));
    }
    return code;
}
} // namespace eight_puzzle_detail

class eight_puzzle_model : public state_model
{
public:
    static constexpr int SIZE = 3;
    static constexpr int CELLS = SIZE * SIZE;
    static constexpr int TILE_COUNT = CELLS - 1;

public:
    static constexpr std::uint64_t GOAL_CODE =
        eight_puzzle_detail::pack({1, 2, 3, 4, 5, 6, 7, 8, 0});
    static constexpr std::uint64_t DEFAULT_START_CODE =
        eight_puzzle_detail::pack({8, 6, 7, 2, 5, 4, 3, 0, 1});

private:
    std::vector<std::uint64_t> states;
    boost::unordered_flat_map<std::uint64_t, size_t> indices;

public:
    eight_puzzle_model();

    auto configure() -> void;

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
    [[nodiscard]] auto blank_cell(state_id id) const -> int;
    [[nodiscard]] auto manhattan(state_id id) const -> int;
    [[nodiscard]] auto is_solvable(state_id id) const -> bool;

    [[nodiscard]] auto find_or_insert(std::uint64_t state) -> state_id;
    [[nodiscard]] auto try_move_tile(state_id id, int cell) -> std::optional<state_id>;
    [[nodiscard]] auto try_move_blank(state_id id, int dx, int dy) -> std::optional<state_id>;
    [[nodiscard]] auto random_state() -> state_id;
    [[nodiscard]] auto solved_state() -> state_id;
    [[nodiscard]] auto default_state() -> state_id;

    [[nodiscard]] static auto is_solvable_code(std::uint64_t state) -> bool;

private:
    [[nodiscard]] static auto value_from_code(std::uint64_t state, int cell) -> int;
    [[nodiscard]] static auto blank_from_code(std::uint64_t state) -> int;
    [[nodiscard]] static auto swap_cells(std::uint64_t state, int first, int second)
        -> std::uint64_t;
    [[nodiscard]] auto insert_code(std::uint64_t state) -> std::pair<state_id, bool>;
    [[nodiscard]] auto state_string(std::uint64_t state) const -> std::string;
};

#endif
