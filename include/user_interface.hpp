#ifndef USER_INTERFACE_HPP_
#define USER_INTERFACE_HPP_

#include "config.hpp"
#include "input_handler.hpp"
#include "layout.hpp"
#include "orbit_camera.hpp"
#include "puzzle_presenter.hpp"
#include "state_manager.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <raylib.h>
#include <vector>

class user_interface : public puzzle_control_context
{
public:
    enum class settings_section : int
    {
        barnes_hut,
        physics,
        links,
        camera,
        rendering,
        performance,
    };

    enum class help_section : int
    {
        hotkeys,
        state_space,
        puzzle_space,
    };

private:
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
    orbit_camera& camera;
    settings_section settings_tab = settings_section::barnes_hut;
    float settings_scroll = 0.0f;
    help_section help_tab = help_section::hotkeys;
    float help_scroll = 0.0f;
    float controls_scroll = 0.0f;
    int controls_font_size = FONT_SIZE;
    size_t renderer_cpu_bytes = 0;
    size_t renderer_gpu_bytes = 0;

    // Widgets can be drawn outside the scrolled pane bounds while their raygui hit-boxes remain
    // at the original screen position. Suppress those interactions; otherwise clicks on the
    // board can activate scrolled-away controls underneath it.
    std::optional<Rectangle> active_input_bounds;

    [[nodiscard]] auto pointer_in_active_bounds() const -> bool;

    // Rolling per-frame timing samples for the performance graph (last 10 seconds).
    struct performance_sample
    {
        double time;
        float physics_ms;
        float render_ms;
    };
    static constexpr size_t performance_capacity = 4096;
    std::array<performance_sample, performance_capacity> performance_samples{};
    size_t performance_head = 0;
    size_t performance_count = 0;

    grid menu_grid =
        grid(0, 0, GetScreenWidth(), COMPACT_MENU_HEIGHT, MENU_COLS, COMPACT_MENU_ROWS, MENU_PAD);
    // Menu helpers draw into whichever grid is currently active (compact bar or controls panel).
    mutable const grid* active_grid = &menu_grid;

    grid graph_overlay_grid = grid(right_pane_x(), COMPACT_MENU_HEIGHT, 200, 100, 1, 4, MENU_PAD);

    grid debug_overlay_grid = grid(right_pane_x(), GetScreenHeight() - 75, 200, 75, 1, 3, MENU_PAD);

    // Windows

    std::string message_title;
    std::string message_message;
    std::string yes_no_yes_label = "Yes";
    std::string yes_no_no_label = "No";
    std::function<void()> yes_no_handler;
    std::function<void()> yes_no_handler_no;
    bool ok_message = false;
    bool yes_no_message = false;
    bool save_window = false;
    std::array<char, 256> preset_comment = {};
    std::array<char, 256> puzzle_space_filename = {};
    bool puzzle_space_file_window = false;
    bool puzzle_space_file_save = true;
    bool help_window = false;

public:
    user_interface(input_handler& _input, state_manager& _state, orbit_camera& _camera)
        : input(_input), state(_state), camera(_camera)
    {
        TextCopy(puzzle_space_filename.data(), "puzzle_space.puzzles");
        init();
    }

    NO_COPY_NO_MOVE(user_interface);

private:
    static auto init() -> void;

    static auto apply_color(style& style, Color color) -> void;
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

    auto draw_slider(Rectangle bounds, const std::string& label, int value, int min, int max,
                     Color color, bool enabled = true, int font_size = FONT_SIZE) const -> int;

    auto draw_menu_slider(int x, int y, int width, int height, const std::string& label, int value,
                          int min, int max, Color color, bool enabled = true,
                          int font_size = FONT_SIZE) const -> int;

    auto draw_menu_enum_spinner(int x, int y, int width, int height, const std::string& label,
                                const std::vector<std::string>& values, int* index, Color color,
                                bool enabled = true) const -> int;

    auto draw_progress(Rectangle bounds, const std::string& label, float fraction, Color color,
                       bool indeterminate = false) const -> void;

    auto draw_overlay_panel(Rectangle bounds) const -> void;
    auto draw_overlay_text(const std::string& text, int x, int y, Color color,
                           int font_size = OVERLAY_FONT_SIZE) const -> void;

    auto draw_label(Rectangle bounds, const std::string& text, Color color, bool enabled = true,
                    int font_size = FONT_SIZE) const -> int;

    [[nodiscard]] auto window_open() const -> bool;

    // puzzle_control_context
    auto button(int x, int y, int width, int height, const std::string& label, Color color,
                bool enabled = true) -> bool override;
    auto toggle(int x, int y, int width, int height, const std::string& off_label,
                const std::string& on_label, bool value, Color color, bool enabled = true)
        -> bool override;
    auto checkbox(int x, int y, int width, int height, const std::string& label, bool value,
                  Color color, bool enabled = true) -> bool override;
    auto spinner(int x, int y, int width, int height, const std::string& label, int value, int min,
                 int max, Color color, bool enabled = true) -> int override;
    auto slider(int x, int y, int width, int height, const std::string& label, int value, int min,
                int max, Color color, bool enabled = true) -> int override;
    auto progress(int x, int y, int width, int height, const std::string& label, float fraction,
                  Color color, bool indeterminate = false) -> void override;
    auto label(int x, int y, int width, int height, const std::string& text, Color color,
               bool enabled = true) -> void override;

    // Different menu sections
    auto draw_menu_header(Color color) const -> void;
    auto draw_graph_info(Color color, int row) const -> int;
    auto draw_graph_controls(Color color, int row) const -> int;
    auto draw_camera_controls(Color color, int row) const -> int;

public:
    static auto get_background_color() -> Color;
    auto help_popup() -> void;
    auto draw_save_preset_popup() -> void;
    auto draw_puzzle_space_file_popup() -> void;
    auto draw_ok_message_box() -> void;
    auto draw_yes_no_message_box() -> void;
    auto draw_main_menu() -> void;
    auto draw_controls_pane() -> void;
    auto draw_performance_graph(int x, int y, int width, int height) const -> void;
    auto draw_settings_pane() -> void;
    auto draw_board() -> void;
    auto draw_graph_overlay(int fps, int ups, size_t mass_count, size_t spring_count) -> void;
    auto draw_performance_overlay() -> void;
    auto draw_memory_overlay() -> void;
    auto set_renderer_memory(size_t cpu_bytes, size_t gpu_bytes) -> void;
    auto add_performance_sample(double physics_ms, double render_ms) -> void;
    auto draw(int fps, int ups, size_t mass_count, size_t spring_count) -> void;
};

#endif
