#ifndef GUI_HPP_
#define GUI_HPP_

#include "orbit_camera.hpp"
#include "config.hpp"
#include "input_handler.hpp"
#include "state_manager.hpp"

#include <raylib.h>

class user_interface
{
    class grid
    {
    public:
        int x;
        int y;
        int width;
        int height;
        int columns;
        int rows;
        const int padding;

    public:
        grid(const int _x, const int _y, const int _width, const int _height, const int _columns,
             const int _rows, const int _padding)
            : x(_x), y(_y), width(_width), height(_height), columns(_columns), rows(_rows),
              padding(_padding)
        {}

    public:
        auto update_bounds(int _x, int _y, int _width, int _height, int _columns, int _rows)
            -> void;
        auto update_bounds(int _x, int _y, int _width, int _height) -> void;
        auto update_bounds(int _x, int _y) -> void;

        [[nodiscard]] auto bounds() const -> Rectangle;
        [[nodiscard]] auto bounds(int _x, int _y, int _width, int _height) const -> Rectangle;

        [[nodiscard]] auto square_bounds() const -> Rectangle;
        [[nodiscard]] auto square_bounds(int _x, int _y, int _width, int _height) const
            -> Rectangle;
    };

    struct style
    {
        int border_color_normal;
        int base_color_normal;
        int text_color_normal;

        int border_color_focused;
        int base_color_focused;
        int text_color_focused;

        int border_color_pressed;
        int base_color_pressed;
        int text_color_pressed;

        int border_color_disabled;
        int base_color_disabled;
        int text_color_disabled;
    };

    struct default_style : style
    {
        int background_color;
        int line_color;

        int text_size;
        int text_spacing;
        int text_line_spacing;
        int text_alignment_vertical;
        int text_wrap_mode;
    };

    struct component_style : style
    {
        int border_width;
        int text_padding;
        int text_alignment;
    };

private:
    input_handler& input;
    state_manager& state;
    const orbit_camera& camera;

    grid menu_grid = grid(0, 0, GetScreenWidth(), MENU_HEIGHT, MENU_COLS, MENU_ROWS, MENU_PAD);

    grid board_grid =
        grid(0, MENU_HEIGHT, GetScreenWidth() / 2, GetScreenHeight() - MENU_HEIGHT,
             state.get_current_state().width, state.get_current_state().height, BOARD_PADDING);

    grid graph_overlay_grid = grid(GetScreenWidth() / 2, MENU_HEIGHT, 200, 100, 1, 4, MENU_PAD);

    grid debug_overlay_grid =
        grid(GetScreenWidth() / 2, GetScreenHeight() - 75, 200, 75, 1, 3, MENU_PAD);

    // Windows

    std::string message_title;
    std::string message_message;
    std::function<void(void)> yes_no_handler;
    bool ok_message = false;
    bool yes_no_message = false;
    bool save_window = false;
    std::array<char, 256> preset_name = {};
    bool help_window = false;

public:
    user_interface(input_handler& _input, state_manager& _state, const orbit_camera& _camera)
        : input(_input), state(_state), camera(_camera)
    {
        init();
    }

    user_interface(const user_interface& copy) = delete;
    auto operator=(const user_interface& copy) -> user_interface& = delete;
    user_interface(user_interface&& move) = delete;
    auto operator=(user_interface&& move) -> user_interface& = delete;

private:
    static auto init() -> void;

    static auto apply_color(style& style, Color color) -> void;
    static auto apply_block_color(style& style, Color color) -> void;
    static auto apply_text_color(style& style, Color color) -> void;

    static auto get_default_style() -> default_style;
    static auto set_default_style(const default_style& style) -> void;
    static auto get_component_style(int component) -> component_style;
    static auto set_component_style(int component, const component_style& style) -> void;

    [[nodiscard]] static auto popup_bounds() -> Rectangle;

    auto draw_button(Rectangle bounds, const std::string& label, Color color, bool enabled = true,
                     int font_size = FONT_SIZE) const -> int;

    auto draw_menu_button(int x, int y, int width, int height, const std::string& label,
                          Color color, bool enabled = true, int font_size = FONT_SIZE) const -> int;

    auto draw_toggle_slider(Rectangle bounds, const std::string& off_label,
                            const std::string& on_label, int* active, Color color,
                            bool enabled = true, int font_size = FONT_SIZE) const -> int;

    auto draw_menu_toggle_slider(int x, int y, int width, int height, const std::string& off_label,
                                 const std::string& on_label, int* active, Color color,
                                 bool enabled = true, int font_size = FONT_SIZE) const -> int;

    auto draw_spinner(Rectangle bounds, const std::string& label, int* value, int min, int max,
                      Color color, bool enabled = true, int font_size = FONT_SIZE) const -> int;

    auto draw_menu_spinner(int x, int y, int width, int height, const std::string& label,
                           int* value, int min, int max, Color color, bool enabled = true,
                           int font_size = FONT_SIZE) const -> int;

    auto draw_label(Rectangle bounds, const std::string& text, Color color, bool enabled = true,
                    int font_size = FONT_SIZE) const -> int;

    auto draw_board_block(int x, int y, int width, int height, Color color,
                          bool enabled = true) const -> bool;

    [[nodiscard]] auto window_open() const -> bool;

    // Different menu sections
    auto draw_menu_header(Color color) const -> void;
    auto draw_graph_info(Color color) const -> void;
    auto draw_graph_controls(Color color) const -> void;
    auto draw_camera_controls(Color color) const -> void;
    auto draw_puzzle_controls(Color color) const -> void;
    auto draw_edit_controls(Color color) const -> void;
    auto draw_menu_footer(Color color) -> void;

public:
    static auto get_background_color() -> Color;
    auto help_popup() -> void;
    auto draw_save_preset_popup() -> void;
    auto draw_ok_message_box() -> void;
    auto draw_yes_no_message_box() -> void;
    auto draw_main_menu() -> void;
    auto draw_puzzle_board() -> void;
    auto draw_graph_overlay(int fps, int ups, size_t mass_count, size_t spring_count) -> void;
    auto draw(int fps, int ups, size_t mass_count, size_t spring_count) -> void;
};

#endif