#ifndef PUZZLE_PRESENTER_HPP_
#define PUZZLE_PRESENTER_HPP_

#include "state_model.hpp"

#include <raylib.h>

#include <optional>
#include <string>
#include <utility>

class input_handler;
class state_manager;

/**
 * A rectangular drawing area for a puzzle. Coordinates are local to the board render target, so
 * mouse positions passed to cell_at() must already be translated into the same space.
 */
struct puzzle_canvas
{
    Rectangle bounds{};
    int columns = 0;
    int rows = 0;
    int padding = 0;

    [[nodiscard]] auto square_bounds(int x, int y, int width = 1, int height = 1) const
        -> Rectangle;
    [[nodiscard]] auto cell_at(Vector2 mouse) const -> std::optional<std::pair<int, int>>;
};

/**
 * The UI drawing primitives a presenter may use for its controls. This keeps the presenter
 * independent of the concrete user_interface implementation while preserving the existing menu
 * layout and styling.
 */
class puzzle_control_context
{
public:
    virtual ~puzzle_control_context() = default;

    virtual auto button(int x, int y, int width, int height, const std::string& label, Color color,
                        bool enabled = true) -> bool = 0;
    virtual auto toggle(int x, int y, int width, int height, const std::string& off_label,
                        const std::string& on_label, bool value, Color color, bool enabled = true)
        -> bool = 0;
    virtual auto checkbox(int x, int y, int width, int height, const std::string& label, bool value,
                          Color color, bool enabled = true) -> bool = 0;
    virtual auto spinner(int x, int y, int width, int height, const std::string& label, int value,
                         int min, int max, Color color, bool enabled = true) -> int = 0;
    virtual auto slider(int x, int y, int width, int height, const std::string& label, int value,
                        int min, int max, Color color, bool enabled = true) -> int = 0;
    virtual auto progress(int x, int y, int width, int height, const std::string& label,
                          float fraction, Color color, bool indeterminate = false) -> void = 0;
    virtual auto label(int x, int y, int width, int height, const std::string& text, Color color,
                       bool enabled = true) -> void = 0;
};

/**
 * Puzzle-specific presentation, input and editing.
 *
 * Implementations receive the state_manager in each call instead of storing a reference to it, so
 * they can be replaced at runtime without dangling references.
 */
class puzzle_presenter
{
public:
    virtual ~puzzle_presenter() = default;

    [[nodiscard]] virtual auto supports_editing() const -> bool
    {
        return true;
    }

    [[nodiscard]] virtual auto editing() const -> bool = 0;
    virtual auto toggle_editing() -> void = 0;
    virtual auto reset() -> void = 0;

    [[nodiscard]] virtual auto board_columns(const state_model& model, state_id id) const
        -> int = 0;
    [[nodiscard]] virtual auto board_rows(const state_model& model, state_id id) const -> int = 0;

    virtual auto draw_board(const puzzle_canvas& canvas, state_manager& state) const -> void = 0;
    virtual auto draw_puzzle_controls(puzzle_control_context& context, int row, Color color,
                                      state_manager& state, input_handler& input) -> int = 0;
    virtual auto draw_edit_controls(puzzle_control_context& context, int row, Color color,
                                    state_manager& state) -> int = 0;

    [[nodiscard]] virtual auto supports_puzzle_space() const -> bool
    {
        return false;
    }
    virtual auto draw_puzzle_space_controls(puzzle_control_context& context, int row, Color color,
                                            state_manager& state, input_handler& input) -> int
    {
        (void)context;
        (void)color;
        (void)state;
        (void)input;
        return row;
    }

    virtual auto on_board_press(MouseButton button, const puzzle_canvas& canvas,
                                state_manager& state) -> void = 0;
    virtual auto on_board_release(MouseButton button, const puzzle_canvas& canvas,
                                  state_manager& state) -> void
    {
        (void)button;
        (void)canvas;
        (void)state;
    }
    virtual auto on_key(KeyboardKey key, bool ctrl, bool shift, state_manager& state) -> bool = 0;

    [[nodiscard]] virtual auto status_text(const state_model& model, state_id id) const
        -> std::string = 0;
    [[nodiscard]] virtual auto footer_text(const state_model& model, state_id id) const
        -> std::string = 0;
};

// Shared graph-navigation buttons used by puzzle presenters. Returns the next free row.
auto draw_graph_navigation(puzzle_control_context& context, int row, Color color,
                           state_manager& state, input_handler& input) -> int;

#endif
