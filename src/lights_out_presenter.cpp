#include "lights_out_presenter.hpp"

#include "config.hpp"
#include "input_handler.hpp"
#include "state_manager.hpp"

#include <format>
#include <optional>

namespace
{
auto local_mouse() -> Vector2
{
    return GetMousePosition() - Vector2(0, COMPACT_MENU_HEIGHT);
}

auto cell_color(const int value, const int base) -> Color
{
    if (base == 3) {
        switch (value) {
        case 0:
            return Color{45, 48, 56, 255};
        case 1:
            return Color{255, 196, 61, 255};
        default:
            return Color{240, 104, 58, 255};
        }
    }

    return value != 0 ? Color{255, 196, 61, 255} : Color{45, 48, 56, 255};
}

auto hover_color(const int value, const int base) -> Color
{
    if (base == 3) {
        switch (value) {
        case 0:
            return Color{78, 84, 96, 255};
        case 1:
            return Color{255, 222, 128, 255};
        default:
            return Color{255, 144, 96, 255};
        }
    }

    return value != 0 ? Color{255, 222, 128, 255} : Color{78, 84, 96, 255};
}

auto outer_bounds(const puzzle_canvas& canvas) -> Rectangle
{
    Rectangle bounds = canvas.square_bounds(0, 0, canvas.columns, canvas.rows);
    bounds.x -= static_cast<float>(canvas.padding);
    bounds.y -= static_cast<float>(canvas.padding);
    bounds.width += static_cast<float>(2 * canvas.padding);
    bounds.height += static_cast<float>(2 * canvas.padding);
    return bounds;
}
} // namespace

lights_out_presenter::lights_out_presenter(lights_out_model& _model) : model(_model)
{}

auto lights_out_presenter::supports_editing() const -> bool
{
    return false;
}

auto lights_out_presenter::editing() const -> bool
{
    return false;
}

auto lights_out_presenter::toggle_editing() -> void
{}

auto lights_out_presenter::reset() -> void
{
    hov_x = -1;
    hov_y = -1;
}

auto lights_out_presenter::board_columns(const state_model&, const state_id) const -> int
{
    return model.rules().width;
}

auto lights_out_presenter::board_rows(const state_model&, const state_id) const -> int
{
    return model.rules().height;
}

auto lights_out_presenter::draw_board(const puzzle_canvas& canvas, state_manager& state) const
    -> void
{
    const lights_out_rules& rules = model.rules();
    const state_id id = state.current_state_id();

    const bool solved = model.is_goal(id);
    const Rectangle board_bounds = outer_bounds(canvas);
    DrawRectangleRec(board_bounds, solved ? BOARD_COLOR_WON : BOARD_COLOR_RESTRICTED);
    DrawRectangle(static_cast<int>(board_bounds.x) + canvas.padding,
                  static_cast<int>(board_bounds.y) + canvas.padding,
                  static_cast<int>(board_bounds.width) - 2 * canvas.padding,
                  static_cast<int>(board_bounds.height) - 2 * canvas.padding,
                  BOARD_COLOR_RESTRICTED);

    hov_x = -1;
    hov_y = -1;
    const Vector2 mouse = local_mouse();

    for (int y = 0; y < rules.height; ++y) {
        for (int x = 0; x < rules.width; ++x) {
            const Rectangle bounds = canvas.square_bounds(x, y);
            if (bounds.width <= 0 || bounds.height <= 0) {
                continue;
            }

            const int cell = y * rules.width + x;
            const int value = model.value(id, cell);
            const bool pressable = rules.is_pressable(x, y);
            const bool hovered = pressable && CheckCollisionPointRec(mouse, bounds);
            if (hovered) {
                hov_x = x;
                hov_y = y;
            }

            Color color = pressable ? cell_color(value, rules.base) : BOARD_COLOR_RESTRICTED;
            if (hovered) {
                color = hover_color(value, rules.base);
            }

            DrawRectangleRec(bounds, color);
        }
    }
}

auto lights_out_presenter::draw_puzzle_controls(puzzle_control_context& context, const int row,
                                                const Color color, state_manager& state,
                                                input_handler& input) -> int
{
    const bool has_navigation = state.has_distances() || state.has_history() ||
        state.get_current_index() != state.get_starting_index();
    int next_row = draw_graph_navigation(context, row, color, state, input);
    if (!has_navigation) {
        context.label(0, next_row, 2, 1, "Populate graph (G) for navigation.", GRAY);
        ++next_row;
    }

    return next_row;
}

auto lights_out_presenter::draw_edit_controls(puzzle_control_context&, const int row, Color,
                                              state_manager&) -> int
{
    return row;
}

auto lights_out_presenter::on_board_press(const MouseButton button, const puzzle_canvas& canvas,
                                          state_manager& state) -> void
{
    if (button != MOUSE_BUTTON_LEFT && button != MOUSE_BUTTON_RIGHT) {
        return;
    }

    const std::optional<std::pair<int, int>> cell = canvas.cell_at(local_mouse());
    if (!cell) {
        return;
    }

    if (!model.rules().is_pressable(cell->first, cell->second)) {
        return;
    }

    state.update_current_state(
        model.try_toggle(state.current_state_id(), cell->first, cell->second));
}

auto lights_out_presenter::on_key(const KeyboardKey key, const bool ctrl, const bool shift,
                                  state_manager& state) -> bool
{
    if (ctrl || shift) {
        return false;
    }

    if (key == KEY_F2) {
        state.edit_starting_state(model.random_state());
        return true;
    }
    if (key == KEY_F3) {
        state.edit_starting_state(model.solved_state());
        return true;
    }

    return false;
}

auto lights_out_presenter::status_text(const state_model&, const state_id id) const -> std::string
{
    return std::format("{} Lit", model.on_count(id));
}

auto lights_out_presenter::footer_text(const state_model&, const state_id id) const -> std::string
{
    return model.describe(id);
}
