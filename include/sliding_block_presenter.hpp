#ifndef SLIDING_BLOCK_PRESENTER_HPP_
#define SLIDING_BLOCK_PRESENTER_HPP_

#include "puzzle_presenter.hpp"
#include "sliding_block_model.hpp"

class state_manager;

class sliding_block_presenter : public puzzle_presenter
{
private:
    sliding_block_model& model;

    mutable int hov_x = -1;
    mutable int hov_y = -1;
    int sel_x = 0;
    int sel_y = 0;

    bool edit_mode = false;
    bool has_block_add_xy = false;
    int block_add_x = -1;
    int block_add_y = -1;

    int puzzle_space_max_blocks = 0;
    int puzzle_space_min_blocks = 0;
    int puzzle_space_min_moves = 10;
    bool puzzle_space_wall_inventory = false;
    int puzzle_space_min_walls = 0;
    int puzzle_space_max_walls = 0;

public:
    explicit sliding_block_presenter(sliding_block_model& _model);

    [[nodiscard]] auto editing() const -> bool override;
    auto toggle_editing() -> void override;
    auto reset() -> void override;

    [[nodiscard]] auto board_columns(const state_model& model, state_id id) const -> int override;
    [[nodiscard]] auto board_rows(const state_model& model, state_id id) const -> int override;

    auto draw_board(const puzzle_canvas& canvas, state_manager& state) const -> void override;
    auto draw_puzzle_controls(puzzle_control_context& context, int row, Color color,
                              state_manager& state, input_handler& input) -> int override;
    auto draw_edit_controls(puzzle_control_context& context, int row, Color color,
                            state_manager& state) -> int override;
    [[nodiscard]] auto supports_puzzle_space() const -> bool override;
    auto draw_puzzle_space_controls(puzzle_control_context& context, int row, Color color,
                                    state_manager& state, input_handler& input) -> int override;

    auto on_board_press(MouseButton button, const puzzle_canvas& canvas, state_manager& state)
        -> void override;
    auto on_key(KeyboardKey key, bool ctrl, bool shift, state_manager& state) -> bool override;

    [[nodiscard]] auto status_text(const state_model& model, state_id id) const
        -> std::string override;
    [[nodiscard]] auto footer_text(const state_model& model, state_id id) const
        -> std::string override;

private:
    [[nodiscard]] auto current_puzzle(const state_manager& state) const -> const puzzle&;
    [[nodiscard]] auto puzzle_space_inventory_text(const state_manager& state) const -> std::string;
    [[nodiscard]] auto puzzle_space_wall_inventory_text(const state_manager& state) const
        -> std::string;
    auto draw_block(const puzzle_canvas& canvas, int x, int y, int width, int height, Color color,
                    bool enabled) const -> bool;
    auto clear_add_block() -> void;
    auto select_or_add_block(const puzzle_canvas& canvas, state_manager& state) -> void;
    auto remove_block_at_hover(const puzzle_canvas& canvas, state_manager& state) -> void;
    auto place_goal_at_hover(const puzzle_canvas& canvas, state_manager& state) -> void;
    auto move_selected(state_manager& state, dir direction) -> void;
    auto toggle_target(state_manager& state) -> void;
    auto toggle_wall(state_manager& state) -> void;
    auto toggle_restricted(state_manager& state) -> void;
    auto clear_goal(state_manager& state) -> void;
    auto add_column(state_manager& state) -> void;
    auto remove_column(state_manager& state) -> void;
    auto add_row(state_manager& state) -> void;
    auto remove_row(state_manager& state) -> void;
};

#endif
