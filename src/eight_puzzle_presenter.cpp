#include "eight_puzzle_presenter.hpp"

#include "config.hpp"
#include "input_handler.hpp"
#include "state_manager.hpp"

#include <algorithm>
#include <cstdlib>
#include <optional>

namespace
{
auto local_mouse() -> Vector2
{
    return GetMousePosition() - Vector2(0, COMPACT_MENU_HEIGHT);
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

auto tile_color(const int value) -> Color
{
    const float t =
        static_cast<float>(value - 1) / static_cast<float>(eight_puzzle_model::TILE_COUNT - 1);
    return ColorFromHSV(205.0f - 150.0f * t, 0.58f, 0.88f);
}
} // namespace

eight_puzzle_presenter::eight_puzzle_presenter(eight_puzzle_model& _model) : model(_model)
{}

auto eight_puzzle_presenter::supports_editing() const -> bool
{
    return false;
}

auto eight_puzzle_presenter::editing() const -> bool
{
    return false;
}

auto eight_puzzle_presenter::toggle_editing() -> void
{}

auto eight_puzzle_presenter::reset() -> void
{
    hov_cell = -1;
}

auto eight_puzzle_presenter::board_columns(const state_model&, const state_id) const -> int
{
    return eight_puzzle_model::SIZE;
}

auto eight_puzzle_presenter::board_rows(const state_model&, const state_id) const -> int
{
    return eight_puzzle_model::SIZE;
}

auto eight_puzzle_presenter::draw_board(const puzzle_canvas& canvas, state_manager& state) const
    -> void
{
    const state_id id = state.current_state_id();
    const bool solved = model.is_goal(id);
    const Rectangle bounds = outer_bounds(canvas);

    DrawRectangleRec(bounds, solved ? BOARD_COLOR_WON : BOARD_COLOR_RESTRICTED);
    DrawRectangle(static_cast<int>(bounds.x) + canvas.padding,
                  static_cast<int>(bounds.y) + canvas.padding,
                  static_cast<int>(bounds.width) - 2 * canvas.padding,
                  static_cast<int>(bounds.height) - 2 * canvas.padding, BOARD_COLOR_RESTRICTED);

    hov_cell = -1;
    const Vector2 mouse = local_mouse();
    const std::optional<std::pair<int, int>> hovered = canvas.cell_at(mouse);
    const int blank = model.blank_cell(id);

    for (int y = 0; y < eight_puzzle_model::SIZE; ++y) {
        for (int x = 0; x < eight_puzzle_model::SIZE; ++x) {
            const Rectangle cell_bounds = canvas.square_bounds(x, y);
            if (cell_bounds.width <= 0 || cell_bounds.height <= 0) {
                continue;
            }

            const int cell = y * eight_puzzle_model::SIZE + x;
            const int value = model.value(id, cell);
            const bool movable = value != 0 &&
                (std::abs(cell % eight_puzzle_model::SIZE - blank % eight_puzzle_model::SIZE) +
                     std::abs(cell / eight_puzzle_model::SIZE - blank / eight_puzzle_model::SIZE) ==
                 1);

            if (hovered && hovered->first == x && hovered->second == y && movable) {
                hov_cell = cell;
            }

            Color color = value == 0 ? Color{45, 48, 56, 255} : tile_color(value);
            if (movable) {
                color = Fade(color, 0.88f);
            }
            if (hov_cell == cell) {
                color = Fade(YELLOW, 0.9f);
            }

            DrawRectangleRec(cell_bounds, color);
            DrawRectangleLinesEx(cell_bounds, 2.0f, Fade(BLACK, 0.45f));

            if (value != 0) {
                const std::string text = std::to_string(value);
                const int font_size = std::max(16, static_cast<int>(cell_bounds.height * 0.45f));
                const int text_width = MeasureText(text.c_str(), font_size);
                DrawText(text.c_str(),
                         static_cast<int>(cell_bounds.x + (cell_bounds.width - text_width) * 0.5f),
                         static_cast<int>(cell_bounds.y + (cell_bounds.height - font_size) * 0.5f),
                         font_size, RAYWHITE);
            }
        }
    }
}

auto eight_puzzle_presenter::draw_puzzle_controls(puzzle_control_context& context, const int row,
                                                  const Color color, state_manager& state,
                                                  input_handler& input) -> int
{
    int current_row = row;

    context.label(0, current_row, 2, 1, "WASD/arrows or click a tile.", color);
    ++current_row;

    if (context.button(0, current_row, 1, 1, "Reset (F3)", color)) {
        state.edit_starting_state(model.solved_state());
    }
    if (context.button(1, current_row, 1, 1, "Random (F2)", color)) {
        state.edit_starting_state(model.random_state());
    }
    ++current_row;

    return draw_graph_navigation(context, current_row, color, state, input);
}

auto eight_puzzle_presenter::draw_edit_controls(puzzle_control_context&, const int row, Color,
                                                state_manager&) -> int
{
    return row;
}

auto eight_puzzle_presenter::on_board_press(const MouseButton button, const puzzle_canvas& canvas,
                                            state_manager& state) -> void
{
    if (button != MOUSE_BUTTON_LEFT) {
        return;
    }

    const std::optional<std::pair<int, int>> cell = canvas.cell_at(local_mouse());
    if (!cell) {
        return;
    }

    const int index = cell->second * eight_puzzle_model::SIZE + cell->first;
    if (const std::optional<state_id> next = model.try_move_tile(state.current_state_id(), index)) {
        state.update_current_state(*next);
    }
}

auto eight_puzzle_presenter::on_key(const KeyboardKey key, const bool ctrl, const bool shift,
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

    int dx = 0;
    int dy = 0;
    switch (key) {
    case KEY_W:
    case KEY_UP:
        dy = -1;
        break;
    case KEY_S:
    case KEY_DOWN:
        dy = 1;
        break;
    case KEY_A:
    case KEY_LEFT:
        dx = -1;
        break;
    case KEY_D:
    case KEY_RIGHT:
        dx = 1;
        break;
    default:
        return false;
    }

    if (const std::optional<state_id> next =
            model.try_move_blank(state.current_state_id(), dx, dy)) {
        state.update_current_state(*next);
    }
    return true;
}

auto eight_puzzle_presenter::status_text(const state_model&, const state_id) const -> std::string
{
    return {};
}

auto eight_puzzle_presenter::footer_text(const state_model&, const state_id id) const -> std::string
{
    return model.describe(id);
}
