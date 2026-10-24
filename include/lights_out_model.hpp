#ifndef LIGHTS_OUT_MODEL_HPP_
#define LIGHTS_OUT_MODEL_HPP_

#include "lights_out_rules.hpp"
#include "state_model.hpp"

#include <boost/unordered/unordered_flat_map.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

/**
 * Rules-driven Lights Out.
 *
 * States are encoded as base-N digits in a 64-bit integer. The default rules are the classic 4x4
 * binary game; ternary, wrap-around, diagonal, restricted-cell and target-pattern variants are
 * configured through lights_out_rules.
 */
class lights_out_model : public state_model
{
public:
    static constexpr int SIZE = 4;
    static constexpr int CELL_COUNT = SIZE * SIZE;
    static constexpr std::uint16_t ALL_ON = 0xFFFF;

private:
    lights_out_rules rules_;
    std::vector<std::uint64_t> states;
    boost::unordered_flat_map<std::uint64_t, size_t> indices;
    std::vector<std::uint64_t> powers;
    std::vector<std::uint8_t> move_deltas;
    std::vector<int> allowed_cells;

public:
    lights_out_model();
    explicit lights_out_model(const lights_out_rules& rules);

    auto configure(const lights_out_rules& rules) -> void;
    [[nodiscard]] auto rules() const -> const lights_out_rules&;

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
    [[nodiscard]] auto directed_edges() const -> bool override;

    [[nodiscard]] auto code(state_id id) const -> std::uint64_t;
    [[nodiscard]] auto mask(state_id id) const -> std::uint16_t;
    [[nodiscard]] auto value(state_id id, int cell) const -> int;
    [[nodiscard]] auto on_count(state_id id) const -> int;
    [[nodiscard]] auto find_or_insert(std::uint64_t state) -> state_id;
    [[nodiscard]] auto try_toggle(state_id id, int x, int y) -> state_id;
    [[nodiscard]] auto random_state() -> state_id;
    [[nodiscard]] auto solved_state() -> state_id;
    [[nodiscard]] auto all_on_code() const -> std::uint64_t;

    [[nodiscard]] static auto toggle_mask(int x, int y) -> std::uint16_t;

private:
    auto build_tables() -> void;
    auto apply_press(std::uint64_t state, int cell) const -> std::uint64_t;
    auto state_string(std::uint64_t state) const -> std::string;
    auto pressable_string() const -> std::string;
    [[nodiscard]] auto insert_code(std::uint64_t state) -> std::pair<state_id, bool>;
};

#endif
