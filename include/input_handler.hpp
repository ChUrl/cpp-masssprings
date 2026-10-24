#ifndef INPUT_HANDLER_HPP_
#define INPUT_HANDLER_HPP_

#include "orbit_camera.hpp"
#include "state_manager.hpp"

#include <cstdint>
#include <functional>
#include <queue>
#include <raylib.h>
#include <raymath.h>

struct show_ok_message
{
    std::string title;
    std::string message;
};

struct show_yes_no_message
{
    std::string title;
    std::string message;
    std::function<void()> on_yes;
};

struct show_save_preset_window
{};

struct show_puzzle_space_file_window
{
    bool save = true;
};

struct toggle_help_window
{};

using ui_command = std::variant<show_ok_message, show_yes_no_message, show_save_preset_window,
                                show_puzzle_space_file_window, toggle_help_window>;

class input_handler
{
    struct generic_handler
    {
        std::function<void(input_handler&)> handler;
    };

    struct mouse_handler : generic_handler
    {
        MouseButton button;
    };

    struct keyboard_handler : generic_handler
    {
        KeyboardKey key;
    };

private:
    std::vector<generic_handler> generic_handlers;
    std::vector<mouse_handler> mouse_pressed_handlers;
    std::vector<mouse_handler> mouse_released_handlers;
    std::vector<keyboard_handler> key_pressed_handlers;
    std::vector<keyboard_handler> key_released_handlers;

public:
    state_manager& state;
    orbit_camera& camera;

    std::queue<ui_command> ui_commands;

    bool disable = false;
    bool text_input_open = false;

    // Graph display
    bool mark_path = false;
    bool mark_solutions = false;
    bool connect_solutions = false;
    bool color_by_distance = false;
    bool settings_open = false; // settings pane replaces the controls pane

    // Camera
    bool camera_lock = true;
    bool camera_mass_center_lock = true;
    bool camera_panning = false;
    bool camera_rotating = false;

    // Mouse dragging
    Vector2 mouse = Vector2Zero();
    Vector2 last_mouse = Vector2Zero();

    // State selection from graph
    size_t collision_mass = -1;

    // Detects puzzle switches so presenter-local state can be reset.
    std::uint64_t last_puzzle_generation = 0;

public:
    input_handler(state_manager& _state, orbit_camera& _camera) : state(_state), camera(_camera)
    {
        init_handlers();
    }

    NO_COPY_NO_MOVE(input_handler);

private:
    auto init_handlers() -> void;

public:
    // Helpers
    [[nodiscard]] auto mouse_in_menu_pane() const -> bool;
    [[nodiscard]] auto mouse_in_board_pane() const -> bool;
    [[nodiscard]] auto mouse_in_graph_pane() const -> bool;

    // Mouse actions
    auto mouse_hover() -> void;
    auto camera_start_pan() -> void;
    auto camera_pan() const -> void;
    auto camera_stop_pan() -> void;
    auto camera_start_rotate() -> void;
    auto camera_rotate() const -> void;
    auto camera_stop_rotate() -> void;
    auto camera_zoom() const -> void;
    auto camera_fov() const -> void;
    auto select_state() const -> void;

    // Key actions
    auto toggle_settings() -> void;
    auto toggle_camera_lock() -> void;
    auto toggle_camera_mass_center_lock() -> void;
    auto toggle_camera_projection() const -> void;
    auto print_state() const -> void;
    auto load_previous_preset() -> void;
    auto load_next_preset() -> void;
    auto goto_starting_state() -> void;
    auto populate_graph() const -> void;
    auto clear_graph() -> void;
    auto toggle_mark_solutions() -> void;
    auto toggle_connect_solutions() -> void;
    auto toggle_color_by_distance() -> void;
    auto toggle_mark_path() -> void;
    auto goto_optimal_next_state() const -> void;
    auto goto_most_distant_state() const -> void;
    auto goto_closest_target_state() const -> void;
    auto goto_previous_state() const -> void;
    auto toggle_editing() -> void;
    auto save_preset() -> void;

    // General
    auto register_generic_handler(const std::function<void(input_handler&)>& handler) -> void;

    auto register_mouse_pressed_handler(MouseButton button,
                                        const std::function<void(input_handler&)>& handler) -> void;

    auto register_mouse_released_handler(MouseButton button,
                                         const std::function<void(input_handler&)>& handler)
        -> void;

    auto register_key_pressed_handler(KeyboardKey key,
                                      const std::function<void(input_handler&)>& handler) -> void;

    auto register_key_released_handler(KeyboardKey key,
                                       const std::function<void(input_handler&)>& handler) -> void;

    auto handle_input() -> void;
};

#endif
