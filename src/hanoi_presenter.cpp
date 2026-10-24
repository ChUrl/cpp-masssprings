#include "hanoi_presenter.hpp"

#include "config.hpp"
#include "input_handler.hpp"
#include "state_manager.hpp"

#include <algorithm>
#include <format>
#include <optional>

namespace
{
constexpr Color PEG_COLOR{126, 82, 48, 255};
constexpr Color BASE_COLOR{96, 60, 34, 255};

auto local_mouse() -> Vector2
{
    return GetMousePosition() - Vector2(0, COMPACT_MENU_HEIGHT);
}

auto board_area(const puzzle_canvas& canvas) -> Rectangle
{
    const float margin = std::min(canvas.bounds.width, canvas.bounds.height) * 0.06f;
    return Rectangle{canvas.bounds.x + margin, canvas.bounds.y + margin,
                     std::max(1.0f, canvas.bounds.width - 2.0f * margin),
                     std::max(1.0f, canvas.bounds.height - 2.0f * margin)};
}

auto peg_center_x(const Rectangle& area, const int peg) -> float
{
    return area.x +
        area.width * (static_cast<float>(peg) + 0.5f) / static_cast<float>(hanoi_model::PEG_COUNT);
}

auto peg_index_at(const Rectangle& area, const Vector2 mouse) -> int
{
    if (!CheckCollisionPointRec(mouse, area)) {
        return -1;
    }

    const float column_width = area.width / static_cast<float>(hanoi_model::PEG_COUNT);
    const int peg = static_cast<int>((mouse.x - area.x) / column_width);
    return std::clamp(peg, 0, hanoi_model::PEG_COUNT - 1);
}

auto disk_color(const int disk, const int disk_count) -> Color
{
    const float t =
        disk_count <= 1 ? 0.0f : static_cast<float>(disk) / static_cast<float>(disk_count - 1);
    return ColorFromHSV(210.0f - 190.0f * t, 0.62f, 0.92f);
}
} // namespace

hanoi_presenter::hanoi_presenter(hanoi_model& _model) : model(_model)
{}

auto hanoi_presenter::supports_editing() const -> bool
{
    return false;
}

auto hanoi_presenter::editing() const -> bool
{
    return false;
}

auto hanoi_presenter::toggle_editing() -> void
{}

auto hanoi_presenter::reset() -> void
{
    hov_peg = -1;
    selected_peg = -1;
}

auto hanoi_presenter::board_columns(const state_model&, const state_id) const -> int
{
    return hanoi_model::PEG_COUNT;
}

auto hanoi_presenter::board_rows(const state_model&, const state_id) const -> int
{
    return 1;
}

auto hanoi_presenter::draw_board(const puzzle_canvas& canvas, state_manager& state) const -> void
{
    const state_id id = state.current_state_id();
    const int disks = model.disk_count();
    const Rectangle area = board_area(canvas);

    const bool solved = model.is_goal(id);
    DrawRectangleRec(area,
                     solved ? Fade(BOARD_COLOR_WON, 0.18f) : Fade(BOARD_COLOR_RESTRICTED, 0.18f));
    DrawRectangleLinesEx(area, 2.0f, solved ? BOARD_COLOR_WON : BOARD_COLOR_RESTRICTED);

    const float base_y = area.y + area.height * 0.82f;
    const float peg_top = area.y + area.height * 0.16f;
    const float base_height = std::max(10.0f, area.height * 0.025f);
    const float peg_width = std::max(6.0f, area.width * 0.008f);
    const float column_width = area.width / static_cast<float>(hanoi_model::PEG_COUNT);

    hov_peg = peg_index_at(area, local_mouse());

    for (int peg = 0; peg < hanoi_model::PEG_COUNT; ++peg) {
        const float center = peg_center_x(area, peg);

        if (peg == selected_peg) {
            DrawRectangleRec(Rectangle{center - column_width * 0.5f, peg_top - 12.0f, column_width,
                                       base_y - peg_top + 12.0f},
                             Fade(YELLOW, 0.14f));
        } else if (peg == hov_peg) {
            DrawRectangleRec(Rectangle{center - column_width * 0.5f, peg_top - 12.0f, column_width,
                                       base_y - peg_top + 12.0f},
                             Fade(RAYWHITE, 0.18f));
        }

        DrawRectangleRec(Rectangle{center - peg_width * 0.5f, peg_top, peg_width, base_y - peg_top},
                         PEG_COLOR);
    }

    DrawRectangleRec(
        Rectangle{area.x + area.width * 0.06f, base_y, area.width * 0.88f, base_height},
        BASE_COLOR);

    const float max_disk_width = column_width - 20.0f;
    const float min_disk_width = std::max(24.0f, max_disk_width * 0.34f);
    const float disk_height = std::max(
        6.0f, std::min(28.0f, (base_y - peg_top - 8.0f) / static_cast<float>(std::max(1, disks))));

    for (int peg = 0; peg < hanoi_model::PEG_COUNT; ++peg) {
        const float center = peg_center_x(area, peg);
        float y = base_y;

        // Draw largest to smallest so the stack grows upwards.
        for (int disk = disks - 1; disk >= 0; --disk) {
            if (model.peg(id, disk) != peg) {
                continue;
            }

            const float t =
                disks <= 1 ? 0.0f : static_cast<float>(disk) / static_cast<float>(disks - 1);
            const float width = min_disk_width + (max_disk_width - min_disk_width) * t;
            const Rectangle disk_bounds{center - width * 0.5f, y - disk_height + 2.0f, width,
                                        disk_height - 3.0f};
            DrawRectangleRec(disk_bounds, disk_color(disk, disks));
            DrawRectangleLinesEx(disk_bounds, 1.5f, Fade(BLACK, 0.55f));
            y -= disk_height;
        }
    }

    if (selected_peg >= 0) {
        const int disk = model.top_disk(id, selected_peg);
        if (disk >= 0) {
            DrawText(TextFormat("Selected disk %d", disk + 1), static_cast<int>(area.x + 12.0f),
                     static_cast<int>(area.y + 10.0f), 20, DARKGRAY);
        }
    }
}

auto hanoi_presenter::draw_puzzle_controls(puzzle_control_context& context, const int row,
                                           const Color color, state_manager& state,
                                           input_handler& input) -> int
{
    int current_row = row;

    context.label(0, current_row, 2, 1, "Click a peg, then another peg.", color);
    ++current_row;

    if (context.button(0, current_row, 1, 1, "Reset (F3)", color)) {
        selected_peg = -1;
        state.edit_starting_state(model.solved_state());
    }
    if (context.button(1, current_row, 1, 1, "Random (F2)", color)) {
        selected_peg = -1;
        state.edit_starting_state(model.random_state());
    }
    ++current_row;

    return draw_graph_navigation(context, current_row, color, state, input);
}

auto hanoi_presenter::draw_edit_controls(puzzle_control_context&, const int row, Color,
                                         state_manager&) -> int
{
    return row;
}

auto hanoi_presenter::on_board_press(const MouseButton button, const puzzle_canvas& canvas,
                                     state_manager& state) -> void
{
    if (button == MOUSE_BUTTON_RIGHT) {
        selected_peg = -1;
        return;
    }
    if (button != MOUSE_BUTTON_LEFT) {
        return;
    }

    const int peg = peg_index_at(board_area(canvas), local_mouse());
    if (peg < 0) {
        selected_peg = -1;
        return;
    }

    const state_id current = state.current_state_id();
    if (selected_peg < 0) {
        if (model.top_disk(current, peg) >= 0) {
            selected_peg = peg;
        }
        return;
    }

    if (selected_peg == peg) {
        selected_peg = -1;
        return;
    }

    if (const std::optional<state_id> next = model.try_move(current, selected_peg, peg)) {
        selected_peg = -1;
        state.update_current_state(*next);
        return;
    }

    // Illegal move: keep the interaction forgiving by selecting the clicked peg if it has a disk.
    selected_peg = model.top_disk(current, peg) >= 0 ? peg : -1;
}

auto hanoi_presenter::on_key(const KeyboardKey key, const bool ctrl, const bool shift,
                             state_manager& state) -> bool
{
    if (ctrl || shift) {
        return false;
    }

    if (key == KEY_F2) {
        selected_peg = -1;
        state.edit_starting_state(model.random_state());
        return true;
    }
    if (key == KEY_F3) {
        selected_peg = -1;
        state.edit_starting_state(model.solved_state());
        return true;
    }

    return false;
}

auto hanoi_presenter::status_text(const state_model&, const state_id id) const -> std::string
{
    return std::format("{}/{} Disks", model.disks_on_target(id), model.disk_count());
}

auto hanoi_presenter::footer_text(const state_model&, const state_id id) const -> std::string
{
    return model.describe(id);
}
