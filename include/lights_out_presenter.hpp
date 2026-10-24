#ifndef LIGHTS_OUT_PRESENTER_HPP_
#define LIGHTS_OUT_PRESENTER_HPP_

#include "lights_out_model.hpp"
#include "puzzle_presenter.hpp"

class state_manager;

class lights_out_presenter : public puzzle_presenter
{
private:
    lights_out_model& model;
    mutable int hov_x = -1;
    mutable int hov_y = -1;

public:
    explicit lights_out_presenter(lights_out_model& _model);

    [[nodiscard]] auto supports_editing() const -> bool override;
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

    auto on_board_press(MouseButton button, const puzzle_canvas& canvas, state_manager& state)
        -> void override;
    auto on_key(KeyboardKey key, bool ctrl, bool shift, state_manager& state) -> bool override;

    [[nodiscard]] auto status_text(const state_model& model, state_id id) const
        -> std::string override;
    [[nodiscard]] auto footer_text(const state_model& model, state_id id) const
        -> std::string override;
};

#endif
