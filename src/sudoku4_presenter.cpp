#include "sudoku4_presenter.hpp"

#include "config.hpp"
#include "input_handler.hpp"
#include "state_manager.hpp"

#include <algorithm>
#include <format>
#include <optional>

namespace
{
constexpr Color GIVEN_CELL_COLOR{214, 224, 238, 255};
constexpr Color USER_CELL_COLOR{78, 126, 186, 255};
constexpr Color EMPTY_CELL_COLOR{250, 250, 250, 255};

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

auto draw_digit(const Rectangle& bounds, const int value, const Color color) -> void
{
    const std::string text = std::to_string(value);
    const int font_size = std::max(16, static_cast<int>(bounds.height * 0.5f));
    const int text_width = MeasureText(text.c_str(), font_size);
    DrawText(text.c_str(), static_cast<int>(bounds.x + (bounds.width - text_width) * 0.5f),
             static_cast<int>(bounds.y + (bounds.height - font_size) * 0.5f), font_size, color);
}
} // namespace

sudoku4_presenter::sudoku4_presenter(sudoku4_model& _model) : model(_model)
{}

auto sudoku4_presenter::supports_editing() const -> bool
{
    return false;
}

auto sudoku4_presenter::editing() const -> bool
{
    return false;
}

auto sudoku4_presenter::toggle_editing() -> void
{}

auto sudoku4_presenter::reset() -> void
{
    hov_cell = -1;
    active_digit = 1;
}

auto sudoku4_presenter::board_columns(const state_model&, const state_id) const -> int
{
    return sudoku4_model::SIZE;
}

auto sudoku4_presenter::board_rows(const state_model&, const state_id) const -> int
{
    return sudoku4_model::SIZE;
}

auto sudoku4_presenter::draw_board(const puzzle_canvas& canvas, state_manager& state) const -> void
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

    for (int y = 0; y < sudoku4_model::SIZE; ++y) {
        for (int x = 0; x < sudoku4_model::SIZE; ++x) {
            const Rectangle cell_bounds = canvas.square_bounds(x, y);
            if (cell_bounds.width <= 0 || cell_bounds.height <= 0) {
                continue;
            }

            const int cell = y * sudoku4_model::SIZE + x;
            const int value = model.value(id, cell);
            const bool given = model.is_given(cell);
            const bool hovered_cell = hovered && hovered->first == x && hovered->second == y;

            if (hovered_cell && !given) {
                hov_cell = cell;
            }

            Color background = EMPTY_CELL_COLOR;
            Color text_color = DARKGRAY;
            if (given) {
                background = GIVEN_CELL_COLOR;
                text_color = Color{30, 40, 60, 255};
            } else if (value != 0) {
                background = USER_CELL_COLOR;
                text_color = RAYWHITE;
            }
            if (hov_cell == cell) {
                background = Fade(YELLOW, 0.45f);
            }

            DrawRectangleRec(cell_bounds, background);
            DrawRectangleLinesEx(cell_bounds, 1.0f, Fade(BLACK, 0.25f));

            if (value != 0) {
                draw_digit(cell_bounds, value, text_color);
            }
        }
    }

    // Thicker lines separate the four 2x2 boxes.
    const Rectangle grid = canvas.square_bounds(0, 0, sudoku4_model::SIZE, sudoku4_model::SIZE);
    const Rectangle left_box = canvas.square_bounds(1, 0);
    const Rectangle right_box = canvas.square_bounds(2, 0);
    const Rectangle top_box = canvas.square_bounds(0, 1);
    const Rectangle bottom_box = canvas.square_bounds(0, 2);
    const float middle_x = (left_box.x + left_box.width + right_box.x) * 0.5f;
    const float middle_y = (top_box.y + top_box.height + bottom_box.y) * 0.5f;
    DrawLineEx(Vector2{middle_x, grid.y}, Vector2{middle_x, grid.y + grid.height}, 3.0f, BLACK);
    DrawLineEx(Vector2{grid.x, middle_y}, Vector2{grid.x + grid.width, middle_y}, 3.0f, BLACK);
}

auto sudoku4_presenter::draw_puzzle_controls(puzzle_control_context& context, const int row,
                                             const Color color, state_manager& state,
                                             input_handler& input) -> int
{
    int current_row = row;

    context.label(0, current_row, 2, 1, "Digit, cell; right-click clears.", color);
    ++current_row;

    for (int digit = 1; digit <= sudoku4_model::DIGITS; ++digit) {
        const int column = (digit - 1) % 2;
        const int line = current_row + (digit - 1) / 2;
        const Color digit_color = digit == active_digit ? ORANGE : color;
        if (context.button(column, line, 1, 1, std::format("{}", digit), digit_color)) {
            active_digit = digit;
        }
    }
    current_row += 2;

    context.label(0, current_row, 2, 1, std::format("Active digit: {}", active_digit), color);
    ++current_row;

    if (context.button(0, current_row, 2, 1, "Reset Puzzle (F3)", color)) {
        active_digit = 1;
        state.edit_starting_state(model.initial_state());
    }
    ++current_row;

    return draw_graph_navigation(context, current_row, color, state, input);
}

auto sudoku4_presenter::draw_edit_controls(puzzle_control_context&, const int row, Color,
                                           state_manager&) -> int
{
    return row;
}

auto sudoku4_presenter::on_board_press(const MouseButton button, const puzzle_canvas& canvas,
                                       state_manager& state) -> void
{
    if (button != MOUSE_BUTTON_LEFT && button != MOUSE_BUTTON_RIGHT) {
        return;
    }

    const std::optional<std::pair<int, int>> cell = canvas.cell_at(local_mouse());
    if (!cell) {
        return;
    }

    const int index = cell->second * sudoku4_model::SIZE + cell->first;
    if (model.is_given(index)) {
        return;
    }

    const int digit = button == MOUSE_BUTTON_RIGHT ? 0 : active_digit;
    if (const std::optional<state_id> next =
            model.try_set(state.current_state_id(), index, digit)) {
        state.update_current_state(*next);
    }
}

auto sudoku4_presenter::on_key(const KeyboardKey key, const bool ctrl, const bool shift,
                               state_manager& state) -> bool
{
    if (ctrl || shift) {
        return false;
    }

    switch (key) {
    case KEY_ONE:
        active_digit = 1;
        return true;
    case KEY_TWO:
        active_digit = 2;
        return true;
    case KEY_THREE:
        active_digit = 3;
        return true;
    case KEY_FOUR:
        active_digit = 4;
        return true;
    case KEY_F3:
        active_digit = 1;
        state.edit_starting_state(model.initial_state());
        return true;
    default:
        return false;
    }
}

auto sudoku4_presenter::status_text(const state_model&, const state_id id) const -> std::string
{
    const int placed = sudoku4_model::CELLS - model.empty_count(id);
    return std::format("{}/{} Digits", placed, sudoku4_model::CELLS);
}

auto sudoku4_presenter::footer_text(const state_model&, const state_id id) const -> std::string
{
    return model.describe(id);
}
