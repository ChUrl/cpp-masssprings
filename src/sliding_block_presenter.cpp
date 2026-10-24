#include "sliding_block_presenter.hpp"

#include "config.hpp"
#include "input_handler.hpp"
#include "state_manager.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <ranges>

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

// Diagonal hatching used to mark the board while edit mode is active.
auto draw_hatch(const Rectangle& bounds) -> void
{
    constexpr float thickness = 1.0f;
    if (bounds.width <= 0 || bounds.height <= 0) {
        return;
    }

    const int left = static_cast<int>(std::floor(bounds.x));
    const int top = static_cast<int>(std::floor(bounds.y));
    const int right = static_cast<int>(std::ceil(bounds.x + bounds.width));
    const int bottom = static_cast<int>(std::ceil(bounds.y + bounds.height));
    BeginScissorMode(left, top, right - left, bottom - top);

    const float spacing = std::clamp(std::min(bounds.width, bounds.height) * 0.12f, 6.0f, 14.0f);
    const Color hatch_color = Fade(BLACK, 0.4f);
    for (float offset = -bounds.height; offset < bounds.width; offset += spacing) {
        DrawLineEx(Vector2{bounds.x + offset, bounds.y + bounds.height},
                   Vector2{bounds.x + offset + bounds.height, bounds.y}, thickness, hatch_color);
    }

    EndScissorMode();
}
} // namespace

sliding_block_presenter::sliding_block_presenter(sliding_block_model& _model) : model(_model)
{}

auto sliding_block_presenter::editing() const -> bool
{
    return edit_mode;
}

auto sliding_block_presenter::toggle_editing() -> void
{
    if (edit_mode) {
        clear_add_block();
    }
    edit_mode = !edit_mode;
}

auto sliding_block_presenter::reset() -> void
{
    hov_x = -1;
    hov_y = -1;
    sel_x = 0;
    sel_y = 0;
    edit_mode = false;
    clear_add_block();
}

auto sliding_block_presenter::board_columns(const state_model&, const state_id id) const -> int
{
    return model.get(id).get_width();
}

auto sliding_block_presenter::board_rows(const state_model&, const state_id id) const -> int
{
    return model.get(id).get_height();
}

auto sliding_block_presenter::draw_block(const puzzle_canvas& canvas, const int x, const int y,
                                         const int width, const int height, const Color color,
                                         const bool enabled) const -> bool
{
    const Rectangle bounds = canvas.square_bounds(x, y, width, height);
    if (bounds.width <= 0 || bounds.height <= 0) {
        return false;
    }

    const bool focused = CheckCollisionPointRec(local_mouse(), bounds);
    const bool pressed = block(x, y, width, height, false).covers(sel_x, sel_y);

    DrawRectangleRec(bounds, RAYWHITE);

    Color base = Fade(color, 0.5f);
    Color border = color;
    if (pressed) {
        base = Fade(color, 0.8f);
        border = color;
    }
    if (focused) {
        base = Fade(color, 0.3f);
        border = Fade(color, 0.7f);
    }
    if (focused && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        base = Fade(color, 0.8f);
    }
    if (!enabled) {
        base = BOARD_COLOR_RESTRICTED;
    }

    DrawRectangleRec(bounds, base);
    if (edit_mode) {
        draw_hatch(bounds);
    }
    if (enabled) {
        DrawRectangleLinesEx(bounds, 2.0f, border);
    }

    return focused && enabled;
}

auto sliding_block_presenter::draw_board(const puzzle_canvas& canvas, state_manager& state) const
    -> void
{
    const state_id id = state.current_state_id();
    const puzzle& current = model.get(id);

    const Rectangle bounds = outer_bounds(canvas);
    DrawRectangleRec(bounds, current.goal_reached() ? BOARD_COLOR_WON : BOARD_COLOR_RESTRICTED);

    DrawRectangle(static_cast<int>(bounds.x) + BOARD_PADDING,
                  static_cast<int>(bounds.y) + BOARD_PADDING,
                  static_cast<int>(bounds.width) - 2 * BOARD_PADDING,
                  static_cast<int>(bounds.height) - 2 * BOARD_PADDING,
                  current.get_restricted() ? BOARD_COLOR_RESTRICTED : BOARD_COLOR_FREE);

    const std::optional<block> target_block = current.try_get_target_block();
    const int target_x = current.get_goal_x();
    const int target_y = current.get_goal_y();
    if (current.get_goal() && target_block) {
        auto [x, y, width, height] = canvas.square_bounds(
            target_x, target_y, target_block->get_width(), target_block->get_height());

        const Color opening_color =
            Fade(current.goal_reached() ? BOARD_COLOR_WON : BOARD_COLOR_RESTRICTED, 0.3f);

        if (target_x == 0) {
            DrawRectangle(static_cast<int>(x) - BOARD_PADDING, static_cast<int>(y), BOARD_PADDING,
                          static_cast<int>(height), RAYWHITE);
            DrawRectangle(static_cast<int>(x) - BOARD_PADDING, static_cast<int>(y), BOARD_PADDING,
                          static_cast<int>(height), opening_color);
        }
        if (target_x + target_block->get_width() == current.get_width()) {
            DrawRectangle(static_cast<int>(x + width), static_cast<int>(y), BOARD_PADDING,
                          static_cast<int>(height), RAYWHITE);
            DrawRectangle(static_cast<int>(x + width), static_cast<int>(y), BOARD_PADDING,
                          static_cast<int>(height), opening_color);
        }
        if (target_y == 0) {
            DrawRectangle(static_cast<int>(x), static_cast<int>(y) - BOARD_PADDING,
                          static_cast<int>(width), BOARD_PADDING, RAYWHITE);
            DrawRectangle(static_cast<int>(x), static_cast<int>(y) - BOARD_PADDING,
                          static_cast<int>(width), BOARD_PADDING, opening_color);
        }
        if (target_y + target_block->get_height() == current.get_height()) {
            DrawRectangle(static_cast<int>(x), static_cast<int>(y + height),
                          static_cast<int>(width), BOARD_PADDING, RAYWHITE);
            DrawRectangle(static_cast<int>(x), static_cast<int>(y + height),
                          static_cast<int>(width), BOARD_PADDING, opening_color);
        }
    }

    // Empty cells and hovered cell. Use the same cell hit-testing as clicks so the highlighted
    // cell is always the one a click would select.
    for (int x = 0; x < canvas.columns; ++x) {
        for (int y = 0; y < canvas.rows; ++y) {
            DrawRectangleRec(canvas.square_bounds(x, y), RAYWHITE);
        }
    }
    const std::optional<std::pair<int, int>> hovered = canvas.cell_at(local_mouse());
    hov_x = hovered ? hovered->first : -1;
    hov_y = hovered ? hovered->second : -1;

    for (const block b : current.block_view()) {
        Color color = BLOCK_COLOR;
        if (b.get_target()) {
            color = TARGET_BLOCK_COLOR;
        } else if (b.get_immovable()) {
            color = WALL_COLOR;
        }

        const auto [x, y, width, height, target, immovable] = b.unpack_repr();
        draw_block(canvas, x, y, width, height, color, !immovable);
    }

    // Draw the greedy wall rectangles rather than every cell, so a converted 2x1 block looks
    // like one wall piece instead of two adjacent 1x1 cells.
    for (const block wall : current.wall_rectangles()) {
        draw_block(canvas, wall.get_x(), wall.get_y(), wall.get_width(), wall.get_height(),
                   WALL_COLOR, true);
    }

    if (edit_mode && has_block_add_xy) {
        if (current.covers(block_add_x, block_add_y) && hov_x >= block_add_x &&
            hov_y >= block_add_y) {
            bool collides = false;
            const block candidate(block_add_x, block_add_y, hov_x - block_add_x + 1,
                                  hov_y - block_add_y + 1, false);
            for (const block b : current.block_view()) {
                if (b.collides(candidate)) {
                    collides = true;
                    break;
                }
            }
            for (int y = block_add_y; y <= hov_y && !collides; ++y) {
                for (int x = block_add_x; x <= hov_x; ++x) {
                    if (current.wall_at(static_cast<u8>(x), static_cast<u8>(y))) {
                        collides = true;
                        break;
                    }
                }
            }
            if (!collides) {
                draw_block(canvas, block_add_x, block_add_y, hov_x - block_add_x + 1,
                           hov_y - block_add_y + 1, PURPLE, true);
            }
        }
    }

    if (edit_mode && current.get_goal() && target_block) {
        DrawRectangleLinesEx(canvas.square_bounds(target_x, target_y, target_block->get_width(),
                                                  target_block->get_height()),
                             2.0f, TARGET_BLOCK_COLOR);
    }
}

auto sliding_block_presenter::draw_puzzle_controls(puzzle_control_context& context, const int row,
                                                   const Color color, state_manager& state,
                                                   input_handler& input) -> int
{
    int next_row = row;
    const bool editing_now = edit_mode;
    const bool next_editing = context.toggle(0, next_row, 2, 1, "Puzzle Mode (Tab)",
                                             "Edit Mode (Tab)", editing_now, color);
    if (next_editing != editing_now) {
        toggle_editing();
    }
    ++next_row;

    if (!next_editing) {
        const bool has_navigation = state.has_distances() || state.has_history() ||
            state.get_current_index() != state.get_starting_index();
        next_row = draw_graph_navigation(context, next_row, color, state, input);
        if (!has_navigation) {
            context.label(0, next_row, 2, 1, "Populate graph (G) for navigation.", GRAY);
            ++next_row;
        }
    }

    return next_row;
}

auto sliding_block_presenter::draw_edit_controls(puzzle_control_context& context, const int row,
                                                 const Color color, state_manager& state) -> int
{
    if (context.button(0, row, 1, 1, "Target (T)", color)) {
        toggle_target(state);
    }
    if (context.button(1, row, 1, 1, "Wall (Y)", color)) {
        toggle_wall(state);
    }

    const puzzle& restricted_state = model.get(state.current_state_id());
    const bool free = !restricted_state.get_restricted();
    const bool next_free =
        context.toggle(0, row + 1, 2, 1, "Restricted (F)", "Free (F)", free, color);
    if (next_free != free) {
        toggle_restricted(state);
    }

    if (context.button(0, row + 2, 2, 1, "Clear Goal (X)", color)) {
        clear_goal(state);
    }

    const puzzle& column_state = model.get(state.current_state_id());
    int columns = column_state.get_width();
    const int next_columns = context.spinner(0, row + 3, 1, 1, "Cols: ", columns, puzzle::MIN_WIDTH,
                                             puzzle::MAX_WIDTH, color);
    if (next_columns > columns) {
        add_column(state);
    } else if (next_columns < columns) {
        remove_column(state);
    }

    const puzzle& row_state = model.get(state.current_state_id());
    int rows = row_state.get_height();
    const int next_rows = context.spinner(1, row + 3, 1, 1, "Rows: ", rows, puzzle::MIN_HEIGHT,
                                          puzzle::MAX_HEIGHT, color);
    if (next_rows > rows) {
        add_row(state);
    } else if (next_rows < rows) {
        remove_row(state);
    }

    return row + 4;
}

auto sliding_block_presenter::supports_puzzle_space() const -> bool
{
    return true;
}

auto sliding_block_presenter::current_puzzle(const state_manager& state) const -> const puzzle&
{
    return model.get(state.current_state_id());
}

auto sliding_block_presenter::puzzle_space_inventory_text(const state_manager& state) const
    -> std::string
{
    const puzzle& board = current_puzzle(state);
    blockset2 inventory;
    for (const block b : board.block_view()) {
        if (b.get_target() || b.get_immovable()) {
            continue;
        }
        inventory.insert(block(0, 0, b.get_width(), b.get_height(), false, false));
    }

    std::vector<std::string> shapes;
    shapes.reserve(inventory.size());
    for (const block b : inventory) {
        shapes.push_back(std::format("{}x{}", b.get_width(), b.get_height()));
    }
    std::ranges::sort(shapes);

    std::string text;
    for (const std::string& shape : shapes) {
        if (!text.empty()) {
            text += ", ";
        }
        text += shape;
    }
    return text;
}

auto sliding_block_presenter::puzzle_space_wall_inventory_text(const state_manager& state) const
    -> std::string
{
    const puzzle& board = current_puzzle(state);
    blockset2 inventory;
    for (const block shape : board.wall_rectangles()) {
        inventory.insert(block(0, 0, shape.get_width(), shape.get_height(), false, false));
    }

    std::vector<std::string> shapes;
    shapes.reserve(inventory.size());
    for (const block b : inventory) {
        shapes.push_back(std::format("{}x{}", b.get_width(), b.get_height()));
    }
    std::ranges::sort(shapes);

    std::string text;
    for (const std::string& shape : shapes) {
        if (!text.empty()) {
            text += ", ";
        }
        text += shape;
    }
    return text;
}

auto sliding_block_presenter::draw_puzzle_space_controls(puzzle_control_context& context,
                                                         const int row, const Color color,
                                                         state_manager& state, input_handler& input)
    -> int
{
    (void)input;

    int next_row = row;
    const bool running = state.search_is_running();
    const search_snapshot snapshot = state.get_search_snapshot();
    const bool cancelling = snapshot.status == search_status::cancelling;
    const puzzle& board = current_puzzle(state);

    const int movable_blocks = static_cast<int>(board.block_count());
    const std::vector<block> wall_rectangles = board.wall_rectangles();
    const int wall_pieces = static_cast<int>(wall_rectangles.size());
    const bool has_walls = wall_pieces != 0;
    if (!has_walls) {
        puzzle_space_wall_inventory = false;
    }
    const int wall_limit = board.get_width() * board.get_height();
    if (puzzle_space_wall_inventory) {
        if (puzzle_space_max_walls < 0 || puzzle_space_max_walls > wall_limit) {
            puzzle_space_max_walls = wall_limit;
        }
        puzzle_space_min_walls = std::clamp(puzzle_space_min_walls, 0, puzzle_space_max_walls);
    }
    if (puzzle_space_max_blocks <= 0) {
        puzzle_space_max_blocks = std::max(1, movable_blocks);
    }
    puzzle_space_max_blocks =
        std::clamp(puzzle_space_max_blocks, 1, static_cast<int>(puzzle::MAX_BLOCKS));
    if (puzzle_space_min_blocks <= 0) {
        puzzle_space_min_blocks = 2;
    }
    puzzle_space_min_blocks = std::clamp(puzzle_space_min_blocks, 1, puzzle_space_max_blocks);
    puzzle_space_min_moves = std::clamp(puzzle_space_min_moves, 0, 999);

    const std::optional<block> target = board.try_get_target_block();
    context.label(0, next_row, 2, 1,
                  std::format("Board: {}x{}  Goal: ({},{})  {}", board.get_width(),
                              board.get_height(), board.get_goal_x(), board.get_goal_y(),
                              board.get_restricted() ? "Restricted" : "Free"),
                  color, !running);
    ++next_row;

    context.label(0, next_row, 2, 1,
                  std::format("Inventory: {}", puzzle_space_inventory_text(state)), color,
                  !running);
    ++next_row;

    context.label(0, next_row, 2, 1,
                  target ? std::format("Target: {}x{}", target->get_width(), target->get_height())
                         : std::string("Target: missing"),
                  color, !running);
    ++next_row;

    if (has_walls) {
        const bool next_wall_inventory =
            context.checkbox(0, next_row, 2, 1, "Use wall shapes as inventory",
                             puzzle_space_wall_inventory, color, !running);
        if (next_wall_inventory != puzzle_space_wall_inventory) {
            puzzle_space_wall_inventory = next_wall_inventory;
            if (puzzle_space_wall_inventory) {
                puzzle_space_max_walls = wall_pieces;
                puzzle_space_min_walls = 0;
            }
        }
        ++next_row;

        if (puzzle_space_wall_inventory) {
            context.label(
                0, next_row, 2, 1,
                std::format("Wall inventory: {}", puzzle_space_wall_inventory_text(state)), color,
                !running);
            ++next_row;
            const int max_walls =
                context.spinner(1, next_row, 1, 1, "Max Walls: ", puzzle_space_max_walls, 0,
                                wall_limit, color, !running);
            puzzle_space_max_walls = std::clamp(max_walls, 0, wall_limit);
            puzzle_space_min_walls = std::clamp(puzzle_space_min_walls, 0, puzzle_space_max_walls);
            const int min_walls =
                context.spinner(0, next_row, 1, 1, "Min Walls: ", puzzle_space_min_walls, 0,
                                puzzle_space_max_walls, color, !running);
            puzzle_space_min_walls = min_walls;
            ++next_row;
            if (puzzle_space_max_walls > 0) {
                context.label(0, next_row, 2, 1,
                              "Large wall inventories can produce very many layouts.", GRAY);
                ++next_row;
            }
        } else {
            context.label(0, next_row, 2, 1, std::format("Walls: {} (fixed)", board.wall_count()),
                          color, !running);
            ++next_row;
        }
    }

    const wall_search_options wall_options{
        puzzle_space_wall_inventory,
        static_cast<size_t>(puzzle_space_min_walls),
        static_cast<size_t>(puzzle_space_max_walls),
    };

    const int max_blocks =
        context.spinner(1, next_row, 1, 1, "Max Blocks: ", puzzle_space_max_blocks, 1,
                        static_cast<int>(puzzle::MAX_BLOCKS), color, !running);
    puzzle_space_max_blocks = max_blocks;
    puzzle_space_min_blocks = std::clamp(puzzle_space_min_blocks, 1, puzzle_space_max_blocks);
    const int min_blocks =
        context.spinner(0, next_row, 1, 1, "Min Blocks: ", puzzle_space_min_blocks, 1,
                        puzzle_space_max_blocks, color, !running);
    puzzle_space_min_blocks = min_blocks;
    ++next_row;

    const int min_moves = context.spinner(0, next_row, 1, 1, "Min Moves: ", puzzle_space_min_moves,
                                          0, 999, color, !running);
    puzzle_space_min_moves = min_moves;
    ++next_row;

    const puzzle_space_config_result config = state.build_puzzle_space_config(
        static_cast<size_t>(puzzle_space_max_blocks), puzzle_space_min_moves,
        static_cast<size_t>(puzzle_space_min_blocks), wall_options);
    if (!running) {
        if (context.button(0, next_row, 2, 1, "Search Puzzle Space", color,
                           config.config.has_value())) {
            state.start_puzzle_space_search(
                static_cast<size_t>(puzzle_space_max_blocks), puzzle_space_min_moves,
                static_cast<size_t>(puzzle_space_min_blocks), wall_options);
        }
        ++next_row;

        if (!config.config) {
            context.label(0, next_row, 2, 1, config.error, GRAY);
            ++next_row;
        }
    } else if (cancelling) {
        context.label(0, next_row, 2, 1, "Cancelling search...", GRAY);
        ++next_row;
    } else {
        if (context.button(0, next_row, 2, 1, "Cancel Search", RED)) {
            state.cancel_search();
        }
        ++next_row;
    }

    if (context.button(0, next_row, 1, 1, "Save Space", color,
                       state.get_puzzle_space_results() != nullptr)) {
        input.ui_commands.emplace(show_puzzle_space_file_window{true});
    }
    if (context.button(1, next_row, 1, 1, "Load Space", color, !running)) {
        input.ui_commands.emplace(show_puzzle_space_file_window{false});
    }
    ++next_row;

    const bool shows_progress = snapshot.running() && snapshot.kind == search_kind::puzzle_space;
    if (shows_progress) {
        std::string progress_text;
        switch (snapshot.phase) {
        case puzzle_space_phase::inventories:
        case puzzle_space_phase::starting_positions:
            progress_text = "Preparing search";
            break;
        case puzzle_space_phase::counting:
            progress_text = std::format("Counting placements: {} / {} partitions",
                                        format_count(snapshot.partitions_counted),
                                        format_count(snapshot.partitions_total));
            break;
        case puzzle_space_phase::probing:
            progress_text = "Preparing partitions";
            break;
        case puzzle_space_phase::finishing:
            progress_text = "Sorting results";
            break;
        default:
            progress_text = "Exploring placements";
            break;
        }

        context.progress(0, next_row, 2, 1, progress_text, snapshot.progress_fraction, color,
                         false);
        ++next_row;

        if (snapshot.wall_layouts > 1) {
            context.label(0, next_row, 2, 1,
                          std::format("Wall layouts: {}", format_count(snapshot.wall_layouts)),
                          color);
            ++next_row;
        }

        // The index construction and the aggregation passes do not advance the
        // placement counters, so surface the aggregated progress of the workers
        // that are currently in such a step.
        if (snapshot.phase == puzzle_space_phase::exploring &&
            snapshot.stage != puzzle_space_stage::none) {
            const char* stage_name =
                snapshot.stage == puzzle_space_stage::indexing ? "Building index" : "Aggregating";
            // Floor the percentage (and keep 100 % for "done") so a build that is
            // still working never reads as finished.
            const int stage_percent = snapshot.stage_total > 0
                ? static_cast<int>(100.0 * static_cast<double>(snapshot.stage_done) /
                                   static_cast<double>(snapshot.stage_total))
                : 0;
            context.label(0, next_row, 2, 1,
                          std::format("{}: {}% | {} indexing, {} partitions | {} states",
                                      stage_name, std::min(stage_percent, 99),
                                      snapshot.stage_workers, snapshot.active_partitions,
                                      format_count(snapshot.stage_partition_states)),
                          color);
            ++next_row;
        }

        const bool exploration_data = snapshot.phase == puzzle_space_phase::exploring ||
            snapshot.phase == puzzle_space_phase::finishing;
        const std::string clusters_text =
            exploration_data ? format_count(snapshot.clusters_found) : "--";
        const bool candidates_ready = snapshot.phase != puzzle_space_phase::inventories &&
            snapshot.phase != puzzle_space_phase::starting_positions;
        std::string candidates_text;
        if (!candidates_ready) {
            candidates_text = "--";
        } else if (exploration_data) {
            candidates_text = std::format("{} / {}", format_count(snapshot.candidates_done),
                                          format_count(snapshot.candidates_total));
        } else if (snapshot.phase == puzzle_space_phase::counting) {
            candidates_text = std::format("{} counted", format_count(snapshot.placements_counted));
        } else {
            candidates_text = format_count(snapshot.candidates_total);
        }
        context.label(0, next_row, 2, 1,
                      std::format("Progress: {} | Clusters: {}", candidates_text, clusters_text),
                      color);
        ++next_row;

        context.label(0, next_row, 2, 1,
                      std::format("Elapsed: {} | ETA: {}",
                                  format_duration(snapshot.elapsed_seconds),
                                  format_duration(snapshot.eta_seconds)),
                      color);
        ++next_row;
    }

    if (!running && snapshot.kind == search_kind::puzzle_space &&
        (snapshot.status == search_status::cancelled || snapshot.status == search_status::failed)) {
        const std::string message = snapshot.error.empty() ? "Search cancelled." : snapshot.error;
        context.label(0, next_row, 2, 1, message, snapshot.error.empty() ? GRAY : RED);
        ++next_row;
    }

    const puzzle_space_result* results = state.get_puzzle_space_results();
    if (!running && results != nullptr) {
        context.label(0, next_row, 2, 1,
                      std::format("Results: {} clusters", format_count(results->clusters.size())),
                      color);
        ++next_row;

        if (results->clusters.empty()) {
            context.label(0, next_row, 2, 1, "No clusters matched.", GRAY);
            ++next_row;
            return next_row;
        }

        const bool blocks_first = state.get_puzzle_space_sort() ==
            puzzle_space_sort::most_blocks_fewest_states_most_moves;
        const bool next_blocks_first =
            context.toggle(0, next_row, 2, 1, "Moves First", "Blocks First", blocks_first, color);
        if (next_blocks_first != blocks_first) {
            state.set_puzzle_space_sort(
                next_blocks_first ? puzzle_space_sort::most_blocks_fewest_states_most_moves
                                  : puzzle_space_sort::most_moves);
        }
        ++next_row;
        if (next_blocks_first) {
            context.label(0, next_row, 2, 1, "Order: blocks, states, moves.", GRAY);
            ++next_row;
        }

        const int selection = static_cast<int>(state.get_puzzle_space_selection());
        const int result_count = static_cast<int>(results->clusters.size());
        int current_selection = selection;
        const auto apply_selection = [&](const size_t index)
        {
            state.set_puzzle_space_selection(index);
            state.request_load_puzzle_space_result(index);
        };

        const int slider_value =
            context.slider(0, next_row, 2, 1, "Result: ", selection + 1, 1, result_count, color);
        if (slider_value - 1 != current_selection) {
            current_selection = slider_value - 1;
            apply_selection(static_cast<size_t>(current_selection));
        }
        ++next_row;

        if (context.button(0, next_row, 1, 1, "Prev", color, current_selection > 0)) {
            --current_selection;
            apply_selection(static_cast<size_t>(current_selection));
        }
        if (context.button(1, next_row, 1, 1, "Next", color,
                           current_selection + 1 < result_count)) {
            ++current_selection;
            apply_selection(static_cast<size_t>(current_selection));
        }
        ++next_row;

        const puzzle_space_cluster& cluster =
            results->clusters[static_cast<size_t>(current_selection)];
        const std::string result_text = results->config.wall_inventory_enabled
            ? std::format("{} blocks, {} walls, {} moves, {} states",
                          format_count(cluster.block_count), format_count(cluster.wall_count),
                          format_count(cluster.max_moves), format_count(cluster.state_count))
            : std::format("{} blocks, {} moves, {} states", format_count(cluster.block_count),
                          format_count(cluster.max_moves), format_count(cluster.state_count));
        context.label(0, next_row, 2, 1, result_text, color);
        ++next_row;

        context.label(0, next_row, 2, 1, "The result loads automatically.", GRAY);
        ++next_row;
    }

    return next_row;
}

auto sliding_block_presenter::on_board_press(const MouseButton button, const puzzle_canvas& canvas,
                                             state_manager& state) -> void
{
    const state_id id = state.current_state_id();
    const puzzle& current = model.get(id);

    const std::optional<std::pair<int, int>> cell = canvas.cell_at(local_mouse());
    if (!cell) {
        return;
    }
    hov_x = cell->first;
    hov_y = cell->second;

    if (button == MOUSE_BUTTON_LEFT) {
        if (edit_mode) {
            select_or_add_block(canvas, state);
        } else if (current.try_get_block(hov_x, hov_y)) {
            sel_x = hov_x;
            sel_y = hov_y;
        }
    } else if (button == MOUSE_BUTTON_RIGHT && edit_mode && !has_block_add_xy) {
        remove_block_at_hover(canvas, state);
    } else if (button == MOUSE_BUTTON_MIDDLE && edit_mode) {
        place_goal_at_hover(canvas, state);
    }
}

auto sliding_block_presenter::on_key(const KeyboardKey key, const bool ctrl, const bool shift,
                                     state_manager& state) -> bool
{
    (void)shift;

    if (ctrl) {
        return false;
    }

    switch (key) {
    case KEY_W:
        move_selected(state, nor);
        return true;
    case KEY_D:
        move_selected(state, eas);
        return true;
    case KEY_S:
        move_selected(state, sou);
        return true;
    case KEY_A:
        move_selected(state, wes);
        return true;
    default:
        break;
    }

    if (!edit_mode) {
        return false;
    }

    switch (key) {
    case KEY_T:
        toggle_target(state);
        return true;
    case KEY_Y:
        toggle_wall(state);
        return true;
    case KEY_F:
        toggle_restricted(state);
        return true;
    case KEY_X:
        clear_goal(state);
        return true;
    case KEY_UP:
        add_row(state);
        return true;
    case KEY_DOWN:
        remove_row(state);
        return true;
    case KEY_RIGHT:
        add_column(state);
        return true;
    case KEY_LEFT:
        remove_column(state);
        return true;
    default:
        return false;
    }
}

auto sliding_block_presenter::status_text(const state_model&, const state_id id) const
    -> std::string
{
    const puzzle& p = model.get(id);
    return std::format("{}/{} Blocks, {} Walls", p.block_count(), puzzle::MAX_BLOCKS,
                       p.wall_count());
}

auto sliding_block_presenter::footer_text(const state_model&, const state_id id) const
    -> std::string
{
    return model.get(id).string_repr();
}

auto sliding_block_presenter::clear_add_block() -> void
{
    has_block_add_xy = false;
    block_add_x = -1;
    block_add_y = -1;
}

auto sliding_block_presenter::select_or_add_block(const puzzle_canvas& canvas, state_manager& state)
    -> void
{
    (void)canvas;

    const state_id id = state.current_state_id();
    const puzzle& current = model.get(id);

    if (current.wall_at(static_cast<u8>(hov_x), static_cast<u8>(hov_y))) {
        sel_x = hov_x;
        sel_y = hov_y;
        clear_add_block();
        return;
    }

    if (current.try_get_block(hov_x, hov_y)) {
        sel_x = hov_x;
        sel_y = hov_y;
        clear_add_block();
        return;
    }

    if (has_block_add_xy) {
        const int width = hov_x - block_add_x + 1;
        const int height = hov_y - block_add_y + 1;
        if (width <= 0 || height <= 0) {
            clear_add_block();
            return;
        }

        if (current.covers(block_add_x, block_add_y, width, height)) {
            if (const auto next = model.try_add_block(
                    id, block(block_add_x, block_add_y, width, height, false))) {
                sel_x = block_add_x;
                sel_y = block_add_y;
                clear_add_block();
                state.edit_starting_state(*next);
            }
        }
        return;
    }

    if (current.block_count() < puzzle::MAX_BLOCKS && current.covers(hov_x, hov_y)) {
        block_add_x = hov_x;
        block_add_y = hov_y;
        has_block_add_xy = true;
        // The empty anchor is now the selection. This lets the wall toggle place a 1x1 wall at
        // that cell instead of still operating on the previously selected block.
        sel_x = hov_x;
        sel_y = hov_y;
    }
}

auto sliding_block_presenter::remove_block_at_hover(const puzzle_canvas& canvas,
                                                    state_manager& state) -> void
{
    (void)canvas;

    const state_id id = state.current_state_id();
    const puzzle& current = model.get(id);
    if (current.wall_at(static_cast<u8>(hov_x), static_cast<u8>(hov_y))) {
        if (const auto next = model.try_remove_wall(id, hov_x, hov_y)) {
            if (sel_x == hov_x && sel_y == hov_y) {
                sel_x = 0;
                sel_y = 0;
            }
            state.edit_starting_state(*next);
        }
        return;
    }

    const std::optional<block> b = current.try_get_block(hov_x, hov_y);
    if (!b) {
        return;
    }

    if (const auto next = model.try_remove_block(id, hov_x, hov_y)) {
        if (b->covers(sel_x, sel_y)) {
            sel_x = 0;
            sel_y = 0;
        }
        state.edit_starting_state(*next);
    }
}

auto sliding_block_presenter::place_goal_at_hover(const puzzle_canvas& canvas, state_manager& state)
    -> void
{
    (void)canvas;

    const state_id id = state.current_state_id();
    if (const auto next = model.try_set_goal(id, hov_x, hov_y)) {
        state.edit_starting_state(*next);
    }
}

auto sliding_block_presenter::move_selected(state_manager& state, const dir direction) -> void
{
    const state_id id = state.current_state_id();
    if (model.get(id).wall_at(static_cast<u8>(sel_x), static_cast<u8>(sel_y))) {
        return;
    }
    if (const auto next = model.try_move(id, sel_x, sel_y, direction)) {
        switch (direction) {
        case nor:
            sel_y--;
            break;
        case eas:
            sel_x++;
            break;
        case sou:
            sel_y++;
            break;
        case wes:
            sel_x--;
            break;
        }
        state.update_current_state(*next);
    }
}

auto sliding_block_presenter::toggle_target(state_manager& state) -> void
{
    const state_id id = state.current_state_id();
    if (const auto next = model.try_toggle_target(id, sel_x, sel_y)) {
        state.edit_starting_state(*next);
    }
}

auto sliding_block_presenter::toggle_wall(state_manager& state) -> void
{
    const state_id id = state.current_state_id();
    const puzzle& current = model.get(id);

    // If the block-add preview is active, use the same rectangle as a wall piece. This is the
    // direct way to create a 2x1, 1x2, 2x2, ... wall without first creating and converting a block.
    if (has_block_add_xy && hov_x >= 0 && hov_y >= 0) {
        const int width = hov_x - block_add_x + 1;
        const int height = hov_y - block_add_y + 1;
        if (width > 0 && height > 0 && current.covers(block_add_x, block_add_y, width, height)) {
            if (const auto next = model.try_add_wall_block(
                    id, block(block_add_x, block_add_y, width, height, false))) {
                sel_x = block_add_x;
                sel_y = block_add_y;
                clear_add_block();
                state.edit_starting_state(*next);
            }
        }
        return;
    }

    // The wall button is usually clicked after selecting a block. In that case the pointer is no
    // longer over the board, so operate on the selected block/wall. If nothing is selected, fall
    // back to the hovered cell to allow placing a 1x1 wall directly.
    const bool selected_object = current.try_get_block(sel_x, sel_y).has_value() ||
        current.wall_at(static_cast<u8>(sel_x), static_cast<u8>(sel_y));
    int x = sel_x;
    int y = sel_y;
    if (!selected_object && hov_x >= 0 && hov_y >= 0) {
        x = hov_x;
        y = hov_y;
    }

    if (const auto next = model.try_toggle_wall(id, x, y)) {
        sel_x = x;
        sel_y = y;
        state.edit_starting_state(*next);
    }
}

auto sliding_block_presenter::toggle_restricted(state_manager& state) -> void
{
    const state_id id = state.current_state_id();
    if (const auto next = model.toggle_restricted(id)) {
        state.edit_starting_state(*next);
    }
}

auto sliding_block_presenter::clear_goal(state_manager& state) -> void
{
    const state_id id = state.current_state_id();
    if (const auto next = model.clear_goal(id)) {
        state.edit_starting_state(*next);
    }
}

auto sliding_block_presenter::add_column(state_manager& state) -> void
{
    const state_id id = state.current_state_id();
    if (const auto next = model.try_add_column(id)) {
        state.edit_starting_state(*next);
    }
}

auto sliding_block_presenter::remove_column(state_manager& state) -> void
{
    const state_id id = state.current_state_id();
    if (const auto next = model.try_remove_column(id)) {
        state.edit_starting_state(*next);
    }
}

auto sliding_block_presenter::add_row(state_manager& state) -> void
{
    const state_id id = state.current_state_id();
    if (const auto next = model.try_add_row(id)) {
        state.edit_starting_state(*next);
    }
}

auto sliding_block_presenter::remove_row(state_manager& state) -> void
{
    const state_id id = state.current_state_id();
    if (const auto next = model.try_remove_row(id)) {
        state.edit_starting_state(*next);
    }
}
