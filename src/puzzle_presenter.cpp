#include "puzzle_presenter.hpp"

#include "input_handler.hpp"
#include "state_manager.hpp"

#include <algorithm>
#include <format>

namespace
{
struct canvas_grid
{
    int cell_size = 0;
    int step = 0;
    int grid_width = 0;
    int grid_height = 0;
    int x_offset = 0;
    int y_offset = 0;
};

auto canvas_grid_of(const puzzle_canvas& canvas) -> std::optional<canvas_grid>
{
    if (canvas.columns <= 0 || canvas.rows <= 0) {
        return std::nullopt;
    }

    const int available_width =
        static_cast<int>(canvas.bounds.width) - canvas.padding * (canvas.columns + 1);
    const int available_height =
        static_cast<int>(canvas.bounds.height) - canvas.padding * (canvas.rows + 1);
    const int cell_size =
        std::min(available_width / canvas.columns, available_height / canvas.rows);
    if (cell_size <= 0) {
        return std::nullopt;
    }

    canvas_grid grid;
    grid.cell_size = cell_size;
    grid.step = cell_size + canvas.padding;
    grid.grid_width = cell_size * canvas.columns + canvas.padding * (canvas.columns + 1);
    grid.grid_height = cell_size * canvas.rows + canvas.padding * (canvas.rows + 1);
    grid.x_offset = static_cast<int>(canvas.bounds.x) +
        (static_cast<int>(canvas.bounds.width) - grid.grid_width) / 2;
    grid.y_offset = static_cast<int>(canvas.bounds.y) +
        (static_cast<int>(canvas.bounds.height) - grid.grid_height) / 2;
    return grid;
}
} // namespace

auto puzzle_canvas::square_bounds(const int x, const int y, const int width, const int height) const
    -> Rectangle
{
    if (width <= 0 || height <= 0 || x < 0 || y < 0 || x + width > columns || y + height > rows) {
        return Rectangle{0, 0, 0, 0};
    }

    const std::optional<canvas_grid> grid = canvas_grid_of(*this);
    if (!grid) {
        return Rectangle{0, 0, 0, 0};
    }

    return Rectangle{
        static_cast<float>(grid->x_offset + x * grid->step + padding),
        static_cast<float>(grid->y_offset + y * grid->step + padding),
        static_cast<float>(width * grid->cell_size + padding * (width - 1)),
        static_cast<float>(height * grid->cell_size + padding * (height - 1)),
    };
}

auto puzzle_canvas::cell_at(const Vector2 mouse) const -> std::optional<std::pair<int, int>>
{
    const std::optional<canvas_grid> grid = canvas_grid_of(*this);
    if (!grid) {
        return std::nullopt;
    }

    // Include the internal padding between cells, because a multi-cell block is drawn as one
    // rectangle and its center gap belongs to that block. The outer board margin remains inert.
    const float left = static_cast<float>(grid->x_offset + padding);
    const float top = static_cast<float>(grid->y_offset + padding);
    const float right = left + static_cast<float>((columns - 1) * grid->step + grid->cell_size);
    const float bottom = top + static_cast<float>((rows - 1) * grid->step + grid->cell_size);
    if (mouse.x < left || mouse.x >= right || mouse.y < top || mouse.y >= bottom) {
        return std::nullopt;
    }

    const int x = std::clamp(static_cast<int>((mouse.x - left) / grid->step), 0, columns - 1);
    const int y = std::clamp(static_cast<int>((mouse.y - top) / grid->step), 0, rows - 1);
    return std::pair<int, int>{x, y};
}

namespace
{
auto nth(const int n) -> const char*
{
    if (n == 11 || n == 12 || n == 13) {
        return "th";
    }
    if (n % 10 == 1) {
        return "st";
    }
    if (n % 10 == 2) {
        return "nd";
    }
    if (n % 10 == 3) {
        return "rd";
    }
    return "th";
}
} // namespace

auto draw_graph_navigation(puzzle_control_context& context, const int row, const Color color,
                           state_manager& state, input_handler& input) -> int
{
    int current_row = row;

    const int visits = state.get_current_visits();
    context.label(0, current_row, 2, 1,
                  std::format("{} Moves ({}{} Time at this State)", state.get_total_moves(), visits,
                              nth(visits)),
                  color);
    ++current_row;

    const bool has_distances = state.has_distances();
    const size_t current = state.get_current_index();
    const std::vector<int>& distances = state.get_distances();
    const bool can_move = has_distances && current < distances.size() && distances[current] > 0;

    int column = 0;
    const auto place = [&](const std::string& text, const auto& action)
    {
        if (context.button(column, current_row, 1, 1, text, color)) {
            action();
        }
        if (++column == 2) {
            column = 0;
            ++current_row;
        }
    };

    if (can_move) {
        place("Optimal (Space)", [&] { input.goto_optimal_next_state(); });
    }
    if (state.has_history()) {
        place("Undo (Backspace)", [&] { input.goto_previous_state(); });
    }
    if (has_distances) {
        place("Nearest (B)", [&] { input.goto_closest_target_state(); });
        place("Worst (V)", [&] { input.goto_most_distant_state(); });
    }
    if (current != state.get_starting_index()) {
        place("Starting (R)", [&] { input.goto_starting_state(); });
    }

    if (column != 0) {
        ++current_row;
    }

    return current_row;
}
